/*
 * web.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <getopt.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/tools.h>
#include <vdr/videodir.h>
#include <Magick++.h>
#include <mutex>

#include <memory>
#include "web.h"
#include "BrowserClient.h"
#include "ini.h"
#include "webosdpage.h"
#include "status.h"
#include "videocontrol.h"
#include "dummyosd.h"

#define TSDIR  "%s/web/%s/%4d-%02d-%02d.%02d.%02d.%d-%d.rec"

TThreadPoolServer *thriftServer;

bool saveTS;
char* currentTSFilename;
char *currentTSDir;

std::string browserIp;
int browserPort;

std::string transcoderIp;
int transcoderPort;

std::string vdrIp;
int vdrPort;

bool bindAll;

cHbbtvDeviceStatus *hbbtvDeviceStatus;

VideoPlayer* videoPlayer = nullptr;

std::string videoInfo;

BrowserClient* browserClient;

enum OSD_COMMAND {
    // normal commands
    OPEN,
    REOPEN,
    CLOSE,

    // start specific applications
    MAIN,
    M3U,
    URL
};

OSD_COMMAND nextOsdCommand = OPEN;
bool useOutputDeviceScale = false;
bool useDummyOsd = false;

int nr = 0;

int lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight;

// parameters for calling application
std::string param_url;
std::string param_m3uContent;
std::string param_userAgent;
std::string param_referrer;
std::string param_cookie;

void createTSFileName() {
    if (currentTSFilename != nullptr) {
        delete currentTSFilename;
        currentTSFilename = nullptr;
    }

    if (currentTSDir != nullptr) {
        delete currentTSDir;
        currentTSDir = nullptr;
    }

    time_t now = time(nullptr);
    struct tm tm_r;
    struct tm *t = localtime_r(&now, &tm_r);

    currentTSDir = strdup(cString::sprintf(TSDIR, cVideoDirectory::Name(), *cString::sprintf("%lu", now), t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, 0, 0));
    currentTSFilename = strdup(cString::sprintf("%s/00001.ts", currentTSDir));
}

void stopVideo() {
    if (videoPlayer != nullptr) {
        VideoPlayer *copy = videoPlayer;
        videoPlayer = nullptr;
        delete copy;

        nextOsdCommand = REOPEN;
        cRemote::CallPlugin("web");
    }
}

inline OperationFailed createException(int code, std::string reason) {
    OperationFailed io;
    io.code = code;
    io.reason = reason;

    return io;
}

VdrPluginWebServer::~VdrPluginWebServer() {
}

void VdrPluginWebServer::ping() {
}

bool VdrPluginWebServer::ProcessOsdUpdate(const ProcessOsdUpdateType &input) {
    WebOSDPage* page = WebOSDPage::Get();
    if (page == nullptr) {
        // illegal request -> abort
        esyslog("[vdrweb] ProcessOsdUpdate: osd update request while webOsdPage is null.");
        throw createException(400, "webOsdPage is null");
    }

    if (!page->drawImage((uint8_t*)input.data.c_str(), input.disp_width, input.disp_height, input.x, input.y, input.width, input.height)) {
        throw createException(400, "ProcessOsdUpdate failed");
    }

    return true;
}

bool VdrPluginWebServer::ProcessOsdUpdateQOI(const ProcessOsdUpdateQOIType &input) {
    WebOSDPage* page = WebOSDPage::Get();
    if (page == nullptr) {
        // illegal request -> abort
        esyslog("[vdrweb] ProcessOsdUpdate: osd update request while webOsdPage is null.");
        throw createException(400, "webOsdPage is null");
    }

    if (!page->drawImageQOI(input.image_data, input.render_width, input.render_height, input.x, input.y)) {
        throw createException(400, "ProcessOsdUpdate failed");
    }

    return true;
}

bool VdrPluginWebServer::ProcessTSPacket(const ProcessTSPacketType &input) {
    if (saveTS) {
        FILE* f = fopen(currentTSFilename, "a");
        if (f != nullptr) {
            fwrite((uint8_t *)input.ts.c_str(), input.ts.length(), 1, f);
            fclose(f);
        }
    }

    if (videoPlayer != nullptr) {
        videoPlayer->PlayPacket((uint8_t *) input.ts.c_str(), (int) input.ts.length());

        /*
        if (videoPlayer && videoPlayer->hasTsError()) {
            // stop video streaming
            stopVideo();
            browserClient->StopVideo("VideoPlayer is null or ts stream has errors");
        }
        */
    } else {
        // No video Player? Stop streaming
        stopVideo();
    }

    return true;
}

