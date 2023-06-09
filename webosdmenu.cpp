#include "webosdmenu.h"
#include "osddelegate.h"
#include "browserclient.h"

WebOSDMenu::WebOSDMenu(const char *title) : cOsdMenu(title) {
    cOsdMenu::Add(new cOsdItem("Red Button"));
    cOsdMenu::Add(new cOsdItem("Spiegel"));
    cOsdMenu::Add(new cOsdItem("Youtube"));
    cOsdMenu::Add(new cOsdItem("ARD"));
    cOsdMenu::Add(new cOsdItem("HTML5 Test"));
    cOsdMenu::Add(new cOsdItem("ARD HbbTV"));
    cOsdMenu::Add(new cOsdItem("HbbTV Test"));

    SetHelp(0, 0, 0,0);
}

WebOSDMenu::~WebOSDMenu() {
}

eOSState WebOSDMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

#if APIVERSNUM >= 20301
    LOCK_CHANNELS_READ
    const cChannel* currentChannel = Channels->GetByNumber(cDevice::CurrentChannel());
#else
    const cChannel* currentChannel = Channels->GetByNumber(cDevice::CurrentChannel());
#endif

    switch (key) {
        case kOk: {
            switch (Current()) {
                case 0: // Red Button
                    browserClient->RedButton(*currentChannel->GetChannelID().ToString());
                    break;

                case 1: // Spiegel
                    browserClient->LoadUrl("https://www.spiegel.de");
                    break;

                case 2: // Youtube
                    browserClient->LoadUrl("https://www.youtube.com");
                    break;

                case 3: // ARD
                    browserClient->LoadUrl("https://www.ardmediathek.de/");
                    break;

                case 4: // HTML5 Test
                    browserClient->LoadUrl("https://html5test.com/");
                    break;

                case 5: // ARD HbbTV
                    browserClient->LoadUrl("http://tv.ardmediathek.de/index.html");
                    break;

                case 6: // HbbTV Test
                    browserClient->LoadUrl("http://itv.mit-xperts.com/hbbtvtest/index.php");
                    break;

                default:
                    return osEnd;
            }

            OSDDelegate::osdType = HTML;
            return osPlugin;
        }
    }

    return state;
}
