#pragma once

#include <string>
#include <map>
#include <thread>
#include <vdr/osdbase.h>
#include <vdr/player.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

enum OSD_MODE {
    OSD,
    PLAYER,
    CLOSED
};

class WebOSDPage : public cControl {

private:
    cPixmap *pixmap;
    cOsd* osd;

    int disp_width;
    int disp_height;
    bool useOutputDeviceScale;
    int browser_width;
    int browser_height;

    static std::map<int, std::string> keyMap;

    std::thread* activityTriggerThread;

    OSD_MODE currentMode;

private:
    explicit WebOSDPage(bool useOutputDeviceScale, OSD_MODE osdMode);

    bool scaleAndPaint(uint8_t* image, int render_width, int render_height, int x, int y, int width, int height, AVPixelFormat srcFormat, AVPixelFormat destFormat);

public:
    static WebOSDPage* Create(bool useOutputDeviceScale, OSD_MODE osdMode);
    static WebOSDPage* Get();
    static bool IsOpen();

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
    bool drawImage(uint8_t* image, int render_width, int render_height, int x, int y, int width, int height);
    bool drawImageQOI(const std::string& qoibuffer);
};