bool VdrPluginWebServer::StartVideo(const StartVideoType &input) {
    dsyslog("[vdrweb] StartVideo received");

    // Close existing OSD
    nextOsdCommand = CLOSE;
    cRemote::CallPlugin("web");

    WebOSDPage* page = WebOSDPage::Create(useOutputDeviceScale, PLAYER);
    page->Display();

    if (saveTS) {
        // create directory if necessary
        createTSFileName();
        if (!MakeDirs(currentTSDir, true)) {
            esyslog("[vdrweb]: can't create directory %s", currentTSDir);
        }
    }

    videoPlayer = new VideoPlayer();
    page->SetPlayer(videoPlayer);

    cControl::Launch(page);
    cControl::Attach();

    return true;
}

bool VdrPluginWebServer::StopVideo() {
    dsyslog("[vdrweb] StopVideo received");

    stopVideo();
    return true;
}

bool VdrPluginWebServer::PauseVideo() {
    dsyslog("[vdrweb] PauseVideo received");

    if (videoPlayer != nullptr) {
        videoPlayer->Pause();
    }

    return true;
}

bool VdrPluginWebServer::ResumeVideo() {
    dsyslog("[vdrweb] ResumeVideo received");

    if (videoPlayer != nullptr) {
        videoPlayer->Resume();
    }

    return true;
}

bool VdrPluginWebServer::Seeked() {
    dsyslog("[vdrweb] Seeked received");

    if (videoPlayer != nullptr) {
        videoPlayer->ResetVideo();
    }

    return true;
}

bool VdrPluginWebServer::VideoSize(const VideoSizeType &input) {
    dsyslog("[vdrweb] Incoming request /VideoSize with x %d, y %d, width %d, height %d", input.x, input.y, input.w, input.h);

    lastVideoX = input.x;
    lastVideoY = input.y;
    lastVideoWidth = input.w;
    lastVideoHeight = input.h;

    VideoPlayer::SetVideoSize(lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight);

    return true;
}

bool VdrPluginWebServer::VideoFullscreen() {
    dsyslog("[vdrweb] VideoFullscreen received");

    lastVideoX = lastVideoY = lastVideoWidth = lastVideoHeight = 0;
    VideoPlayer::setVideoFullscreen();

    return true;
}

bool VdrPluginWebServer::ResetVideo(const ResetVideoType &input) {
    dsyslog("[vdrweb] ResetVideo received: Coords x=%d, y=%d, w=%d, h=%d", lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight);

    if (saveTS) {
        // create directory if necessary
        createTSFileName();
        if (!MakeDirs(currentTSDir, true)) {
            esyslog("[vdrweb]: can't create directory %s", currentTSDir);
        }
    }

    if (videoPlayer != nullptr) {
        // TODO: Compare saved videoInfo with new value to determine
        //   if DeviceClear is sufficient or a complete reset is necessary

        dsyslog("[vdrweb] video change from %s to %s", videoInfo.c_str(), input.videoInfo.c_str());

        if (videoInfo != input.videoInfo) {
            dsyslog("[vdrweb] Device res requested, because of a video format change");
            // videoPlayer->ResetVideo();
            cControl::Shutdown();

            WebOSDPage* page = WebOSDPage::Create(useOutputDeviceScale, PLAYER);
            page->Display();
            videoPlayer = new VideoPlayer();
            cControl::Launch(page);
            page->SetPlayer(videoPlayer);
        } else {
            dsyslog("[vdrweb] Device reset is sufficent, because video format does not change");
            videoPlayer->ResetVideo();
        }

        VideoPlayer::SetVideoSize(lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight);
    } else {
        // TODO: Is it necessary to create a new Player?
        esyslog("[vdrweb] ResetVideo called, but videoPlayer is null");
    }

    videoInfo = input.videoInfo;

    return true;
}

