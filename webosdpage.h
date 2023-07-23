#pragma once

#include <string>
#include <map>
#include <thread>
#include <vdr/osdbase.h>
#include <vdr/player.h>
#include "lancir.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

class WebOSDPage : public cControl {

private:
    cPixmap *pixmap;
    cOsd* osd;
    static avir::CLancIR ImageResizer;

    int disp_width;
    int disp_height;

    static std::map<int, std::string> keyMap;

    std::thread* activityTriggerThread;

private:
    bool scaleAndPaint1(uint8_t* image, int width, int height, AVPixelFormat srcFormat, AVPixelFormat destFormat);
    bool scaleAndPaint2(uint8_t* image, int width, int height, AVPixelFormat srcFormat, AVPixelFormat destFormat);

public:
    WebOSDPage();
    ~WebOSDPage() override;

    void Show() override;
    cOsdObject* GetInfo() override;
    void Display();
    void SetOsdSize();

    void Hide() override;

    bool NeedsFastResponse() override { return false; };

    eOSState ProcessKey(eKeys Key) override;

    // Only one method shall be used.
    // Using both methods in parallel leads to undefined behaviour.
    bool drawImage(uint8_t* image, int width, int height);
    bool drawImageQOI(const std::string& qoibuffer);
    bool drawImageQOIR(const std::string& qoibuffer);
};

extern WebOSDPage *webOsdPage;
