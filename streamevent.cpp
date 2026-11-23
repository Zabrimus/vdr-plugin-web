/*
 * streamevent.cpp: DSM-CC stream events (do-it-now) for HbbTV applications
 *
 * HbbTV applications register for stream events (video/broadcast addStreamEventListener).
 * The browser resolves the event names (stream event description file) and requests the
 * events of a component tag. The PID of the component comes from the PMT, the events are
 * stream event descriptors in DSM-CC sections (table id 0x3D) on this PID.
 *
 * See the README file for copyright information and how to reach the author.
 */

#include <map>
#include <set>
#include <deque>
#include <string>
#include <vdr/filter.h>
#include <vdr/thread.h>
#include <libsi/util.h>
#include "streamevent.h"
#include "web.h"

#define TID_DSMCC_STREAM_DESCRIPTORS  0x3D
#define TAG_STREAM_EVENT_DESCRIPTOR   0x1A

struct tStreamEvent {
    int componentTag;
    int eventId;
    std::string privateData;
};

// --- sender: the thrift call must not block the section handler -----------------

class cStreamEventSender : public cThread {
private:
    cMutex mutex;
    cCondVar condition;
    std::deque<tStreamEvent> queue;

protected:
    void Action() override {
        while (Running()) {
            tStreamEvent event;
            {
                cMutexLock lock(&mutex);
                if (queue.empty()) {
                    condition.TimedWait(mutex, 1000);
                    continue;
                }
                event = queue.front();
                queue.pop_front();
            }

            if (browserClient != nullptr) {
                browserClient->StreamEvent(event.componentTag, event.eventId, event.privateData, "trigger");
            }
        }
    }

public:
    cStreamEventSender() : cThread("vdrweb stream events") {}

    void Send(const tStreamEvent &event) {
        cMutexLock lock(&mutex);
        if (queue.size() < 100) {
            queue.push_back(event);
        }
        condition.Broadcast();
    }

    void Stop() {
        Cancel(-1);
        condition.Broadcast();
        Cancel(3);
    }
};

static cStreamEventSender *sender = nullptr;

// --- filter -------------------------------------------------------------------------

class cStreamEventFilter : public cFilter {
private:
    cMutex mutex;
    std::map<int, int> componentPids;   // component tag -> PID
    std::set<int> wantedTags;           // requested by the browser
    std::map<int, int> activePids;      // filtered PID -> component tag
    std::map<int, int> versions;        // PID << 16 | table id extension -> version
    bool changed = false;

    void Apply();
    void Parse(int pid, int componentTag, const u_char *Data, int Length);

protected:
    void Process(u_short Pid, u_char Tid, const u_char *Data, int Length) override;

public:
    cStreamEventFilter() {
        // the PAT arrives regularly, the requested changes are applied in Process()
        Set(0x00, SI::TableIdPAT);
    }

    void SetComponentPid(int componentTag, int pid) {
        cMutexLock lock(&mutex);
        if (componentPids[componentTag] != pid) {
            componentPids[componentTag] = pid;
            changed = true;
        }
    }

    bool Request(int componentTag, bool enable) {
        cMutexLock lock(&mutex);
        if (enable) {
            wantedTags.insert(componentTag);
        } else {
            wantedTags.erase(componentTag);
        }
        changed = true;
        // the PID may become known later (PMT)
        return true;
    }
};