bool VdrPluginWebServer::SelectAudioTrack(const SelectAudioTrackType &input) {
    dsyslog("[vdrweb] SelectAudioTrack received");

    if (videoPlayer != nullptr) {
        auto track = input.audioTrack;
        videoPlayer->SelectAudioTrack(track);
    }

    return true;
}

bool VdrPluginWebServer::IsWebActive() {
    return (WebOSDPage::Get() != nullptr) || (videoPlayer != nullptr);
}

VdrPluginWebIf *VdrPluginWebCloneFactory::getHandler(const TConnectionInfo &connInfo) {
    std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);

    /*
    std::cout << "Incoming connection" << "\n";
    std::cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
    std::cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
    std::cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
    std::cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";
    */

    return new VdrPluginWebServer();
}

void VdrPluginWebCloneFactory::releaseHandler(CommonServiceIf* handler) {
    delete handler;
}

void startServer() {
    const int workerCount = 4;
    std::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(workerCount);
    threadManager->threadFactory(std::make_shared<ThreadFactory>());
    threadManager->start();

    std::string listenIp = bindAll ? "0.0.0.0" : vdrIp;

    thriftServer = new TThreadPoolServer(
            std::make_shared<VdrPluginWebProcessorFactory>(std::make_shared<VdrPluginWebCloneFactory>()),
            std::make_shared<TServerSocket>(listenIp, vdrPort),
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>(),
            threadManager);

    thriftServer->serve();
}

cRegularWorker::cRegularWorker() {
}

cRegularWorker::~cRegularWorker() {
}

void cRegularWorker::Stop() {
    m_waitCondition.Broadcast();  // wakeup the thread
    Cancel(10);                   // wait up to 10 seconds for thread was stopping
}

void cRegularWorker::Action() {
    m_mutex.Lock();
    int loopSleep = 500; // do this every 1/2 second

    while (Running()) {
        m_waitCondition.TimedWait(m_mutex, loopSleep);

        if (WebOSDPage::Get() == nullptr && videoPlayer != nullptr) {
            VideoPlayer *cp = videoPlayer;
            videoPlayer = nullptr;
            delete cp;
        }
    }
}

cPluginWeb::cPluginWeb() {
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
    currentTSFilename = nullptr;
    saveTS = false;
    bindAll = false;
}

cPluginWeb::~cPluginWeb() {
}

const char *cPluginWeb::CommandLineHelp() {
    return "  -c file,     --config=file configuration file sockets.ini\n"
           "  -f,          --fastscale   used opengl to scale images\n"
           "  -o,          --dummyosd    opens a dummy osd while playing videos\n"
           "  -s,          --savets      Saves all incoming ts streams\n"
           "  -b,          --bindall     bind the web server to all interfaces\n"
           "  -n,          --name        Menu entry name\n";
}

bool cPluginWeb::ProcessArgs(int argc, char *argv[]) {
    static struct option long_options[] = {
            { "config",      required_argument, nullptr, 'c' },
            { "fastscale",   optional_argument, nullptr, 'f' },
            { "dummyosd",    optional_argument, nullptr, 'o' },
            { "savets",      optional_argument, nullptr, 's' },
            { "bindall",     optional_argument, nullptr, 'b' },
            { "name",        optional_argument, nullptr, 'n' },
            { nullptr }
    };

    int c, option_index = 0;
    while ((c = getopt_long(argc, argv, "c:fosbn:", long_options, &option_index)) != -1)
    {
        switch (c)
        {
            case 'c':
                if (!readConfiguration(optarg)) {
                    exit(-1);
                }
                break;

            case 'f':
                useOutputDeviceScale = true;
                break;

            case 'o':
                useDummyOsd = true;
                break;

            case 's':
                saveTS = true;
                break;

            case 'b':
                bindAll = true;
                break;

            case 'n':
                MAINMENUENTRYALT = strdup(optarg);
                break;

            default:
                break;
        }
    }
    return true;
}

