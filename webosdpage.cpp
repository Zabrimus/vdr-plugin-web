#include <vdr/config.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vdr/device.h>
#include <vdr/remote.h>
#include <Magick++/Blob.h>
#include <Magick++/Image.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include "webosdpage.h"
#include <algorithm>

extern bool measureOsd;
#include "web.h"
#include "setup.h"

// #define MEASURE_SCALE_TIME 1
// #define DEBUG_SAVE_OSD_IMAGE 1

#define QOI_IMPLEMENTATION
#include "qoi.h"

// the state of the browser page, also before a WebOSDPage exists
static std::atomic<bool> pageEmpty(false);
static std::atomic<uint64_t> pageEmptySince(0);

std::recursive_mutex pixmap_mutex;

WebOSDPage *webOsdPage;
WebOSDPage *playerWebOsdPage;

OSD_MODE currentOsdMode = CLOSED;

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
        {kPlayPause, "VK_PLAY_PAUSE"},
        // HbbTV: next/previous are the skip keys of the VCR keyset
        {kNext, "VK_TRACK_NEXT"},
        {kPrev, "VK_TRACK_PREV"},
        {kInfo, "VK_INFO"},
        {kSubtitles, "VK_SUBTITLE"},
});

WebOSDPage *WebOSDPage::Create(bool useOutputDeviceScale, OSD_MODE osdMode) {
    switch(osdMode) {
        case OSD:
            if (webOsdPage == nullptr) {
                webOsdPage = new WebOSDPage(useOutputDeviceScale, osdMode);
            }
            webOsdPage->useOutputDeviceScale = useOutputDeviceScale;
            currentOsdMode = OSD;
            return webOsdPage;

        case PLAYER:
            if (playerWebOsdPage == nullptr) {
                playerWebOsdPage = new WebOSDPage(useOutputDeviceScale, osdMode);
            }
            playerWebOsdPage->useOutputDeviceScale = useOutputDeviceScale;
            currentOsdMode = PLAYER;
            return playerWebOsdPage;

        case CLOSED:
            return nullptr;
    }

    return nullptr;
}

WebOSDPage *WebOSDPage::Get() {
    if (currentOsdMode == OSD) {
        // dsyslog("[vdrweb] WebOSDPage::Get(%d): Return webOsdPage %p", (int)currentOsdMode, webOsdPage);
        return webOsdPage;
    } else if (currentOsdMode == PLAYER) {
        // dsyslog("[vdrweb] WebOSDPage::Get(%d): Return playerWebOsdPage %p", (int)currentOsdMode, playerWebOsdPage);
        return playerWebOsdPage;
    }

    // dsyslog("[vdrweb] WebOSDPage::Get(-1): Return nullptr");
    return nullptr;
}

bool WebOSDPage::IsOpen() {
    return (currentOsdMode == OSD && webOsdPage != nullptr)
       || (currentOsdMode == PLAYER && playerWebOsdPage != nullptr);
}

WebOSDPage::WebOSDPage(bool useOutputDeviceScale, OSD_MODE osdMode)
        : cControl(nullptr), useOutputDeviceScale(useOutputDeviceScale)
{
    dsyslog("[vdrweb] Create WebOSDPage, osdMode %d", (int)osdMode);

    osd = nullptr;
    pixmap = nullptr;
    disp_height = 1920;
    disp_width = 1080;
    lastVolume = -1;
    lastVolumeTime = time(NULL);

    currentMode = osdMode;

    // the browser may show the application already
    contentSeen = !pageEmpty;
    lastKey.Set();
}

WebOSDPage::~WebOSDPage() {
    dsyslog("[vdrweb] Destruct WebOSDPage, osdMode %d", (int)currentMode);

    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
    DestroyOsd();

    switch (currentMode) {
        case OSD:
            webOsdPage = nullptr;
            break;

        case PLAYER:
            playerWebOsdPage = nullptr;
            break;

        case CLOSED:
            break;
    }

    // cDevice::PrimaryDevice()->ScaleVideo(cRect::Null);
}

void WebOSDPage::Show() {
    dsyslog("[vdrweb] WebOSDPage show\n");

    Display();
}

void WebOSDPage::SetEmpty(bool empty) {
    if (empty && !pageEmpty) {
        pageEmptySince = cTimeMs::Now();
    }
    pageEmpty = empty;
}

