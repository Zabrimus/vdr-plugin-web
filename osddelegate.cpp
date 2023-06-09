#include <mutex>
#include "osddelegate.h"
#include "webosdmenu.h"
#include "webosdpage.h"

OSDType OSDDelegate::osdType = OSDType::MENU;

OSDDelegate::OSDDelegate() {
}

cOsdObject *OSDDelegate::get(const char *title) {
    dsyslog("[vdrweb] OSDDelegate called with type %d", osdType);

    if (osdType == MENU) {
        dsyslog("[vdrweb] OSDDelegate: Construct new Menu");

        return new WebOSDMenu(title);
    } else if (osdType == HTML) {
        dsyslog("[vdrweb] OSDDelegate: Construct new Page");

        OSDDelegate::osdType = OSDType::MENU;

        return new WebOSDPage();
    } else if (osdType == CLOSE) {
        dsyslog("[vdrweb] OSDDelegate: Close");

        // close OSD
        OSDDelegate::osdType = OSDType::MENU;
        return nullptr;
    } else if (osdType == OPEN_VIDEO) {
        fprintf(stderr, "TRY OPENVIDEO....: %s\n", (webOsdPage != nullptr ? "vorhanden" : "nicht vorhanden"));
        if (webOsdPage == nullptr) {
            new WebOSDPage();
        }

        return webOsdPage;
    }

    return nullptr;
}
