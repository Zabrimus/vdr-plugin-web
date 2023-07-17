#include "videocontrol.h"

bool isPlayerActivated;

VideoPlayer::VideoPlayer() {
    dsyslog("[vdrweb] Create Player...");
    pause = false;
}

VideoPlayer::~VideoPlayer() {
    dsyslog("[vdrweb] Delete Player...");

    pause = true;
    setVideoFullscreen();

    Detach();
}

void VideoPlayer::Activate(bool On) {
    dsyslog("[vdrweb] Activate video player: %s", On ? " Ja" : "Nein");

    if (On) {
        isPlayerActivated = true;
    } else {
        isPlayerActivated = false;
        setVideoFullscreen();
    }
}

void VideoPlayer::Pause() {
    DeviceClear();
    DeviceFreeze();
    pause = true;
}

void VideoPlayer::Resume() {
    DevicePlay();
    pause = false;
}

void VideoPlayer::setVideoFullscreen() {
    // fullscreen
    cRect r = {0,0,0,0};
    cDevice::PrimaryDevice()->ScaleVideo(r);
}

void VideoPlayer::ResetVideo() {
    DeviceClear();
}

void VideoPlayer::SetVideoSize(int x, int y, int width, int height) {
    dsyslog("[vdrweb] SetVideoSize in video player: x=%d, y=%d, width=%d, height=%d", x, y, width, height);

    int osdWidth;
    int osdHeight;
    double osdPh;
    cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, osdPh);

    int newX, newY, newWidth, newHeight;
    calcVideoPosition(x, y, width,height, &newX, &newY, &newWidth, &newHeight);

    cRect r = {newX, newY, newWidth, newHeight};
    cDevice::PrimaryDevice()->ScaleVideo(r);
}

void VideoPlayer::calcVideoPosition(int x, int y, int w, int h, int *newx, int *newy, int *newwidth, int *newheight) {
    int osdWidth;
    int osdHeight;
    double osdPh;
    cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, osdPh);

    *newx = (x * osdWidth) / 1280;
    *newy = (y * osdHeight) / 720;
    *newwidth = (w * osdWidth) / 1280;
    *newheight = (h * osdHeight) / 720;
}

void VideoPlayer::PlayPacket(uint8_t *buffer, int len) {
    if (len) { // at least one tspacket
        int result = PlayTs(buffer, len);
        if (result < 0) {
            esyslog("[vdrweb] Error playing ts: %d", result);
        }
    }
}