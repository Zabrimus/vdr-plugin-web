#include <vdr/device.h>
#include <vdr/remote.h>
#include "webosdpage.h"
#include "browserclient.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

tArea areas[] = {
//        {0, 0, 4096 - 1, 2160 - 1, 32}, // 4K
//        {0, 0, 2560 - 1, 1440 - 1, 32}, // 2K
//        {0, 0, 1920 - 1, 1080 - 1, 32}, // Full HD
        {0, 0, 1280 - 1,  720 - 1, 32}, // 720p
};

struct SwsContext *swsCtx = nullptr;

WebOSDPage *currentPage = nullptr;
WebOSDPage *currentVideoPage = nullptr;

// initialize keyMap
std::map<int, std::string> keyMap({
        {kUp, "VK_UP"},
        {kDown, "VK_DOWN"},
        {kLeft, "VK_LEFT"},
        {kRight, "VK_RIGHT"},
        {kOk, "VK_ENTER"},
        {kBack, "VK_BACK"},
        {kRed, "VK_RED"},
        {kGreen, "VK_GREEN"},
        {kYellow, "VK_YELLOW"},
        {kBlue, "VK_BLUE"},
        {k0, "VK_0"},
        {k1, "VK_1"},
        {k2, "VK_2"},
        {k3, "VK_3"},
        {k4, "VK_4"},
        {k5, "VK_5"},
        {k6, "VK_6"},
        {k7, "VK_7"},
        {k8, "VK_8"},
        {k9, "VK_9"},
        {kPlay, "VK_PLAY"},
        {kPause, "VK_PAUSE"},
        {kStop, "VK_STOP"},
        {kFastFwd, "VK_FAST_FWD"},
        {kFastRew, "VK_REWIND"},
        {kNext, "VK_PAGE_DOWN"},
        {kPrev, "VK_PAGE_UP"},
});

// Thread to prevent automatically closing the OSD
bool runTriggerActivity = false;
void triggerActivityThread() {
    int counter = 0;
    int waitTime = 100;

    while (runTriggerActivity) {
        counter++;

        if ((60 * 1000) % (counter * waitTime) == 0) {
            cRemote::TriggerLastActivity();
            counter = 0;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        }
    }
}

WebOSDPage::WebOSDPage(bool isPage) : cControl(nullptr) {
    dsyslog("[vdrweb] Create WebOSDPage\n");

    osd = nullptr;
    pixmap = nullptr;

    double ph;
    cDevice::PrimaryDevice()->GetOsdSize(disp_width, disp_height, ph);

    if (isPage) {
        currentPage = this;
        currentVideoPage = nullptr;
    } else {
        currentPage = nullptr;
        currentVideoPage = this;

        // init OSD. For pages, this will be done in a seperate call of Display()
        Display();
    }

    runTriggerActivity = true;
    activityTriggerThread = new std::thread(triggerActivityThread);
}

WebOSDPage::~WebOSDPage() {
    /*
    cStringList stringList;

    // cBackTrace::BackTrace(stringList, int Level = 0, bool Mangled = false);
    cBackTrace::BackTrace(stringList);
    esyslog("[vdrweb] Backtrace size: %d", stringList.Size());
    for (int i = 0; i < stringList.Size(); ++i) {
        esyslog("[vdrweb] ==> %s", stringList[i]);
    }

    esyslog("[vdrweb] ==> Caller: %s", *cBackTrace::GetCaller());
    */

    currentPage = nullptr;
    currentVideoPage = nullptr;

    dsyslog("[vdrweb] Destruct WebOSDPage\n");

    runTriggerActivity = false;
    activityTriggerThread->join();

    sws_freeContext(swsCtx);
    swsCtx = nullptr;

    if (pixmap != nullptr) {
        osd->DestroyPixmap(pixmap);
        pixmap = nullptr;
    }

    if (osd != nullptr) {
        delete osd;
        osd = nullptr;
    }
}

void WebOSDPage::Show() {
    dsyslog("[vdrweb] WebOSDPage show\n");

    Display();
}

