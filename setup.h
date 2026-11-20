/*
 * setup.h: setup of the web plugin
 *
 * See the README file for copyright information and how to reach the author.
 */

#pragma once

#include <vdr/menuitems.h>
#include "hbbtvhint.h"

struct cWebSetup {
    int resolution = 0;    // width of the page: 0 = browser default, 1 = 1280, 2 = 1920, 3 = 2560, 4 = 3840
    int deviceScale = -1;  // scale by the output device, -1 = command line (-f)
    int frameRate = 0;     // 0 = browser default, 1 = 25, 2 = 30, 3 = 50, 4 = 60
    int mse = 1;           // Media Source Extensions (dash.js, hls.js, ...)
    int zdfQuirks = 1;     // special handling of the ZDF applications
    int osdSharedMemory = 0;  // the OSD in shared memory, the browser on the same machine
    int downloadRate = 0;  // Mbit/s, 0 = unlimited
    int downloadQuality = 0;  // 0 = best, 1 = 720p at most, 2 = SD
    int downloadReserve = 5;  // GB kept free on the disk
    int downloadParallel = 2; // downloads at the same time
    int downloadRecording = 0; // a recording of VDR: 0 = manually, 1 = automatically
    int recordingLifetime = 99; // lifetime of these recordings
    int downloadsKey = 1;  // opens the list of the downloads: 0 = none, 1 = Info, 2 = Stop, 3 = Blue, 4 - 12 = User1 - User9
    int autoClose = 0;     // s without a visible page and a key until HbbTV is closed, 0 = never

    int Width() const;
    int FrameRate() const;
    int DownloadMaxHeight() const;  // 0 = the best
    eKeys DownloadsKey() const;     // kNone = none
};

extern cWebSetup webSetup;

// tell the browser resolution, frame rate and options
void SendRenderParameters();

class cMenuSetupWeb : public cMenuSetupPage {
private:
    cHbbtvHintSetup hint;
    cWebSetup web;
    const char *positions[4];
    const char *resolutions[5];
    const char *frameRates[5];
    const char *qualities[3];
    const char *recordingModes[2];
    const char *downloadsKeys[13];
    cOsdItem *downloadsItem;

protected:
    void Store() override;

public:
    cMenuSetupWeb();

    eOSState ProcessKey(eKeys Key) override;
};
