/*
* ait.c: AIT section filter
*
* See the README file for copyright information and how to reach the author.
*
* $Id$
*/

#include <string>
#include <cstdio>
#include <vdr/tools.h>
#include <libsi/section.h>
#include <libsi/descriptor.h>
#include "ait.h"
#include "status.h"
#include "web.h"

#define PMT_SCAN_IDLE     5 // 60 //300    // seconds

// #define DEBUG

#ifdef DEBUG
#       define DSYSLOG(x...)    dsyslog(x);
#else
#       define DSYSLOG(x...)
#endif

const char *hbbtvJson = R"(
    {
        "channelId": "%s",
        "channelName": "%s",
        "applicationId": %d,
        "controlCode": %d,
        "name": "%s",
        "urlBase": "%s",
        "urlLoc": "%s",
        "urlExt": "%s"
    }
)";

inline const char* toHbbtvJson(const char* channelId, const char* channelName, int applicationId, int controlCode, const char* name, const char* urlBase, const char* urlLoc, const char* urlExt) {
    char *buffer;
    asprintf(&buffer, hbbtvJson, channelId, channelName, applicationId, controlCode, name, urlBase, urlLoc, urlExt);
    return buffer;
}

// --- cAIT ------------------------------------------------------------

class cAIT : public SI::AIT {
public:
    cAIT(const u_char *Data, u_short Pid);
};

