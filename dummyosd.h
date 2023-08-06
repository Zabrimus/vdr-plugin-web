#pragma once

#include <vdr/osd.h>
#include "webosdpage.h"

class cDummyOsdObject : public cOsdObject {
public:
    cDummyOsdObject() = default;
    ~cDummyOsdObject() override {
        delete WebOSDPage::Get();
    }

    void Show() override {};
    eOSState ProcessKey(eKeys Key) override {
        return WebOSDPage::Get()->ProcessKey(Key);
    }
};
