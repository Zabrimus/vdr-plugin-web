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
#include "web.h"
#include "browserclient.h"
#include "ini.h"
#include "webosdpage.h"
#include "status.h"
#include "videocontrol.h"
#include "sharedmemory.h"
#include "dummyosd.h"
#include "httplib.h"

#define TSDIR  "%s/web/%s/%4d-%02d-%02d.%02d.%02d.%d-%d.rec"

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

httplib::Server vdrServer;
cHbbtvDeviceStatus *hbbtvDeviceStatus;

VideoPlayer* videoPlayer = nullptr;

std::string videoInfo;

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
        delete videoPlayer;
        videoPlayer = nullptr;

        nextOsdCommand = REOPEN;
        cRemote::CallPlugin("web");
    }
}

void startHttpServer(std::string vdrIp, int vdrPort, bool bindAll) {

    vdrServer.Post("/ProcessOsdUpdate", [](const httplib::Request &req, httplib::Response &res) {
        auto render_width = req.get_param_value("disp_width");
        auto render_height = req.get_param_value("disp_height");
        auto x = req.get_param_value("x");
        auto y = req.get_param_value("y");
        auto width = req.get_param_value("width");
        auto height = req.get_param_value("height");

        // dsyslog("[vdrweb] Incoming request /ProcessOsdUpdate with width %s, height %s", width.c_str(), height.c_str());

        if (width.empty() || height.empty()) {
            res.status = 404;
        } else {
            WebOSDPage* page = WebOSDPage::Get();
            if (page == nullptr) {
                // illegal request -> abort
                esyslog("[vdrweb] ProcessOsdUpdate: osd update request while webOsdPage is null.");
                res.status = 404;
                return;
            }

            bool result = page->drawImage(sharedMemory.Get(), std::stoi(render_width), std::stoi(render_height), std::stoi(x), std::stoi(y), std::stoi(width), std::stoi(height));

            if (result) {
                res.status = 200;
                res.set_content("ok", "text/plain");
            }  else {
                res.status = 500;
                res.set_content("error", "text/plain");
            }
        }
    });

    vdrServer.Post("/ProcessOsdUpdateQOI", [](const httplib::Request &req, httplib::Response &res) {
        const std::string body = req.body;

        WebOSDPage* page = WebOSDPage::Get();
        if (page == nullptr) {
            // illegal request -> abort
            esyslog("[vdrweb] ProcessOsdUpdateQOI: osd update request while webOsdPage is null.");
            res.status = 404;
            return;
        }

        bool result = page->drawImageQOI(body);

        if (result) {
            res.status = 200;
            res.set_content("ok", "text/plain");
        }  else {
            res.status = 500;
            res.set_content("error", "text/plain");
        }
    });

    vdrServer.Post("/ProcessTSPacket", [](const httplib::Request &req, httplib::Response &res) {
        const std::string body = req.body;

        if (body.empty()) {
            res.status = 404;
        } else {
            if (saveTS) {
                FILE* f = fopen(currentTSFilename, "a");
                if (f != nullptr) {
                    fwrite((uint8_t *)body.c_str(), body.length(), 1, f);
                    fclose(f);
                }
            }

            if (videoPlayer != nullptr) {
                videoPlayer->PlayPacket((uint8_t *) body.c_str(), (int) body.length());

                if (videoPlayer->hasTsError()) {
                    // stop video streaming
                    browserClient->StopVideo();
                    stopVideo();
                }
            } else {
                // No video Player? Stop streaming
                stopVideo();
            }

            res.status = 200;
            res.set_content("ok", "text/plain");
        }
    });

    vdrServer.Post("/StartVideo", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] StartVideo received");

        videoInfo = req.get_param_value("videoInfo");

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

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Get("/StopVideo", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] StopVideo received");

        stopVideo();

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Get("/PauseVideo", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] PauseVideo received");

        if (videoPlayer != nullptr) {
            videoPlayer->Pause();
        }

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Get("/ResumeVideo", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] ResumeVideo received");

        if (videoPlayer != nullptr) {
            videoPlayer->Resume();
        }

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Post("/VideoSize", [](const httplib::Request &req, httplib::Response &res) {
        auto x = req.get_param_value("x");
        auto y = req.get_param_value("y");
        auto w = req.get_param_value("w");
        auto h = req.get_param_value("h");

        dsyslog("[vdrweb] Incoming request /VideoSize with x %s, y %s, width %s, height %s", x.c_str(), y.c_str(), w.c_str(), h.c_str());

        if (x.empty() || y.empty() || w.empty() || h.empty()) {
            res.status = 404;
        } else {
            lastVideoX = std::atoi(x.c_str());
            lastVideoY = std::atoi(y.c_str());
            lastVideoWidth = std::atoi(w.c_str());
            lastVideoHeight = std::atoi(h.c_str());

            videoPlayer->SetVideoSize(lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight);

            res.status = 200;
            res.set_content("ok", "text/plain");
        }
    });

    vdrServer.Get("/VideoFullscreen", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] VideoFullscreen received");

        lastVideoX = lastVideoY = lastVideoWidth = lastVideoHeight = 0;

        videoPlayer->setVideoFullscreen();

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Get("/Hello", [](const httplib::Request &req, httplib::Response &res) {
        browserClient->HelloFromBrowser();

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Post("/ResetVideo", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] ResetVideo received: Coords x=%d, y=%d, w=%d, h=%d", lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight);

        std::string vi = req.get_param_value("videoInfo");

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

            dsyslog("[vdrweb] video change from %s to %s", videoInfo.c_str(), vi.c_str());

            if (videoInfo != vi) {
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

            videoPlayer->SetVideoSize(lastVideoX, lastVideoY, lastVideoWidth, lastVideoHeight);
        } else {
            // TODO: Is it necessary to create a new Player?
            esyslog("[vdrweb] ResetVideo called, but videoPlayer is null");
        }

        videoInfo = vi;

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Get("/Seeked", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] Seeked received");
        if (videoPlayer != nullptr) {
            videoPlayer->ResetVideo();
        }

        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    vdrServer.Post("/SelectAudioTrack", [](const httplib::Request &req, httplib::Response &res) {
        dsyslog("[vdrweb] SelectAudioTrack received");
        if (videoPlayer != nullptr) {
            auto track = req.get_param_value("audioTrack");
            videoPlayer->SelectAudioTrack(track);
        }

        res.status = 200;
        res.set_content("ok", "text/plain");
    });


    std::string listenIp = bindAll ? "0.0.0.0" : vdrIp;
    if (!vdrServer.listen(listenIp, vdrPort)) {
        esyslog("[vdrweb] Call of listen failed: ip %s, port %d, Reason: %s", listenIp.c_str(), vdrPort, strerror(errno));
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
    MagickLib::InitializeMagickEx(NULL, MAGICK_OPT_NO_SIGNAL_HANDER, NULL);

    return true;
}

bool cPluginWeb::Start() {
    isyslog("[vdrweb] Start hbbtv url collector");
    hbbtvDeviceStatus = new cHbbtvDeviceStatus();

    isyslog("[vdrweb] Start Http Server on %s:%d", vdrIp.c_str(), vdrPort);
    std::thread t1(startHttpServer, vdrIp, vdrPort, bindAll);
    t1.detach();

    new BrowserClient(browserIp, browserPort);

    return true;
}

void cPluginWeb::Stop() {
    vdrServer.stop();
    delete browserClient;

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

    if (WebOSDPage::Get() == nullptr && videoPlayer != nullptr) {
        delete videoPlayer;
        videoPlayer = nullptr;
    }
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
        browserClient->RedButton(*currentChannel->GetChannelID().ToString());
    } else if (nextOsdCommand == REOPEN) {
        // nothing to do
    } else if (nextOsdCommand == URL) {
        browserClient->StartApplication(*currentChannel->GetChannelID().ToString(), "URL", param_userAgent, param_referrer, param_cookie, param_url);
    } else if (nextOsdCommand == MAIN) {
        browserClient->StartApplication(*currentChannel->GetChannelID().ToString(), "MAIN", param_userAgent, param_referrer, param_cookie);
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
