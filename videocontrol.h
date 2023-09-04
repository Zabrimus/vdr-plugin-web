#pragma once

#include <vdr/osd.h>
#include <vdr/player.h>
#include <vdr/tools.h>

class VideoPlayer : public cPlayer {
public:
    explicit VideoPlayer(std::string vi);
    ~VideoPlayer() override;

    void Pause();
    void Resume();

    void PlayPacket(uint8_t *buffer, int len);
    void SetVideoSize(int x, int y, int width, int height);
    void setVideoFullscreen();

    void ResetVideo(std::string string);

    bool hasTsError() { return tsError; };

protected:
    void Activate(bool On) override;

private:
    void calcVideoPosition(int x, int y, int w, int h, int *newx, int *newy, int *newwidth, int *newheight);

private:
    bool pause;
    bool tsError;

    std::string videoInfo;
};