void WebOSDPage::Display() {
    dsyslog("[vdrweb] WebOSDPage Display\n");
    if (osd) {
        delete osd;
    }

    osd = cOsdProvider::NewOsd(0, 0, OSD_LEVEL_SUBTITLES);

    double ph;
    cDevice::PrimaryDevice()->GetOsdSize(disp_width, disp_height, ph);
    tArea area {0,0,disp_width, disp_height };
    auto areaResult = osd->SetAreas(&area, 1);

    if (areaResult == oeOk) {
        isyslog("[vdrweb] Area size set to %d:%d - %d:%d", 0, 0, disp_width, disp_height);
    }

    SetOsdSize();
}

void WebOSDPage::SetOsdSize() {
    dsyslog("[vdrweb] webospage SetOsdSize()");

    if (pixmap != nullptr) {
        dsyslog("[vdrweb] webosdpage SetOsdSize, Destroy old pixmap");

        osd->DestroyPixmap(pixmap);
        pixmap = nullptr;
    }

    dsyslog("[vdrweb] webosdpage SetOsdSize, Get new OSD size");
    double ph;
    cDevice::PrimaryDevice()->GetOsdSize(disp_width, disp_height, ph);

    if (disp_width <= 0 || disp_height <= 0 || disp_width > 4096 || disp_height > 2160) {
        esyslog("[vdrweb] Got illegal OSD size %dx%d", disp_width, disp_height);
        return;
    }

    cRect rect(0, 0, disp_width, disp_height);

    // try to get a pixmap
    dsyslog("[vdrweb] webosdpage SetOsdSize, Create pixmap %dx%d", disp_width, disp_height);
    pixmap = osd->CreatePixmap(1, rect, rect);

    dsyslog("[vdrweb] webosdpage SetOsdSize, Clear Pixmap");
    LOCK_PIXMAPS;
    pixmap->Clear();
}

bool WebOSDPage::drawImage(uint8_t* image, int width, int height) {
    // create image buffer for scaled image
    cSize recImageSize(disp_width, disp_height);
    cPoint recPoint(0, 0);
    const cImage recImage(recImageSize);
    auto *scaled  = (uint8_t*)(recImage.Data());

    if (scaled == nullptr) {
        esyslog("[vdrweb] Out of memory reading OSD image");
        return true;
    }

    swsCtx = sws_getCachedContext(swsCtx,
                                  width, height, AV_PIX_FMT_BGRA,
                                  disp_width, disp_height, AV_PIX_FMT_BGRA,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);

    uint8_t *inData[1] = { image };
    int inLinesize[1] = {4 * width};
    int outLinesize[1] = {4 * disp_width};

    sws_scale(swsCtx, inData, inLinesize, 0, height, &scaled, outLinesize);

    if (pixmap != nullptr) {
        LOCK_PIXMAPS;
        // Pixmap::Lock();
        pixmap->DrawImage(recPoint, recImage);
        // pixmap->Unlock();
    } else {
        esyslog("[vdrweb] Pixmap is null. OSD not available");
    }

    if (osd != nullptr) {
        osd->Flush();
    }

    return true;
}

eOSState WebOSDPage::ProcessKey(eKeys Key) {
    eOSState state = cOsdObject::ProcessKey(Key);

    if (state == osUnknown) {
        // special key: kInfo -> Load Application in Browser
        if (Key == kInfo) {
            LOCK_CHANNELS_READ
            const cChannel *currentChannel = Channels->GetByNumber(cDevice::CurrentChannel());
            // browserClient->RedButton(*currentChannel->GetChannelID().ToString());
            browserClient->StartApplication(*currentChannel->GetChannelID().ToString(), "currently unused");
            return osContinue;
        }

        auto search = ::keyMap.find(Key);
        if (search != ::keyMap.end()) {
            browserClient->ProcessKey(search->second);
            return osContinue;
        }
    }

    return state;
}

void WebOSDPage::Hide() {
    dsyslog("[vdrweb] WebOSDPage::Hide...");
}

cOsdObject *WebOSDPage::GetInfo() {
    dsyslog("[vdrweb] WebOSDPage::GetInfo...");

    return this;
}
