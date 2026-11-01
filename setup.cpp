/*
 * setup.cpp: setup of the web plugin
 *
 * See the README file for copyright information and how to reach the author.
 */

#include <vdr/i18n.h>
#include "setup.h"
#include "web.h"

cWebSetup webSetup;

extern bool useOutputDeviceScale;

int cWebSetup::Width() const {
    static const int widths[] = { 0, 1280, 1920, 2560, 3840 };
    return (resolution >= 0 && resolution <= 4) ? widths[resolution] : 0;
}

int cWebSetup::FrameRate() const {
    static const int rates[] = { 0, 25, 30, 50, 60 };
    return (frameRate >= 0 && frameRate <= 4) ? rates[frameRate] : 0;
}

void SendRenderParameters() {
    if (browserClient != nullptr && (webSetup.Width() > 0 || webSetup.FrameRate() > 0)) {
        browserClient->SetRenderParameters(webSetup.Width(), webSetup.FrameRate());
    }
}

cMenuSetupWeb::cMenuSetupWeb() {
    hint = hbbtvHintSetup;
    web = webSetup;

    positions[0] = tr("bottom right");
    positions[1] = tr("bottom left");
    positions[2] = tr("top right");
    positions[3] = tr("top left");

    resolutions[0] = tr("browser default");
    resolutions[1] = "1280x720";
    resolutions[2] = "1920x1080";
    resolutions[3] = "2560x1440";
    resolutions[4] = "3840x2160";

    frameRates[0] = tr("browser default");
    frameRates[1] = "25";
    frameRates[2] = "30";
    frameRates[3] = "50";
    frameRates[4] = "60";

    Add(new cMenuEditBoolItem(tr("Show HbbTV hint"), &hint.enabled));
    Add(new cMenuEditIntItem(tr("HbbTV hint duration (s)"), &hint.duration, 1, 60));
    Add(new cMenuEditStraItem(tr("HbbTV hint position"), &hint.position, 4, positions));

    Add(new cMenuEditStraItem(tr("Browser resolution"), &web.resolution, 5, resolutions));
#ifdef ENABLE_FAST_SCALE
    Add(new cMenuEditBoolItem(tr("Scale by output device"), &web.deviceScale));
#endif
    Add(new cMenuEditStraItem(tr("Browser frame rate"), &web.frameRate, 5, frameRates));
}

void cMenuSetupWeb::Store() {
    hbbtvHintSetup = hint;
    webSetup = web;
    useOutputDeviceScale = webSetup.deviceScale != 0;

    SetupStore("HbbtvHint", hbbtvHintSetup.enabled);
    SetupStore("HbbtvHintDuration", hbbtvHintSetup.duration);
    SetupStore("HbbtvHintPosition", hbbtvHintSetup.position);

    SetupStore("Resolution", webSetup.resolution);
    SetupStore("DeviceScale", webSetup.deviceScale);
    SetupStore("FrameRate", webSetup.frameRate);

    SendRenderParameters();
}