cAIT::cAIT(const u_char *Data, u_short Pid) : SI::AIT(Data, true) {
    if (!CheckCRCAndParse())
        return;

    SI::AIT::Application aitApp;
    for (SI::Loop::Iterator it; applicationLoop.getNext(aitApp, it);) {
        DSYSLOG("[vdrweb] -----------SI::AIT::Application Pid=0x%04x ---------------\n", Pid);
        DSYSLOG("[vdrweb] SI::AIT::Application ApplicationId=0x%04x ControlCode=0x%04x", aitApp.getApplicationId(), aitApp.getControlCode());

        char nameBuffer[Utf8BufSize(256)];
        char URLBaseBuffer[Utf8BufSize(256)];
        char URLLocBuffer[Utf8BufSize(256)];
        char URLExtBuffer[Utf8BufSize(256)];
        nameBuffer[0] = '\0';
        URLBaseBuffer[0] = '\0';
        URLLocBuffer[0] = '\0';
        URLExtBuffer[0] = '\0';
        uchar ApplPriority = 0;

        SI::Descriptor *d;
        for (SI::Loop::Iterator it2; (d = aitApp.applicationDescriptors.getNext(it2));) {
            switch (d->getDescriptorTag()) {
                case SI::MHP_ApplicationDescriptorTag: {
                    //DSYSLOG("[vdrweb] SI::AIT::Application ApplicationId=0x%04x ControlCode=0x%04x Appl_Prority=%02x\n",aitApp.getApplicationId(), aitApp.getControlCode(), aitApp.getApplicationPriority());
                    SI::MHP_ApplicationDescriptor *app = (SI::MHP_ApplicationDescriptor *) d;

                    DSYSLOG("[vdrweb] SI::MHP_ApplicationDescriptor Visibility=%x Priority=%x\n",
                            app->getVisibility(), app->getApplicationPriority());
                    ApplPriority = app->getApplicationPriority();
                }
                    break;

                case SI::MHP_ApplicationNameDescriptorTag: {
                    SI::MHP_ApplicationNameDescriptor *de = (SI::MHP_ApplicationNameDescriptor *) d;
                    SI::MHP_ApplicationNameDescriptor::NameEntry nameEntry;

                    for (SI::Loop::Iterator it3; de->nameLoop.getNext(nameEntry, it3);) {
                        nameEntry.name.getText(nameBuffer, sizeof(nameBuffer));
                        DSYSLOG("[vdrweb] SI::MHP_ApplicationNameDescriptor Name=%s\n", nameBuffer);
                    }
                }
                    break;

                case SI::MHP_TransportProtocolDescriptorTag: {
                    SI::MHP_TransportProtocolDescriptor *tpd = (SI::MHP_TransportProtocolDescriptor *) d;
                    SI::MHP_TransportProtocolDescriptor::UrlExtensionEntry urlExtEntry;

                    DSYSLOG("[vdrweb] SI::MHP_TransportProtocolDescriptor ProtocolLabel=0x%04x ProtocolID=0x%04x\n",
                            tpd->getProtocolLabel(), tpd->getProtocolId());

                    switch (tpd->getProtocolId()) {
                        case SI::MHP_TransportProtocolDescriptor::ObjectCarousel:
                            DSYSLOG("[vdrweb] SI::MHP_TransportProtocolDescriptor ObjectCarousel isRemote=0x%04x ComponentTag=0x%04x\n",
                                    tpd->isRemote(), tpd->getComponentTag());
                            DSYSLOG("[vdrweb] SI::MHP_TransportProtocolDescriptor ObjectCarousel isRemote=0x%04x ComponentTag=0x%04x\n",
                                    tpd->isRemote(), tpd->getComponentTag());
                            break;

                        case SI::MHP_TransportProtocolDescriptor::HTTPoverInteractionChannel:
                            tpd->getUrlBase(URLBaseBuffer, sizeof(URLBaseBuffer));
                            DSYSLOG("[vdrweb] SI::MHP_TransportProtocolDescriptor URL base=\"%s\" \n", URLBaseBuffer);

                            for (SI::Loop::Iterator it3; tpd->UrlExtensionLoop.getNext(urlExtEntry, it3);) {
                                urlExtEntry.UrlExtension.getText(URLExtBuffer, sizeof(URLExtBuffer));
                                if (Utf8StrLen(URLExtBuffer))
                                    isyslog("[vdrweb] SI::MHP_ApplicationNameDescriptor Extension=\"%s\" \n",
                                            URLExtBuffer);
                            }
                            break;

                        default:
                            break;
                    }
                }
                    break;

                case SI::MHP_SimpleApplicationLocationDescriptorTag: {
                    SI::MHP_SimpleApplicationLocationDescriptor *sal = (SI::MHP_SimpleApplicationLocationDescriptor *) d;
                    sal->getLocation(URLLocBuffer, sizeof(URLLocBuffer));
                    DSYSLOG("[vdrweb] SI::MHP_SimpleApplicationLocationDescriptorTag=%s \n", URLLocBuffer);
                }
                    break;

                default:
                    break;
            }
            delete d;
        }

#if APIVERSNUM >= 20301
        LOCK_CHANNELS_READ
        const cChannel* currentChannel = Channels->GetByNumber(cDevice::CurrentChannel());
#else
        const cChannel* currentChannel = Channels->GetByNumber(cDevice::CurrentChannel());
#endif

        const char* buffer = toHbbtvJson(currentChannel->GetChannelID().ToString(), currentChannel->Name(), aitApp.getApplicationId(), aitApp.getControlCode(), nameBuffer, URLBaseBuffer, URLLocBuffer, URLExtBuffer);
        if (browserClient != nullptr) {
            browserClient->InsertHbbtv(buffer);
        }

        free((void *) buffer);
    }
}

// --- cAitFilter ------------------------------------------------------------

cAitFilter::cAitFilter(int Sid) {
    Trigger(Sid);
    Set(0x00, SI::TableIdPAT);
}

void cAitFilter::SetStatus(bool On) {
    cMutexLock MutexLock(&mutex);
    cFilter::SetStatus(On);
    Trigger();
}

void cAitFilter::Trigger(int Sid) {
    cMutexLock MutexLock(&mutex);
    patVersion = -1;
    pmtVersion = -1;
    pmtNextCheck = 0;
    if (Sid >= 0) {
        sid = Sid;
        pmtPid = -1;
    }
}

void cAitFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length) {
    cMutexLock MutexLock(&mutex);
    int now = time(NULL);

    if (Pid == 0x00 && Tid == SI::TableIdPAT) {
        if (!pmtNextCheck || now > pmtNextCheck) {
            SI::PAT pat(Data, false);
            if (!pat.CheckCRCAndParse())
                return;

            if (pat.getVersionNumber() != patVersion) {
                //DSYSLOG("[vdrweb] PAT %d %d -> %d", Transponder(), patVersion, pat.getVersionNumber());
                if (pmtPid >= 0) {
                    Del(pmtPid, SI::TableIdPMT);
                    pmtPid = -1;
                }

                SI::PAT::Association assoc;
                for (SI::Loop::Iterator it; pat.associationLoop.getNext(assoc, it);) {
                    if (!assoc.isNITPid()) {
                        if (sid == assoc.getServiceId()) {
                            pmtPid = assoc.getPid();
                            DSYSLOG("[vdrweb] found sid = 0x%04x pmtPid = 0x%04x", sid, pmtPid);
                            Add(pmtPid, SI::TableIdPMT);
                            pmtNextCheck = now + PMT_SCAN_IDLE;
                            break;
                        }
                    }
                }
                patVersion = pat.getVersionNumber();
            }
        }
    } else if (pmtPid == Pid && Tid == SI::TableIdPMT && Source() && Transponder()) {
        if (pmtVersion == -1 || !pmtNextCheck || now > pmtNextCheck) {
            SI::PMT pmt(Data, false);
            if (!pmt.CheckCRCAndParse())
                return;

            if (pmtVersion != -1) {
                if (pmtVersion != pmt.getVersionNumber()) {
                    Del(pmtPid, SI::TableIdPMT);
                    Trigger(-1); //something is wrong - trigger new PAT scan
                    return;
                }
            }
            pmtVersion = pmt.getVersionNumber();

            // Scan the stream-specific loop:
            SI::PMT::Stream stream;
            for (SI::Loop::Iterator it; pmt.streamLoop.getNext(stream, it);) {
                if (stream.getStreamType() == 0x05) {
                    SI::Descriptor *d;
                    for (SI::Loop::Iterator it2; (d = stream.streamDescriptors.getNext(it2,
                                                                                       SI::ApplicationSignallingDescriptorTag));) {
                        switch (d->getDescriptorTag()) {
                            case SI::ApplicationSignallingDescriptorTag: {
                                DSYSLOG("[vdrweb] -----------SI::ApplicationSignallingDescriptorTag Len %d Pid 0x%04x ---------------",
                                        d->getLength(), stream.getPid());
                                SI::ApplicationSignallingDescriptor *appSig = (SI::ApplicationSignallingDescriptor *) d;
                                SI::ApplicationSignallingDescriptor::ApplicationEntryDescriptor appEntry;

                                for (SI::Loop::Iterator it3; appSig->entryLoop.getNext(appEntry, it3);) {
                                    DSYSLOG("[vdrweb] asd ApplType=0x%x AITVersion 0x%x\n",
                                            appEntry.getApplicationType(), appEntry.getAITVersionNumber());
                                }

                                if (!Matches(stream.getPid(), SI::TableIdAIT)) {
                                    Add(stream.getPid(), SI::TableIdAIT);
                                    Del(Pid, SI::TableIdPMT);
                                    SetStatus(true);
                                    DSYSLOG("[vdrweb] SI::ApplicationSignallingDescriptorTag added Pid=0x%04x",
                                            stream.getPid());
                                }
                                break;
                            }
                            default:
                                break;

                        }
                        delete d;
                    }
                }
            }
        }
    } else if (Tid == SI::TableIdAIT && Source() && Transponder()) {
        DSYSLOG("[vdrweb] AIT Pid 0x%04x found \n", Pid);
        cAIT AIT(Data, Pid);
        Del(Pid, SI::TableIdAIT);
        pmtNextCheck = now + PMT_SCAN_IDLE;
    }
}
