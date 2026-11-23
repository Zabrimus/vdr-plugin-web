/*
 * hbbtvhint.cpp: shows "HbbTV available" after a channel switch,
 *                if the channel signals an HbbTV autostart application
 *
 * See the README file for copyright information and how to reach the author.
 */

#include <vdr/player.h>
#include <vdr/i18n.h>
#include "hbbtvhint.h"
#include "webosdpage.h"
#include "videocontrol.h"

// above the subtitles, VDR menus (level 0) are not affected
#define HBBTVHINT_OSD_LEVEL  (OSD_LEVEL_SUBTITLES + 1)

// how long to wait for a possibility to show the hint after the channel switch, e.g.
// while the channel display of VDR is open
#define HBBTVHINT_PENDING_MS 15000

cHbbtvHintSetup hbbtvHintSetup;

cMutex cHbbtvHint::mutex;
bool cHbbtvHint::pending = false;
bool cHbbtvHint::shown = false;
bool cHbbtvHint::hideRequest = false;
cTimeMs cHbbtvHint::pendingTimeout;
cTimeMs cHbbtvHint::showTimeout;
cOsd *cHbbtvHint::osd = nullptr;

void cHbbtvHint::ChannelSwitch() {
    cMutexLock lock(&mutex);
    pending = false;
    shown = false;
    hideRequest = true;
}

void cHbbtvHint::AutostartApplicationFound() {
    cMutexLock lock(&mutex);

    // the AIT is repeated, show the hint only once per channel switch
    if (!hbbtvHintSetup.enabled || shown || pending) {
        return;
    }

    pending = true;
    pendingTimeout.Set(HBBTVHINT_PENDING_MS);
}

void cHbbtvHint::Hide() {
    cMutexLock lock(&mutex);
    pending = false;
    hideRequest = true;
}

void cHbbtvHint::Close() {
    if (osd != nullptr) {
        delete osd;
        osd = nullptr;
    }
}

// true if something else uses the screen: a VDR menu, the web page, a replay or a video
static bool screenBusy() {
    if (cOsd::IsOpen() || WebOSDPage::IsOpen() || videoPlayer != nullptr) {
        return true;
    }

    // Only a visible control (e.g. a replay) uses the screen. Plugins may keep a hidden
    // control running during live TV.
    cMutexLock controlLock;
    return cControl::Control(controlLock, false) != nullptr;
}

void cHbbtvHint::Draw() {
    const cFont *font = cFont::GetFont(fontOsd);

    const char *text = tr("HbbTV available");

    int lineHeight = font->Height();
    int padding = lineHeight / 2;
    int width = padding + font->Width(text) + padding;
    int height = lineHeight + padding;

    // position inside the OSD area with a small margin
    int margin = lineHeight;
    int left = cOsd::OsdLeft() + margin;
    int top = cOsd::OsdTop() + margin;
    int right = cOsd::OsdLeft() + cOsd::OsdWidth() - margin - width;
    int bottom = cOsd::OsdTop() + cOsd::OsdHeight() - margin - height;

    int x = (hbbtvHintSetup.position == 0 || hbbtvHintSetup.position == 2) ? right : left;
    int y = (hbbtvHintSetup.position <= 1) ? bottom : top;

    osd = cOsdProvider::NewOsd(x, y, HBBTVHINT_OSD_LEVEL);
    if (osd == nullptr) {
        return;
    }

    tArea area = { 0, 0, width - 1, height - 1, 32 };
    if (osd->CanHandleAreas(&area, 1) != oeOk) {
        area.bpp = 8;
    }

    if (osd->SetAreas(&area, 1) != oeOk) {
        esyslog("[vdrweb] HbbTV hint: unable to set the OSD area");
        Close();
        return;
    }

    const tColor clrBack = 0xD0202020;
    int radius = height / 2;

    // background with rounded ends
    osd->DrawRectangle(0, 0, width - 1, height - 1, clrTransparent);
    osd->DrawRectangle(radius, 0, width - 1 - radius, height - 1, clrBack);
    osd->DrawEllipse(0, 0, radius - 1, height - 1, clrBack, 7);                       // left half
    osd->DrawEllipse(width - radius, 0, width - 1, height - 1, clrBack, 5);           // right half

    // the text
    osd->DrawText(padding, padding / 2, text, clrWhite, clrBack, font,
                  font->Width(text), lineHeight, taLeft);

    osd->Flush();
}

void cHbbtvHint::Process() {
    cMutexLock lock(&mutex);

    if (osd != nullptr) {
        // hide after the timeout, on a channel switch, if the hint is disabled meanwhile
        // or if anything else (menu, web page, replay) is opened
        if (hideRequest || !hbbtvHintSetup.enabled || showTimeout.TimedOut() || screenBusy()) {
            Close();
        }
    }

    hideRequest = false;

    if (!pending) {
        return;
    }

    if (!hbbtvHintSetup.enabled || pendingTimeout.TimedOut()) {
        pending = false;
        return;
    }

    if (osd == nullptr && !screenBusy()) {
        pending = false;
        shown = true;

        Draw();
        showTimeout.Set(hbbtvHintSetup.duration * 1000);
    }
}
