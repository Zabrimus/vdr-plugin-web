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
        auto osd = WebOSDPage::Get();
        if (osd != nullptr) {
            return osd->ProcessKey(Key);
        } else {
            return osContinue;
        }
    }
};
