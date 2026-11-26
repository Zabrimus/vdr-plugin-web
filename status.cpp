/*
 * status.c: keep track of VDR device status
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <vdr/channels.h>
#include <vdr/dvbdevice.h>
#include <vdr/thread.h>
#include <vdr/sources.h>
#include <string>
#include <vector>
#include <algorithm>
#include "web.h"
#include "status.h"
#include "ait.h"
#include "BrowserClient.h"
#include "webosdpage.h"
#include "jsonescape.h"
#include "hbbtvhint.h"
#include "streamevent.h"

const char *channelJson = R"(
    {
        "channelId": "%s",
        "channelType": %d,
        "ccid": "ccid://1.0",
        "nid": %d,
        "dsd": "",
        "onid": %d,
        "tsid": %d,
        "sid": %d,
        "name": "%s",
        "longName": "%s",
        "description": "OIPF (SD&amp;S) - TCServiceData doesn’t support yet!",
        "authorised": true,
        "genre": null,
        "hidden": false,
        "idType": %d,
        "channelMaxBitRate": 0,
        "manualBlock": false,
        "majorChannel": 1,
        "ipBroadcastID": "rtp://1.2.3.4/",
        "locked": false
    }
)";

// The audio and subtitle tracks of the channel, ordered by PID like the
// track list VDR uses for the track selection (VideoPlayer::SelectTrack).
static std::string channelComponentsJson(const cChannel* channel) {
    struct track {
        int pid;
        std::string json;
    };

    std::vector<track> audio;
    for (int i = 0; channel->Apid(i); ++i) {
        int type = channel->Atype(i);
        const char *encoding = (type == 0x0F || type == 0x11) ? "audio/mp4" : "audio/mpeg";
        audio.push_back({ channel->Apid(i), *cString::sprintf("{\"pid\":%d,\"language\":\"%s\",\"encoding\":\"%s\"}",
                                                           channel->Apid(i), jsonEscape(channel->Alang(i)).c_str(), encoding) });
    }
    for (int i = 0; channel->Dpid(i); ++i) {
        audio.push_back({ channel->Dpid(i), *cString::sprintf("{\"pid\":%d,\"language\":\"%s\",\"encoding\":\"audio/ac3\"}",
                                                           channel->Dpid(i), jsonEscape(channel->Dlang(i)).c_str()) });
    }

    std::vector<track> subtitles;
    for (int i = 0; channel->Spid(i); ++i) {
        // subtitling types 0x20-0x25: for the hard of hearing
        bool hearingImpaired = channel->SubtitlingType(i) >= 0x20 && channel->SubtitlingType(i) <= 0x25;
        subtitles.push_back({ channel->Spid(i), *cString::sprintf("{\"pid\":%d,\"language\":\"%s\",\"hearingImpaired\":%s}",
                                                               channel->Spid(i), jsonEscape(channel->Slang(i)).c_str(), hearingImpaired ? "true" : "false") });
    }

    auto byPid = [](const track& a, const track& b) { return a.pid < b.pid; };
    std::sort(audio.begin(), audio.end(), byPid);
    std::sort(subtitles.begin(), subtitles.end(), byPid);

    std::string result = "{\"audio\":[";
    for (size_t i = 0; i < audio.size(); ++i) {
        result += (i ? "," : "") + audio[i].json;
    }
    result += "],\"subtitle\":[";
    for (size_t i = 0; i < subtitles.size(); ++i) {
        result += (i ? "," : "") + subtitles[i].json;
    }
    result += "]}";

    return result;
}

static std::string programmeJson(const cEvent *event) {
    if (event == nullptr) {
        return "";
    }

    std::string ratings = "[]";
    if (event->ParentalRating() > 0) {
        // dvb-si scheme: the name is the minimum age, the value the rating of the descriptor
        ratings = *cString::sprintf("[{\"name\":\"%d\",\"scheme\":\"dvb-si\",\"value\":%d,\"region\":\"de\"}]",
                                    event->ParentalRating(), event->ParentalRating() - 3);
    }

    return *cString::sprintf("{\"programmeID\":\"%u\",\"name\":\"%s\",\"description\":\"%s\",\"longDescription\":\"%s\","
                             "\"startTime\":%ld,\"duration\":%d,\"parentalRatings\":%s}",
                             event->EventID(), jsonEscape(event->Title()).c_str(), jsonEscape(event->ShortText()).c_str(),
                             jsonEscape(event->Description()).c_str(), (long) event->StartTime(), event->Duration(), ratings.c_str());
}

// present and following programme of the channel (video/broadcast programmes)
static std::string channelProgrammesJson(const cChannel* channel, tEventID *presentId = nullptr) {
    std::string result = "[";

    LOCK_SCHEDULES_READ
    const cSchedule *schedule = Schedules->GetSchedule(channel);
    if (schedule != nullptr) {
        const cEvent *present = schedule->GetPresentEvent();
        const cEvent *following = schedule->GetFollowingEvent();

        if (presentId != nullptr) {
            *presentId = present ? present->EventID() : 0;
        }

        std::string p = programmeJson(present);
        std::string f = programmeJson(following);
        result += p;
        if (!p.empty() && !f.empty()) {
            result += ",";
        }
        result += f;
    }

    return result + "]";
}

// OIPF DAE 7.13.11.1: Channel.TYPE_TV, TYPE_RADIO, TYPE_OTHER
static int oipfChannelType(const cChannel* channel) {
    if (channel->Vpid() != 0) {
        return 0;
    }
    return channel->Apid(0) != 0 || channel->Dpid(0) != 0 ? 1 : 2;
}

// OIPF DAE 7.13.11.1: the ID_* constant of the delivery system. Applications check it,
// the ProSiebenSat.1 red button asserts that it is a number.
static int oipfIdType(const cChannel* channel) {
    int source = channel->Source();
    if (cSource::IsSat(source) || cSource::IsTerr(source)) {
        // System() is 0 for DVB-S/DVB-T and 1 for DVB-S2/DVB-T2 (DVB_SYSTEM_1/2 in dvbdevice.c)
        bool second = cDvbTransponderParameters(channel->Parameters()).System() != 0;
        if (cSource::IsSat(source)) {
            return second ? 15 : 11; // ID_DVB_S2, ID_DVB_S
        }
        return second ? 16 : 12;     // ID_DVB_T2, ID_DVB_T
    }
    if (cSource::IsCable(source)) {
        return 10;                   // ID_DVB_C
    }
    if (cSource::IsAtsc(source)) {
        return 30;                   // ID_ATSC_T
    }
    return 41;                       // ID_IPTV_URI
}

inline const char* toChannelJson(const cChannel* channel) {
    // longName, Name => currentChannel
    // nid            => ??? (use 1 as default)
    // onid           => channel, Nid
    // sid            => channel, Sid
    // tsid           => channel, Tid
    int channelType = oipfChannelType(channel);
    int idType = oipfIdType(channel);

    char *cmd;
    std::string name = jsonEscape(channel->Name());
    asprintf(&cmd, channelJson, *channel->GetChannelID().ToString(), channelType, 1, channel->Nid(), channel->Tid(), channel->Sid(), name.c_str(), name.c_str(), idType);

    // add the tracks of the channel
    std::string json = cmd;
    free(cmd);

    auto end = json.find_last_of('}');
    json = json.substr(0, end) + ",\n        \"components\": " + channelComponentsJson(channel) +
                                 ",\n        \"programmes\": " + channelProgrammesJson(channel) + "\n    }\n";

    return strdup(json.c_str());
}

// Attach() and Detach() wait for the section handlers, which may wait for the locks of VDR.
// So they must not run in the channel switch callback, which holds such locks.
class cHbbtvDeviceStatus::cSwitchWorker : public cThread {
private:
   cMutex mutex;
   cCondVar condition;
   bool pending = false;
   int channelNumber = 0;
   const cDevice *vdrDevice = nullptr;
   cHbbtvDeviceStatus *status;

protected:
   void Action() override {
      while (Running()) {
         int number;
         const cDevice *dev;
         {
            cMutexLock lock(&mutex);
            if (!pending) {
               condition.TimedWait(mutex, 1000);
               continue;
            }
            number = channelNumber;
            dev = vdrDevice;
            pending = false;
         }
         status->Switch(number, dev);
      }
   }

public:
   cSwitchWorker(cHbbtvDeviceStatus *Status) : cThread("vdrweb channel switch"), status(Status) {}

   // only the latest switch counts
   void Request(int ChannelNumber, const cDevice *VdrDevice) {
      cMutexLock lock(&mutex);
      channelNumber = ChannelNumber;
      vdrDevice = VdrDevice;
      pending = true;
      condition.Broadcast();
   }

   void Stop() {
      Cancel(-1);
      condition.Broadcast();
      Cancel(3);
   }
};

cHbbtvDeviceStatus::cHbbtvDeviceStatus() {
   device = nullptr;
   aitFilter = nullptr;
   sid = -1;
   switchWorker = new cSwitchWorker(this);
   switchWorker->Start();
}

cHbbtvDeviceStatus::~cHbbtvDeviceStatus() {
   switchWorker->Stop();
   delete switchWorker;

   // the filter sends the applications to the browser
   if (device) {
      device->Detach(aitFilter);
   }
   delete aitFilter;
}

void cHbbtvDeviceStatus::ChannelSwitch(const cDevice * vdrDevice, int channelNumber, bool LiveView) {
   // Indicates a channel switch on the given DVB device.
   // If ChannelNumber is 0, this is before the channel is being switched,
   // otherwise ChannelNumber is the number of the channel that has been switched to.
   // LiveView tells whether this channel switch is for live viewing.
   if (LiveView) {
      // a new channel needs its own HbbTV hint
      cHbbtvHint::ChannelSwitch();

      switchWorker->Request(channelNumber, vdrDevice);
   }
}

void cHbbtvDeviceStatus::Switch(int channelNumber, const cDevice *vdrDevice) {
   // stream events belong to the channel
   StreamEventsChannelSwitch(nullptr);

   if (device) {
      isyslog("[vdrweb] Detaching HbbTV ait filter from device %d", device->CardIndex()+1);
      device->Detach(aitFilter);
      delete aitFilter;
      device = nullptr;
      aitFilter = nullptr;
      sid = -1;
   }

   if (channelNumber) {
      device = cDevice::ActualDevice();

      std::string json;
      {
#if APIVERSNUM >= 20301
         LOCK_CHANNELS_READ
         auto channel = Channels->GetByNumber(channelNumber);
#else
         auto channel = Channels->GetByNumber(channelNumber);
#endif
         if (channel == nullptr) {
            device = nullptr;
            return;
         }

         sid = channel->Sid();
         lastPresentEventId = 0;

         const char* buffer = toChannelJson(channel);
         json = buffer;
         free((void *) buffer);
      }

      // not with the lock of the channels: the browser may answer slowly
      if (browserClient != nullptr) {
         browserClient->InsertChannel(json);
      }

      device->AttachFilter(aitFilter = new cAitFilter(sid));
      StreamEventsChannelSwitch(device);
      isyslog("[vdrweb] Attached HbbTV ait filter to device %d, vdrDev=%d actDev=%d, Sid=0x%04x", device->CardIndex()+1, vdrDevice->CardIndex()+1,cDevice::ActualDevice()->CardIndex()+1, sid.load());
   }
}

// The present programme changes, or the EPG arrives after the channel switch.
// Send the channel again, the browser updates the running application.
void cHbbtvDeviceStatus::CheckProgramme() {
    if (sid < 0 || browserClient == nullptr) {
        return;
    }

    std::string buffer;
    {
        LOCK_CHANNELS_READ
        const cChannel *channel = Channels->GetByNumber(cDevice::CurrentChannel());
        if (channel == nullptr) {
            return;
        }

        tEventID presentId = 0;
        channelProgrammesJson(channel, &presentId);

        if (presentId == lastPresentEventId) {
            return;
        }
        lastPresentEventId = presentId;

        const char *json = toChannelJson(channel);
        buffer = json;
        free((void *) json);
    }

    browserClient->InsertChannel(buffer);
}

void cHbbtvDeviceStatus::SetVolume(int Volume, bool Absolute) {
    // instead using the parameter (relative volume change), use the absolute volume of the Device

   volume = Absolute ? Volume : volume + Volume;

   // another thread may delete the page (releasePlayer)
   std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
   WebOSDPage* page = WebOSDPage::Get();
   if (page)
       page->DrawVolume(volume);
}
