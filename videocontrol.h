#pragma once

#include <vdr/osd.h>
#include <vdr/player.h>
#include <vdr/tools.h>

class VideoPlayer : public cPlayer {
public:
    explicit VideoPlayer();
    ~VideoPlayer() override;

    void Pause();
    void Resume();

    void PlayPacket(uint8_t *buffer, int len);
    static void SetVideoSize(int x, int y, int width, int height);
    static void setVideoFullscreen();

    void ResetVideo();
    void SelectAudioTrack(std::string track);

    bool hasTsError() { return tsError; };

protected:
    void Activate(bool On) override;

private:
    static void calcVideoPosition(int x, int y, int w, int h, int *newx, int *newy, int *newwidth, int *newheight);

private:
    bool pause;
    bool tsError;
    bool tsPlayed;

    static int video_x;
    static int video_y;
    static int video_width;
    static int video_height;
};

extern VideoPlayer *videoPlayer;