void WebOSDPage::ShowHint(bool show) {
    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
    if (osd == nullptr || show == (pixmapHint != nullptr)) {
        return;
    }

    if (!show) {
        osd->DestroyPixmap(pixmapHint);
        pixmapHint = nullptr;
        osd->Flush();
        return;
    }

    // "HbbTV  (red key) Start  (MENU key) Close"
    const cFont *font = cFont::GetFont(fontOsd);
    const char *hbbtv = "HbbTV";
    const char *start = tr("Start");
    const char *closeText = tr("Close");
    const char *menu = "MENU";

    int lineHeight = font->Height();
    int border = lineHeight / 2;
    int gap = lineHeight / 4;            // between a key and its text
    int space = lineHeight;              // between the parts
    int keyHeight = lineHeight * 3 / 4;
    cFont *keyFont = cFont::CreateFont(Setup.FontSml, keyHeight / 2);

    int redWidth = keyHeight * 3 / 2;
    int menuWidth = keyFont->Width(menu) + keyHeight * 2 / 3;
    int w = border + font->Width(hbbtv) + space + redWidth + gap + font->Width(start) +
            space + menuWidth + gap + font->Width(closeText) + border;
    int h = lineHeight + border;
    int x = disp_width - w - disp_width / 20;
    int y = disp_height / 20;

    pixmapHint = osd->CreatePixmap(7, cRect(x, y, w, h));
    LOCK_PIXMAPS;
    pixmapHint->Fill(0xC0000000);

    int textY = border / 2;
    int keyY = textY + (lineHeight - keyHeight) / 2;
    int pos = border;

    auto text = [&](const char *s) {
        pixmapHint->DrawText(cPoint(pos, textY), s, clrWhite, clrTransparent, font);
        pos += font->Width(s);
    };

    text(hbbtv);
    pos += space;
    DrawKey(pixmapHint, cRect(pos, keyY, redWidth, keyHeight), 0xFFE01010);
    pos += redWidth + gap;
    text(start);
    pos += space;
    DrawKey(pixmapHint, cRect(pos, keyY, menuWidth, keyHeight), 0xFF505050);
    pixmapHint->DrawText(cPoint(pos, keyY), menu, clrWhite, clrTransparent, keyFont, menuWidth, keyHeight, taCenter);
    pos += menuWidth + gap;
    text(closeText);

    delete keyFont;
    osd->Flush();
}

// a key of the remote control: a light frame and rounded corners
void WebOSDPage::DrawKey(cPixmap *pixmap, const cRect& rect, tColor color) {
    auto rounded = [pixmap](const cRect& r, tColor c) {
        int d = std::max(2, r.Height() / 3);
        pixmap->DrawRectangle(cRect(r.X() + d, r.Y(), r.Width() - 2 * d, r.Height()), c);
        pixmap->DrawRectangle(cRect(r.X(), r.Y() + d, d, r.Height() - 2 * d), c);
        pixmap->DrawRectangle(cRect(r.Right() - d + 1, r.Y() + d, d, r.Height() - 2 * d), c);
        pixmap->DrawEllipse(cRect(r.Right() - d + 1, r.Y(), d, d), c, 1);
        pixmap->DrawEllipse(cRect(r.X(), r.Y(), d, d), c, 2);
        pixmap->DrawEllipse(cRect(r.X(), r.Bottom() - d + 1, d, d), c, 3);
        pixmap->DrawEllipse(cRect(r.Right() - d + 1, r.Bottom() - d + 1, d, d), c, 4);
    };

    int frame = std::max(1, rect.Height() / 12);
    rounded(rect, 0xFFD0D0D0);
    rounded(cRect(rect.X() + frame, rect.Y() + frame, rect.Width() - 2 * frame, rect.Height() - 2 * frame), color);
}

// The application shows nothing, e.g. the red button hint has disappeared: a hint
// tells that HbbTV is still open, and without a key it is closed after a while.
void WebOSDPage::CheckEmptyPage(eKeys key) {
    if (currentMode != OSD || hidden) {
        return;
    }

    if (key != kNone) {
        lastKey.Set();
    }

    bool empty = pageEmpty;
    if (!empty) {
        contentSeen = true;
    }

    uint64_t now = cTimeMs::Now();
    uint64_t emptyFor = now - pageEmptySince;

    // a short gap while the application changes its page shows no hint
    bool hint = empty && contentSeen && emptyFor >= 1000;
    ShowHint(hint);

    int seconds = webSetup.autoClose;
    if (hint && seconds > 0 && emptyFor >= (uint64_t) seconds * 1000 && lastKey.Elapsed() >= (uint64_t) seconds * 1000) {
        isyslog("[vdrweb] the application shows nothing for %d s, close", seconds);
        closeEmptyPage = true;
    }
}

