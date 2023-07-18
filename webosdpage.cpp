#include <vdr/device.h>
#include <vdr/remote.h>
#include "webosdpage.h"
#include "browserclient.h"
#include "backtrace.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

tArea areas[] = {
//        {0, 0, 4096 - 1, 2160 - 1, 32}, // 4K
//        {0, 0, 2560 - 1, 1440 - 1, 32}, // 2K
//        {0, 0, 1920 - 1, 1080 - 1, 32}, // Full HD
        {0, 0, 1280 - 1,  720 - 1, 32}, // 720p
};

WebOSDPage *webOsdPage;
struct SwsContext *swsCtx = nullptr;

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

WebOSDPage::WebOSDPage() : cControl(nullptr) {
    dsyslog("[vdrweb] Create WebOSDPage\n");

    osd = nullptr;
    pixmap = nullptr;
    webOsdPage = this;

    runTriggerActivity = true;
    activityTriggerThread = new std::thread(triggerActivityThread);
}

WebOSDPage::~WebOSDPage() {
    webOsdPage = nullptr;

    dsyslog("[vdrweb] Destruct WebOSDPage\n");

    // printBacktrace();

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

    /*
    // set the maximum area size
    bool areaFound = false;
    for (int i = 0; i < 4; ++i) {
        auto areaResult = osd->SetAreas(&areas[i], 1);

        if (areaResult == oeOk) {
            isyslog("[vdrweb] Area size set to %d:%d - %d:%d", areas[i].x1, areas[i].y1, areas[i].x2, areas[i].y2);
            areaFound = true;
            break;
        }
    }

    if (!areaFound) {
        esyslog("[vdrweb] Unable set any OSD area. OSD will not be created");
    }
    */
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

#ifdef DEBUG_SAVE_OSD_IMAGE
    static int osd_image_number = 0;

    // the qoi encoder expects the RGBA pixel format.
    // For debugging purposes make a copy of the buffer and change the pixel format
    uint32_t* image_copy = new uint32_t[width * height];
    uint32_t* image_u32 = (uint32_t*) image;

    for (int i = 0; i < width * height; ++i) {
        // Source: BGRA = 0xAARRGGBB
        // Dest:   RGBA = 0xAABBGGRR
        image_copy[i] =
                ((image_u32[i] & 0xFF00FF00)      ) | // AA__GG__
                ((image_u32[i] & 0x00FF0000) >> 16) | // __RR____ -> ______RR
                ((image_u32[i] & 0x000000FF) << 16);  // ______BB -> __BB____
    }

    std::string filename = std::string("/tmp/osd_image_") + std::to_string(osd_image_number) + ".qoi";

    qoi_desc desc {
        .width = static_cast<unsigned int>(width),
        .height = static_cast<unsigned int>(height),
        .channels = 4,
        .colorspace = QOI_LINEAR
    };
    qoi_write(filename.c_str(), image_copy, &desc);
    osd_image_number++;

    delete(image_copy);
#endif

    return scaleAndPaint(image, width, height, AV_PIX_FMT_BGRA, AV_PIX_FMT_BGRA);
}

bool WebOSDPage::drawImageQOI(const std::string& qoibuffer) {
    // decode image data
    qoi_desc desc;
    void *image = qoi_decode(qoibuffer.c_str(), (int)qoibuffer.size(), &desc, 4);

    if (image == nullptr) {
        // something failed
        esyslog("[vdrweb] failed to decode qoi OSD image");
        return false;
    }

    bool retValue = scaleAndPaint(static_cast<uint8_t *>(image), (int)desc.width, (int)desc.height, AV_PIX_FMT_RGBA, AV_PIX_FMT_BGRA);
    free(image);

    return retValue;
}

bool WebOSDPage::scaleAndPaint(uint8_t* image, int width, int height, AVPixelFormat srcFormat, AVPixelFormat destFormat) {
    // sanity check
    if (width > 1920 || height > 1080 || width < 0 || height < 0) {
        return false;
    }

    // create image buffer for scaled image
    cSize recImageSize(disp_width, disp_height);
    cPoint recPoint(0, 0);
    const cImage recImage(recImageSize);
    auto *scaled  = (uint8_t*)(recImage.Data());

    if (scaled == nullptr) {
        esyslog("[vdrweb] Out of memory reading OSD image");
        return true;
    }

    // scale image
    if (swsCtx != nullptr) {
        swsCtx = sws_getCachedContext(swsCtx,
                                      width, height, srcFormat,
                                      disp_width, disp_height, destFormat,
                                      SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    if (swsCtx == nullptr) {
        swsCtx = sws_getContext(width, height, srcFormat,
                                disp_width, disp_height, destFormat,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    uint8_t *inData[1] = { image };
    int inLinesize[1] = {4 * width};
    int outLinesize[1] = {4 * disp_width};

    sws_scale(swsCtx, inData, inLinesize, 0, height, &scaled, outLinesize);

    if (pixmap != nullptr) {
        LOCK_PIXMAPS;
        pixmap->DrawImage(recPoint, recImage);
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