bool cPluginWeb::Initialize() {
    // Initialize any background activities the plugin shall perform.
    MagickLib::InitializeMagickEx(nullptr, MAGICK_OPT_NO_SIGNAL_HANDER, NULL);

    return true;
}

bool cPluginWeb::Start() {
    isyslog("[vdrweb] Start hbbtv url collector");
    hbbtvDeviceStatus = new cHbbtvDeviceStatus();

    isyslog("[vdrweb] Start Thrift Server on %s:%d", vdrIp.c_str(), vdrPort);
    std::thread t1(startServer);
    t1.detach();

    browserClient = new BrowserClient(browserIp, browserPort);

    regularWorker = std::make_unique<cRegularWorker>();
    regularWorker->Start();

    return true;
}

void cPluginWeb::Stop() {
    thriftServer->stop();

    delete browserClient;

    regularWorker->Stop();

    if (hbbtvDeviceStatus != nullptr) {
        delete hbbtvDeviceStatus;
    }

    if (currentTSDir != nullptr) {
        delete currentTSDir;
    }

    if (currentTSFilename != nullptr) {
        delete currentTSFilename;
    }
}

void cPluginWeb::Housekeeping() {
    // Perform any cleanup or other regular tasks.
}

void cPluginWeb::MainThreadHook() {
    // Perform actions in the context of the main program thread.
    // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginWeb::Active() {
    // Return a message string if shutdown should be postponed
    return nullptr;
}

time_t cPluginWeb::WakeupTime() {
    // Return custom wakeup time for shutdown script
    return 0;
}

cOsdObject *cPluginWeb::MainMenuAction() {
    dsyslog("[vdrweb] MainMenuAction: command = %d\n", (int)nextOsdCommand);

    if (nextOsdCommand == CLOSE) {
        nextOsdCommand = OPEN;

        if (useDummyOsd) {
            // instead of really closing the OSD, open a dummy OSD
            return new cDummyOsdObject();
        } else {
            // close OSD
            return nullptr;
        }
    }

    LOCK_CHANNELS_READ
    const cChannel *currentChannel = Channels->GetByNumber(cDevice::CurrentChannel());

    WebOSDPage* page = WebOSDPage::Create(useOutputDeviceScale, OSD);

    if (nextOsdCommand == OPEN) {
        if (!browserClient->RedButton(*currentChannel->GetChannelID().ToString())) {
            Skins.QueueMessage(mtInfo, tr("Browser not available"));
        }
    } else if (nextOsdCommand == REOPEN) {
        // nothing to do
    } else if (nextOsdCommand == URL) {
        browserClient->StartApplication(*currentChannel->GetChannelID().ToString(), "URL", param_userAgent, param_referrer, param_cookie, param_url);
    } else if (nextOsdCommand == MAIN) {
        browserClient->StartApplication(*currentChannel->GetChannelID().ToString(), "MAIN", param_userAgent, param_referrer, param_cookie, "");
    } else if (nextOsdCommand == M3U) {
        browserClient->StartApplication(*currentChannel->GetChannelID().ToString(), "M3U", param_userAgent, param_referrer, param_cookie, param_m3uContent);
    }

    nextOsdCommand = OPEN;

    return page;
}

cMenuSetupPage *cPluginWeb::SetupMenu() {
    // Return a setup menu in case the plugin supports one.
    return nullptr;
}

bool cPluginWeb::SetupParse(const char *Name, const char *Value) {
    // Parse your own setup parameters and store their values.
    return false;
}