void WebOSDPage::DestroyOsd() {
    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);

    if (osd != nullptr) {
        if (pixmap != nullptr) {
            osd->DestroyPixmap(pixmap);
            pixmap = nullptr;
        }

        if (pixmapVol != nullptr) {
            osd->DestroyPixmap(pixmapVol);
            pixmapVol = nullptr;
        }

        if (pixmapHint != nullptr) {
            osd->DestroyPixmap(pixmapHint);
            pixmapHint = nullptr;
        }

        delete osd;
        osd = nullptr;
    }
}

void WebOSDPage::HideOsd() {
    dsyslog("[vdrweb] WebOSDPage HideOsd");

    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
    hidden = true;
    DestroyOsd();
}

void WebOSDPage::Display() {
    dsyslog("[vdrweb] WebOSDPage Display\n");
    DestroyOsd();

    eOsdError areaResult;
    {
        std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
        hidden = false;
        osd = cOsdProvider::NewOsd(0, 0, 5);

        double ph;
        cDevice::PrimaryDevice()->GetOsdSize(disp_width, disp_height, ph);
        tArea area {0, 0, disp_width - 1, disp_height - 1, 32};
        areaResult = osd->SetAreas(&area, 1);
    }

    if (areaResult == oeOk) {
        isyslog("[vdrweb] Area size set to %d:%d - %d:%d", 0, 0, disp_width - 1, disp_height - 1);
    }

    SetOsdSize();

    browserClient->ReloadOSD();
}

void WebOSDPage::SetOsdSize() {
    dsyslog("[vdrweb] webospage SetOsdSize()");

    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
    if (osd == nullptr) {
        return;
    }

    if (pixmap != nullptr) {
        dsyslog("[vdrweb] webosdpage SetOsdSize, Destroy old pixmap");

        osd->DestroyPixmap(pixmap);
        pixmap = nullptr;
    }

    dsyslog("[vdrweb] webosdpage SetOsdSize, Get new OSD size");
    double ph;
    cDevice::PrimaryDevice()->GetOsdSize(disp_width, disp_height, ph);

    /*
    if (disp_width <= 0 || disp_height <= 0 || disp_width > 3840 || disp_height > 2160) {
        esyslog("[vdrweb] Got illegal OSD size %dx%d", disp_width, disp_height);
        return;
    }
    */

    cRect rect(0, 0, disp_width, disp_height);

    // try to get a pixmap
    dsyslog("[vdrweb] webosdpage SetOsdSize, Create pixmap %dx%d", disp_width, disp_height);
    pixmap = osd->CreatePixmap(5, rect, rect);

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

    return scaleAndPaint(image, render_width, render_height, x, y, width, height);
}

// measurement (-m): time from a key to its frame and statistics of the OSD frames
static std::mutex measureMutex;
static eKeys measureKey = kNone;
static cTimeMs measureKeyTime;

struct tOsdStats {
    cTimeMs start;
    int frames = 0;
    int rects = 0;
    size_t bytes = 0;
    uint64_t draw = 0;
    uint64_t drawMax = 0;
    uint64_t flush = 0;
    uint64_t flushMax = 0;
};

static tOsdStats osdStats;

