/*
 * hbbtvhint.h: shows "HbbTV available" after a channel switch,
 *              if the channel signals an HbbTV autostart application
 *
 * See the README file for copyright information and how to reach the author.
 */

#pragma once

#include <vdr/osd.h>
#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/menuitems.h>

struct cHbbtvHintSetup {
    int enabled = 1;    // show the hint at all
    int duration = 7;   // seconds
    int position = 0;   // 0 = bottom right, 1 = bottom left, 2 = top right, 3 = top left
};

extern cHbbtvHintSetup hbbtvHintSetup;

class cHbbtvHint {
private:
    static cMutex mutex;
    static bool pending;          // an autostart application has been found after the last switch
    static bool shown;            // the hint has been shown for the current channel
    static bool hideRequest;
    static cTimeMs pendingTimeout;
    static cTimeMs showTimeout;
    static cOsd *osd;

    static void Draw();
    static void Close();

public:
    // called by cStatus::ChannelSwitch
    static void ChannelSwitch();

    // called by the AIT filter
    static void AutostartApplicationFound();

    // e.g. the user opened the web page
    static void Hide();

    // must be called from the main thread (MainThreadHook)
    static void Process();
};

class cMenuSetupWeb : public cMenuSetupPage {
private:
    cHbbtvHintSetup data;
    const char *positions[4];

protected:
    void Store() override;

public:
    cMenuSetupWeb();
};