bool cPluginWeb::Service(const char *Id, void *Data = nullptr) {
    param_url = "";
    param_m3uContent = "";
    param_userAgent = "";
    param_referrer = "";
    param_cookie = "";

    if (! Data) {
        // There exists no service without data
        return false;
    }

    if (strcmp(Id, "WebApp-Main-v1.0") == 0) {
        WebApp_Main_v1_0 *sc = (WebApp_Main_v1_0*)Data;
        nextOsdCommand = MAIN;
        cRemote::CallPlugin("web");

        param_userAgent = sc->userAgent;
        param_referrer = sc->referrer;
        param_cookie = sc->cookie;

        return true;
    }

    if (strcmp(Id, "WebApp-M3U-v1.0") == 0) {
        WebApp_M3U_v1_0 *sc = (WebApp_M3U_v1_0*)Data;
        nextOsdCommand = M3U;

        param_m3uContent = sc->m3uContent;
        param_userAgent = sc->userAgent;
        param_referrer = sc->referrer;
        param_cookie = sc->cookie;

        cRemote::CallPlugin("web");
        return true;
    }

    if (strcmp(Id, "WebApp-Url-v1.0") == 0) {
        WebApp_Url_v1_0 *sc = (WebApp_Url_v1_0*)Data;
        nextOsdCommand = URL;

        param_url = sc->url;
        param_userAgent = sc->userAgent;
        param_referrer = sc->referrer;
        param_cookie = sc->cookie;

        cRemote::CallPlugin("web");
        return true;
    }

    return false;
}

const char **cPluginWeb::SVDRPHelpPages() {
    // Return help text for SVDRP commands this plugin implements
    return nullptr;
}

cString cPluginWeb::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) {
    // Process SVDRP commands this plugin implements
    return nullptr;
}

bool cPluginWeb::readConfiguration(const char* configFile) {
    // check at first if configFile really exists
    printf("Config: %s\n", configFile);

    FILE *tf;
    if (tf = fopen("demo.txt", "r")) {
        fclose(tf);
        printf("file exists");
    } else {
        esyslog("[vdrweb] Unable to read config file: %s. Reason: %s", configFile, strerror(errno));
        return false;
    }

    mINI::INIFile file(configFile);
    mINI::INIStructure ini;
    auto result = file.read(ini);

    if (!result) {
        esyslog("[vdrweb] Unable to read config file: %s", configFile);
        return false;
    }

    try {
        browserIp = ini["browser"]["http_ip"];
        if (browserIp.empty()) {
            esyslog("[vdrweb] http ip (browser) not found");
            return false;
        }

        std::string tmpBrowserPort = ini["browser"]["http_port"];
        if (tmpBrowserPort.empty()) {
            esyslog("[vdrweb] http port (browser) not found");
            return false;
        }

        transcoderIp = ini["transcoder"]["http_ip"];
        if (transcoderIp.empty()) {
            esyslog("[vdrweb] http ip (transcoder) not found");
            return false;
        }

        std::string tmpTranscoderPort = ini["transcoder"]["http_port"];
        if (tmpTranscoderPort.empty()) {
            esyslog("[vdrweb] http port (transcoder) not found");
            return false;
        }

        vdrIp = ini["vdr"]["http_ip"];
        if (vdrIp.empty()) {
            esyslog("[vdrweb] http ip (vdr) not found");
            return false;
        }

        std::string tmpVdrPort = ini["vdr"]["http_port"];
        if (tmpVdrPort.empty()) {
            esyslog("[vdrweb] http port (vdr) not found");
            return false;
        }

        browserPort = std::stoi(tmpBrowserPort);
        transcoderPort = std::stoi(tmpTranscoderPort);
        vdrPort = std::stoi(tmpVdrPort);
    } catch (...) {
        esyslog("[vdrweb] configuration error. aborting...");
        return false;
    }

    return true;
}

VDRPLUGINCREATOR(cPluginWeb); // Don't touch this!