static void measureFrame(int rects, size_t bytes, uint64_t draw, uint64_t flush) {
    std::lock_guard<std::mutex> lock(measureMutex);

    if (measureKey != kNone) {
        isyslog("[vdrweb] [measure] key %s: frame drawn after %d ms (draw %d ms, flush %d ms)",
                cKey::ToString(measureKey), (int) measureKeyTime.Elapsed(), (int) draw, (int) flush);
        measureKey = kNone;
    }

    osdStats.frames++;
    osdStats.rects += rects;
    osdStats.bytes += bytes;
    osdStats.draw += draw;
    osdStats.drawMax = std::max(osdStats.drawMax, draw);
    osdStats.flush += flush;
    osdStats.flushMax = std::max(osdStats.flushMax, flush);

    uint64_t elapsed = osdStats.start.Elapsed();
    if (elapsed >= 5000) {
        isyslog("[vdrweb] [measure] OSD: %d frames in %.1f s, %.1f regions/frame, %.1f MB, draw avg %.1f max %d ms, flush avg %.1f max %d ms",
                osdStats.frames, elapsed / 1000.0, (double) osdStats.rects / osdStats.frames, osdStats.bytes / 1048576.0,
                (double) osdStats.draw / osdStats.frames, (int) osdStats.drawMax,
                (double) osdStats.flush / osdStats.frames, (int) osdStats.flushMax);
        osdStats = tOsdStats();
    }
}

bool WebOSDPage::drawFrame(const pluginweb::ProcessOsdFrameType &frame) {
    bool ok = true;
    cTimeMs drawTime;

    for (const auto &r : frame.rects) {
        if (frame.qoi) {
            qoi_desc desc;
            void *image = qoi_decode(r.data.c_str(), (int) r.data.size(), &desc, 4);
            if (image == nullptr) {
                esyslog("[vdrweb] failed to decode qoi OSD image");
                ok = false;
                continue;
            }

            ok &= scaleAndPaint(static_cast<uint8_t *>(image), frame.render_width, frame.render_height, r.x, r.y, (int) desc.width, (int) desc.height, false);
            free(image);
        } else {
            if ((int64_t) r.width * r.height * 4 != (int64_t) r.data.size()) {
                ok = false;
                continue;
            }

            ok &= scaleAndPaint((uint8_t *) r.data.c_str(), frame.render_width, frame.render_height, r.x, r.y, r.width, r.height, false);
        }
    }

    uint64_t draw = drawTime.Elapsed();
    cTimeMs flushTime;

    // one flush for the whole frame
    {
        std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
        if (osd != nullptr) {
            osd->Flush();
        }
    }

    if (measureOsd) {
        size_t bytes = 0;
        for (const auto &r : frame.rects) {
            bytes += r.data.size();
        }
        measureFrame((int) frame.rects.size(), bytes, draw, flushTime.Elapsed());
    }

    return ok;
}

// the shared memory of the browser, kept until it changes (a new size)
static std::string shmName;
static uint8_t *shmData = nullptr;
static size_t shmSize = 0;

void WebOSDPage::ReleaseShm() {
    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
    if (shmData != nullptr) {
        munmap(shmData, shmSize);
        shmData = nullptr;
        shmSize = 0;
        shmName.clear();
    }
}

bool WebOSDPage::drawShmFrame(const pluginweb::ProcessOsdShmType &frame) {
    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);

    size_t size = (size_t) frame.render_width * frame.render_height * 4;
    if (frame.render_width <= 0 || frame.render_height <= 0 || frame.render_width > 3840 || frame.render_height > 2160) {
        return false;
    }

    if (frame.name != shmName || shmData == nullptr) {
        ReleaseShm();

        // a file of /dev/shm, nothing else
        if (frame.name.empty() || frame.name.find('/') != std::string::npos || frame.name[0] == '.') {
            return false;
        }

        std::string path = "/dev/shm/" + frame.name;
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            esyslog("[vdrweb] unable to open the OSD in %s: %s", path.c_str(), strerror(errno));
            return false;
        }

        struct stat st {};
        void *p = MAP_FAILED;
        if (fstat(fd, &st) == 0 && (size_t) st.st_size >= size) {
            p = mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        }
        close(fd);

        if (p == MAP_FAILED) {
            esyslog("[vdrweb] unable to map the OSD in %s", path.c_str());
            return false;
        }

        shmData = (uint8_t *) p;
        shmSize = st.st_size;
        shmName = frame.name;
        isyslog("[vdrweb] OSD in shared memory %s", path.c_str());
    }

    if (shmSize < size) {
        return false;
    }

    cTimeMs drawTime;
    size_t bytes = 0;
    std::vector<uint8_t> region;

    for (const auto &r : frame.rects) {
        if (r.x < 0 || r.y < 0 || r.width <= 0 || r.height <= 0 || r.x + r.width > frame.render_width || r.y + r.height > frame.render_height) {
            continue;
        }

        // the rows of the region one after another
        region.resize((size_t) r.width * r.height * 4);
        for (int y = 0; y < r.height; ++y) {
            memcpy(region.data() + (size_t) y * r.width * 4, shmData + (((size_t) (r.y + y) * frame.render_width) + r.x) * 4, (size_t) r.width * 4);
        }
        bytes += region.size();

        scaleAndPaint(region.data(), frame.render_width, frame.render_height, r.x, r.y, r.width, r.height, false);
    }

    uint64_t draw = drawTime.Elapsed();
    cTimeMs flushTime;

    if (osd != nullptr) {
        osd->Flush();
    }

    if (measureOsd) {
        measureFrame((int) frame.rects.size(), bytes, draw, flushTime.Elapsed());
    }

    return true;
}