// Add() and Del() are called in the thread of the section handler only
void cStreamEventFilter::Apply() {
    std::map<int, int> wanted;
    for (int tag : wantedTags) {
        auto it = componentPids.find(tag);
        if (it != componentPids.end()) {
            wanted[it->second] = tag;
        }
    }

    for (auto it = activePids.begin(); it != activePids.end();) {
        if (wanted.count(it->first) == 0) {
            Del(it->first, TID_DSMCC_STREAM_DESCRIPTORS);
            it = activePids.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto &w : wanted) {
        if (activePids.count(w.first) == 0) {
            dsyslog("[vdrweb] stream events: filter component tag %d, pid %d", w.second, w.first);
            Add(w.first, TID_DSMCC_STREAM_DESCRIPTORS);
            // report the current events of a newly requested component again
            for (auto v = versions.begin(); v != versions.end();) {
                v = ((v->first >> 16) == w.first) ? versions.erase(v) : std::next(v);
            }
        }
        activePids[w.first] = w.second;
    }
}

void cStreamEventFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length) {
    cMutexLock lock(&mutex);

    if (changed) {
        changed = false;
        Apply();
    }

    if (Tid != TID_DSMCC_STREAM_DESCRIPTORS) {
        return;
    }

    auto active = activePids.find(Pid);
    if (active != activePids.end()) {
        Parse(Pid, active->second, Data, Length);
    }
}

void cStreamEventFilter::Parse(int pid, int componentTag, const u_char *Data, int Length) {
    if (Length < 12) {
        return;
    }

    int sectionLength = ((Data[1] & 0x0F) << 8) | Data[2];
    bool syntaxIndicator = (Data[1] & 0x80) != 0;
    if (sectionLength + 3 > Length) {
        return;
    }

    if (syntaxIndicator && !SI::CRC32::isValid((const char *) Data, sectionLength + 3)) {
        return;
    }

    int tableIdExtension = (Data[3] << 8) | Data[4];
    int version = (Data[5] >> 1) & 0x1F;

    // the sections are repeated, an event is triggered once per version
    int key = (pid << 16) | tableIdExtension;

    auto v = versions.find(key);
    if (v != versions.end() && v->second == version) {
        return;
    }
    versions[key] = version;

    // descriptors between the header (8 bytes) and the CRC (4 bytes)
    const u_char *p = Data + 8;
    const u_char *end = Data + 3 + sectionLength - 4;

    while (p + 2 <= end) {
        int tag = p[0];
        int length = p[1];
        if (p + 2 + length > end) {
            break;
        }

        if (tag == TAG_STREAM_EVENT_DESCRIPTOR && length >= 10) {
            tStreamEvent event;
            event.componentTag = componentTag;
            event.eventId = (p[2] << 8) | p[3];
            // p[4..11]: reserved and event NPT, HbbTV uses do-it-now events only
            event.privateData.assign((const char *) p + 12, length - 10);

            dsyslog("[vdrweb] stream event: component tag %d, event id %d, %d bytes", componentTag, event.eventId, length - 10);

            if (sender != nullptr) {
                sender->Send(event);
            }
        }

        p += 2 + length;
    }
}

// --- interface -------------------------------------------------------------------------

static cMutex filterMutex;
static cStreamEventFilter *filter = nullptr;
static cDevice *filterDevice = nullptr;

void StreamEventsChannelSwitch(cDevice *device) {
    cMutexLock lock(&filterMutex);

    if (sender == nullptr) {
        sender = new cStreamEventSender();
        sender->Start();
    }

    if (filter != nullptr && filterDevice != nullptr) {
        filterDevice->Detach(filter);
    }
    delete filter;
    filter = nullptr;
    filterDevice = nullptr;

    if (device != nullptr) {
        filter = new cStreamEventFilter();
        filterDevice = device;
        device->AttachFilter(filter);
    }
}

void StreamEventsComponent(int componentTag, int pid) {
    cMutexLock lock(&filterMutex);
    if (filter != nullptr) {
        filter->SetComponentPid(componentTag, pid);
    }
}

bool StreamEventsRequest(int componentTag, bool enable) {
    cMutexLock lock(&filterMutex);
    return filter != nullptr && filter->Request(componentTag, enable);
}

void StreamEventsShutdown() {
    StreamEventsChannelSwitch(nullptr);

    cMutexLock lock(&filterMutex);
    if (sender != nullptr) {
        sender->Stop();
        delete sender;
        sender = nullptr;
    }
}
