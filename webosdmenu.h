#pragma once

#include <vdr/osd.h>
#include <vdr/menu.h>
#include <vdr/tools.h>

class WebOSDMenu : public cOsdMenu {
public:
    explicit WebOSDMenu(const char * title);
    ~WebOSDMenu() override;
    eOSState ProcessKey(eKeys key) override;
};
