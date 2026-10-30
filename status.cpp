/*
 * status.c: keep track of VDR device status
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <vdr/channels.h>
#include <string>
#include <vector>
#include <algorithm>
#include "web.h"
#include "status.h"
#include "ait.h"
#include "BrowserClient.h"
#include "webosdpage.h"
#include "jsonescape.h"

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
        "idType": "%d",
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

inline const char* toChannelJson(const cChannel* channel) {
    // longName, Name => currentChannel
    // nid            => ??? (use 1 as default)
    // onid           => channel, Nid
    // sid            => channel, Sid
    // tsid           => channel, Tid
    // channelType    => HDTV 0x19, TV 0x01, Radio 0x02
    // idType         => ??? (use 15 as default)
    int channelType;

    if (strstr(channel->Name(), "HD") != nullptr) {
        channelType = 0x19;
    } else if (channel->Rid() > 0) {
        channelType = 0x02;
    } else {
        channelType = 0x01;
    }

    char *cmd;
    std::string name = jsonEscape(channel->Name());
    asprintf(&cmd, channelJson, *channel->GetChannelID().ToString(), channelType, 1, channel->Nid(), channel->Tid(), channel->Sid(), name.c_str(), name.c_str(), 15);

    // add the tracks of the channel
    std::string json = cmd;
    free(cmd);

    auto end = json.find_last_of('}');
    json = json.substr(0, end) + ",\n        \"components\": " + channelComponentsJson(channel) + "\n    }\n";

    return strdup(json.c_str());
}

cHbbtvDeviceStatus::cHbbtvDeviceStatus() {
   device = nullptr;
   aitFilter = nullptr;
   sid = -1;
}

cHbbtvDeviceStatus::~cHbbtvDeviceStatus() {
}

void cHbbtvDeviceStatus::ChannelSwitch(const cDevice * vdrDevice, int channelNumber, bool LiveView) {
   // Indicates a channel switch on the given DVB device.
   // If ChannelNumber is 0, this is before the channel is being switched,
   // otherwise ChannelNumber is the number of the channel that has been switched to.
   // LiveView tells whether this channel switch is for live viewing.
   if (LiveView) {
      if (device) {
         isyslog("[vdrweb] Detaching HbbTV ait filter from device %d", device->CardIndex()+1);
         device->Detach(aitFilter);
         device = nullptr;
         aitFilter = nullptr;
         sid = -1;
      }

      if (channelNumber) {
         device = cDevice::ActualDevice();

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

         const char* buffer = toChannelJson(channel);
         browserClient->InsertChannel(buffer);
         free((void *) buffer);

         device->AttachFilter(aitFilter = new cAitFilter(sid));
         isyslog("[vdrweb] Attached HbbTV ait filter to device %d, vdrDev=%d actDev=%d, Sid=0x%04x", device->CardIndex()+1, vdrDevice->CardIndex()+1,cDevice::ActualDevice()->CardIndex()+1, sid);
      }
   }
}

void cHbbtvDeviceStatus::SetVolume(int Volume, bool Absolute) {
    // instead using the parameter (relative volume change), use the absolute volume of the Device

   volume = Absolute ? Volume : volume + Volume;
   WebOSDPage* page = WebOSDPage::Get();
   if (page)
       page->DrawVolume(volume);
}
