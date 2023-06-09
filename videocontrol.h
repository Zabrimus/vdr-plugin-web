#pragma once

#include <vdr/osd.h>
#include <vdr/player.h>
#include <vdr/tools.h>

class VideoPlayer : public cPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    void Pause();
    void Resume();

    void PlayPacket(uint8_t *buffer, int len);
    void SetVideoSize();
    void setVideoDefaultSize();

protected:
    void Activate(bool On) override;

private:
    void calcVideoPosition(int *x, int *y, int *width, int *height);
    bool isVideoFullscreen();

private:
    bool pause;

    // current video size and coordinates
    int video_x, video_y;
    int video_width, video_height;
};

/*
class VideoControl : public cControl {
public:
    VideoControl(cPlayer* Player, bool Hidden = false);
    ~VideoControl();
    void Hide() override;
    cOsdObject *GetInfo() override;
    cString GetHeader() override;
    eOSState ProcessKey(eKeys Key) override;
};
*/