bool WebOSDPage::drawImageQOI(const std::string qoibuffer, const int render_width, const int render_height, const int x, const int y) {
    // decode image data
    qoi_desc desc;
    void *image = qoi_decode(qoibuffer.c_str(), (int)qoibuffer.size(), &desc, 4);

    if (image == nullptr) {
        // something failed
        esyslog("[vdrweb] failed to decode qoi OSD image");
        return false;
    }

    bool retValue = scaleAndPaint(static_cast<uint8_t *>(image), render_width, render_height, x, y, (int)desc.width, (int)desc.height);
    free(image);

    return retValue;
}

bool WebOSDPage::scaleAndPaint(uint8_t* image, int render_width, int render_height, int x, int y, int width, int height, bool flush) {
    // sanity check
    if (width > 3840 || height > 2160 || width <= 0 || height <= 0) {
        return false;
    }

    bool scaleRequired = (disp_width != render_width) || (disp_height != render_height);

    // dsyslog("[vdrweb] RenderSize: %d x %d, DisplaySize: %d x %d, x=%d,y=%d, Scaling required: %s", render_width, render_height, disp_width, disp_height, x, y, (scaleRequired ? "yes" : "no"));

    if (!scaleRequired) {
        // dsyslog("[vdrweb] no scaling required");

        // scaling is not needed. Draw the image as is.
        const cImage recImage(cSize(width, height), (const tColor *)image);

        std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
        if (pixmap != nullptr) {
            LOCK_PIXMAPS;
            pixmap->DrawImage(cPoint(x,y), recImage);
        } else {
            if (!hidden) {
                esyslog("[vdrweb] Pixmap is null. OSD not available");
            }
        }

        if (flush && osd != nullptr) {
            osd->Flush();
        }

        return true;
    }

    // calculate scale factor
    double scalex = disp_width / (double)render_width;
    double scaley = disp_height / (double)render_height;

    // calculate coordinates
    int osd_width = (int)lround(scalex * width);
    int osd_height = (int)lround(scaley * height);

#ifdef ENABLE_FAST_SCALE
    if (useOutputDeviceScale) {
        // dsyslog("[vdrweb] scaling using outputdevice");

        const cImage recImage(cSize(width, height), (const tColor *)image);

        std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
        if (pixmap != nullptr) {
            LOCK_PIXMAPS;
            pixmap->DrawScaledImage(cPoint(x * scalex, y * scaley), recImage, scalex, scaley, true);
        } else {
            if (!hidden) {
                esyslog("[vdrweb] Pixmap is null. OSD not available");
            }
        }
    } else {
#endif // ENABLE_FAST_SCALE
        // dsyslog("[vdrweb] scaling using GraphicsMagick");

        // create image buffer for scaled image
        cSize recImageSize(osd_width, osd_height);
        const cImage recImage(recImageSize);
        auto *scaled = (uint8_t *) (recImage.Data());

        if (scaled == nullptr) {
            esyslog("[vdrweb] Out of memory reading OSD image");
            return true;
        }

#ifdef MEASURE_SCALE_TIME
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
#endif

        Magick::Image mimage;
        mimage.read(width, height, "RGBA", static_cast<const MagickLib::StorageType>(0), image);
        mimage.sample(Magick::Geometry(osd_width, osd_height));
        mimage.write ( 0, 0, osd_width, osd_height, "RGBA", static_cast<const MagickLib::StorageType>(0), scaled);

#ifdef MEASURE_SCALE_TIME
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    dsyslog("[vdrweb] Scale time = %lu [ms]", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
#endif

        std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
        if (pixmap != nullptr) {
            LOCK_PIXMAPS;
            pixmap->DrawImage(cPoint(x * scalex, y * scaley), recImage);
        } else {
            if (!hidden) {
                esyslog("[vdrweb] Pixmap is null. OSD not available");
            }
        }
#ifdef ENABLE_FAST_SCALE
    }
#endif

    if (flush && osd != nullptr) {
        osd->Flush();
    }

    return true;
}


eOSState WebOSDPage::ProcessKey(eKeys Key) {
    // check if volumebar is displayed and kill it after a timeout
    // a volume change resets the timeout
    if (pixmapVol && (time(NULL) - lastVolumeTime > 3))
        DeleteVolume();

    CheckEmptyPage(Key);
    if (closeEmptyPage) {
        return osEnd;
    }

    if (Key & k_Release) {
        return osContinue;
    }

    // repeated keys (e.g. scrolling with a held arrow key) are sent as normal keys
    eKeys key = NORMALKEY(Key);

    if (measureOsd) {
        std::lock_guard<std::mutex> lock(measureMutex);
        measureKey = key;
        measureKeyTime.Set();
    }

    // Rec: a download of the current video. Without a video VDR handles Rec itself.
    if (key == kRecord) {
        if (!(Key & k_Repeat) && currentMode == PLAYER) {
            OpenDownloads();
        }
        return osContinue;
    }

    auto search = ::keyMap.find(key);
    if (search != ::keyMap.end() && browserClient->ProcessKey(search->second)) {
        return osContinue;
    }

    // the key of the setup opens the list of the downloads, if the application doesn't use it
    if (key == webSetup.DownloadsKey() && key != kNone) {
        if (!(Key & k_Repeat)) {
            OpenDownloadList();
        }
        return osContinue;
    }

    // The application doesn't use this key (not in its keyset or not mapped).
    // Like a TV: the numeric and channel keys switch the channel. Close the page
    // and let VDR handle the key in normal viewing mode.
    bool isChannelKey = (key >= k0 && key <= k9) || key == kChanUp || key == kChanDn || key == kChanPrev;
    if (isChannelKey && !(Key & k_Repeat)) {
        dsyslog("[vdrweb] key %d not used by the application, pass it to VDR", (int)key);
        cRemote::Put(key);
        return osEnd;
    }

    return osContinue;
}

void WebOSDPage::Hide() {
    dsyslog("[vdrweb] WebOSDPage::Hide...");
}

void WebOSDPage::DrawVolume(int volume) {
    dsyslog("[vdrweb] WebOSDPage::DrawVolume...");
   std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
   if (osd == nullptr) {
      return;
   }

   if (volume != lastVolume) {
      int x = 0;           // volumebar x
      int y = 20;          // volumebar y
      int w = disp_width;  // volumebar width
      int h = 20;          // volumebar height
      if (!pixmapVol) {
          pixmapVol = osd->CreatePixmap(7, cRect(x, y, w, h));
          pixmapVol->Fill(clrTransparent);
      }
      if (volume) {
         int p = (w - 1) * volume / MAXVOLUME;
         pixmapVol->DrawRectangle(cRect(0, 0, p - 1, h - 1), clrVolumeBarLower);
         pixmapVol->DrawRectangle(cRect(p, 0, w - 1, h - 1), clrVolumeBarUpper);
      } else {
         pixmapVol->DrawRectangle(cRect(0, 0, w - 1, h - 1), clrVolumeBarUpper);
      }
      lastVolume = volume;
      // (re)set the display volumebar timeout
      lastVolumeTime = time(NULL);
      if (osd != nullptr)
         osd->Flush();
   }
}

void WebOSDPage::DeleteVolume(void) {
    dsyslog("[vdrweb] WebOSDPage::DeleteVolume...: %s", pixmapVol != nullptr ? "pixmapVol != null" : "pixmapVol == null");
    std::lock_guard<std::recursive_mutex> lock(pixmap_mutex);
    if (pixmapVol && osd != nullptr) {
        osd->DestroyPixmap(pixmapVol);
        osd->Flush();
        pixmapVol = nullptr;
    }
}

cOsdObject *WebOSDPage::GetInfo() {
    dsyslog("[vdrweb] WebOSDPage::GetInfo...");

    // VDR deletes the returned object after closing the info menu
    return nullptr;
}
