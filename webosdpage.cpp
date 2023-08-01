#include <vdr/device.h>
#include <vdr/remote.h>
#include "webosdpage.h"
#include "browserclient.h"
#include "backtrace.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

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

        if ((60 * 1000) - (counter * waitTime) <= 0) {
            cRemote::TriggerLastActivity();
            counter = 0;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        }
    }
}

WebOSDPage::WebOSDPage(bool useOutputDeviceScale)
        : cControl(nullptr), useOutputDeviceScale(useOutputDeviceScale)
{
    dsyslog("[vdrweb] Create WebOSDPage\n");

    osd = nullptr;
    pixmap = nullptr;
    webOsdPage = this;
    disp_height = 1920;
    disp_width = 1080;

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

    osd = cOsdProvider::NewOsd(0, 0, 5);

    double ph;
    cDevice::PrimaryDevice()->GetOsdSize(disp_width, disp_height, ph);
    tArea area {0,0,disp_width, disp_height, 32};
    auto areaResult = osd->SetAreas(&area, 1);

    if (areaResult == oeOk) {
        isyslog("[vdrweb] Area size set to %d:%d - %d:%d", 0, 0, disp_width, disp_height);
    }

    SetOsdSize();

    browserClient->ReloadOSD();
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

    if (disp_width <= 0 || disp_height <= 0 || disp_width > 3840 || disp_height > 2160) {
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

bool WebOSDPage::drawImage(uint8_t* image, int render_width, int render_height, int x, int y, int width, int height) {

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

    return scaleAndPaint(image, render_width, render_height, x, y, width, height, AV_PIX_FMT_BGRA, AV_PIX_FMT_BGRA);
}

bool WebOSDPage::drawImageQOI(const std::string& qoibuffer) {
    std::string::size_type pos1 = 0, pos2 = 0, pos_rw = 0, pos_rh = 0;

    // extract render_width, render_height
    pos_rw = qoibuffer.find(':', 0);
    int render_width = std::atoi(qoibuffer.substr(0, pos_rw).c_str());

    pos_rh = qoibuffer.find(':', pos_rw + 1);
    int render_height = std::atoi(qoibuffer.substr(pos_rw + 1, pos_rh).c_str());

    // extract x,y
    pos1 = qoibuffer.find(':', pos_rh + 1);
    int x = std::atoi(qoibuffer.substr(pos_rh + 1, pos1).c_str());

    pos2 = qoibuffer.find(':', pos1 + 1);
    int y = std::atoi(qoibuffer.substr(pos1 + 1, pos2).c_str());

    // decode image data
    qoi_desc desc;
    void *image = qoi_decode(qoibuffer.c_str() + pos2 + 1, (int)qoibuffer.size() - pos2 - 1, &desc, 4);

    if (image == nullptr) {
        // something failed
        esyslog("[vdrweb] failed to decode qoi OSD image");
        return false;
    }

    bool retValue = scaleAndPaint(static_cast<uint8_t *>(image), render_width, render_height, x, y, (int)desc.width, (int)desc.height, AV_PIX_FMT_BGRA, AV_PIX_FMT_BGRA);
    free(image);

    return retValue;
}

bool WebOSDPage::scaleAndPaint(uint8_t* image, int render_width, int render_height, int x, int y, int width, int height, AVPixelFormat srcFormat, AVPixelFormat destFormat) {
    // sanity check
    if (width > 3840 || height > 2160 || width <= 0 || height <= 0) {
        return false;
    }

    bool scaleRequired = (disp_width != render_width) || (disp_height != render_height);

    // dsyslog("RenderSize: %d x %d, DisplaySize: %d x %d, x,y=%d,%d, Scaling required: %s", render_width, render_height, disp_width, disp_height, x, y, (scaleRequired ? "yes" : "no"));

    if (!scaleRequired) {
        // scaling is not needed. Draw the image as is.
        cPoint recPoint(x, y);
        const cImage recImage(cSize(width, height), (const tColor *)image);

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

    // calculate scale factor
    double scalex = disp_width / (double)render_width;
    double scaley = disp_height / (double)render_height;

    // calculate coordinates
    int osd_x = (int)lround(scalex * x);
    int osd_y = (int)lround(scaley * y);
    int osd_width = (int)lround(scalex * width);
    int osd_height = (int)lround(scalex * height);

    cPoint recPoint(osd_x, osd_y);

#ifdef ENABLE_FAST_SCALE
    if (useOutputDeviceScale) {
        const cImage recImage(cSize(width, height), (const tColor *)image, scalex, scaley);

        if (pixmap != nullptr) {
            LOCK_PIXMAPS;
            pixmap->DrawImage(recPoint, recImage);
        } else {
            esyslog("[vdrweb] Pixmap is null. OSD not available");
        }
    } else {
#endif // ENABLE_FAST_SCALE

        // create image buffer for scaled image
        cSize recImageSize(osd_width, osd_height);
        const cImage recImage(recImageSize);
        auto *scaled = (uint8_t *) (recImage.Data());

        if (scaled == nullptr) {
            esyslog("[vdrweb] Out of memory reading OSD image");
            return true;
        }

        // scale image
        if (swsCtx != nullptr) {
            swsCtx = sws_getCachedContext(swsCtx,
                                          width, height, srcFormat,
                                          osd_width, osd_height, destFormat,
                                          SWS_BILINEAR, nullptr, nullptr, nullptr);
        }

        if (swsCtx == nullptr) {
            swsCtx = sws_getContext(width, height, srcFormat,
                                    osd_width, osd_height, destFormat,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
        }

        uint8_t *inData[1] = {image};
        int inLinesize[1] = {4 * width};
        int outLinesize[1] = {4 * osd_width};

        sws_scale(swsCtx, inData, inLinesize, 0, height, &scaled, outLinesize);

        if (pixmap != nullptr) {
            LOCK_PIXMAPS;
            pixmap->DrawImage(recPoint, recImage);
        } else {
            esyslog("[vdrweb] Pixmap is null. OSD not available");
        }
#ifdef ENABLE_FAST_SCALE
    }
#endif

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
