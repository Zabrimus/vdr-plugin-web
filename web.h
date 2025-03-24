/*
 * web.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#pragma once

#include <string>
#include <shared_mutex>
#include <memory>
#include <vdr/plugin.h>
#include "service/web_service.h"
#include "thrift-services/src-client/BrowserClient.h"

#include "VdrPluginWeb.h"
#include <thrift/transport/TSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;
using namespace ::pluginweb;
using namespace ::common;

static const char *VERSION = "0.0.1";
static const char *DESCRIPTION = "Uses the cefbrowser to show HTTP Pages, HbbTV applications and videos";
static const char *MAINMENUENTRY = "Web";
static char* MAINMENUENTRYALT = nullptr;

extern BrowserClient* browserClient;

class cRegularWorker: public cThread {
    private:
        cMutex m_mutex;
        cCondVar m_waitCondition;

    public:
        cRegularWorker();
        ~cRegularWorker();

        void Stop();
        void Action();
};

class cPluginWeb : public cPlugin {
private:
    std::unique_ptr<cRegularWorker> regularWorker;

public:
    cPluginWeb();
    ~cPluginWeb() override;

    const char *Version() override { return VERSION; }
    const char *Description() override { return DESCRIPTION; }
    const char *CommandLineHelp() override;
    bool ProcessArgs(int argc, char *argv[]) override;

    bool Initialize() override;
    bool Start() override;
    void Stop() override;

    void Housekeeping() override;
    void MainThreadHook() override;

    cString Active() override;
    time_t WakeupTime() override;

    const char *MainMenuEntry() override { return MAINMENUENTRYALT != nullptr ? MAINMENUENTRYALT : MAINMENUENTRY; }
    cOsdObject *MainMenuAction() override;
    cMenuSetupPage *SetupMenu() override;

    bool SetupParse(const char *Name, const char *Value) override;
    bool Service(const char *Id, void *Data) override;
    const char **SVDRPHelpPages() override;
    cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) override;

    bool readConfiguration(const char* configFile);
};

class VdrPluginWebServer : public VdrPluginWebIf {

public:
    ~VdrPluginWebServer() override;

    void ping() override;
    bool ProcessOsdUpdate(const ProcessOsdUpdateType& input) override;
    bool ProcessOsdUpdateQOI(const ProcessOsdUpdateQOIType& input) override;
    bool ProcessTSPacket(const ProcessTSPacketType& input) override;
    bool StartVideo(const StartVideoType& input) override;
    bool StopVideo() override;
    bool PauseVideo() override;
    bool ResumeVideo() override;
    bool Seeked() override;
    bool VideoSize(const VideoSizeType& input) override;
    bool VideoFullscreen() override;
    bool ResetVideo(const ResetVideoType& input) override;
    bool SelectAudioTrack(const SelectAudioTrackType& input) override;
};

class VdrPluginWebCloneFactory : virtual public VdrPluginWebIfFactory {
public:
    ~VdrPluginWebCloneFactory() override = default;

    VdrPluginWebIf* getHandler(const TConnectionInfo& connInfo) override;
    void releaseHandler(CommonServiceIf* handler) override;
};
