#pragma once

#include <string>
#include <map>
#include <thread>
#include <vdr/osdbase.h>
#include <vdr/player.h>

class WebOSDPage : public cControl {

private:
    cPixmap *pixmap;
    cOsd* osd;

    int disp_width;
    int disp_height;

    static std::map<int, std::string> keyMap;

    std::thread* activityTriggerThread;

public:
    WebOSDPage();
    ~WebOSDPage();

    void Show() override;
    cOsdObject* GetInfo() override;
    void Display();
    void SetOsdSize();

    void Hide() override;

    bool NeedsFastResponse() override { return false; };

    eOSState ProcessKey(eKeys Key) override;
    bool drawImage(uint8_t* image, int width, int height);
};

extern WebOSDPage *webOsdPage;
