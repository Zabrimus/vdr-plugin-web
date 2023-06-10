#include "videocontrol.h"

bool isPlayerActivated;

VideoPlayer::VideoPlayer() {
    dsyslog("[vdrweb] Create Player...");
    pause = false;
}

VideoPlayer::~VideoPlayer() {
    dsyslog("[vdrweb] Delete Player...");

    pause = true;
    setVideoDefaultSize();

    Detach();
}

void VideoPlayer::Activate(bool On) {
    dsyslog("[vdrweb] Activate video player: %s", On ? " Ja" : "Nein");

    if (On) {
        isPlayerActivated = true;
    } else {
        isPlayerActivated = false;
        setVideoDefaultSize();

        cRect r = {0,0,0,0};
        cDevice::PrimaryDevice()->ScaleVideo(r);
    }
}

void VideoPlayer::Pause() {
    pause = true;
}

void VideoPlayer::Resume() {
    pause = false;
}

void VideoPlayer::setVideoDefaultSize() {
    video_x = 0;
    video_y = 0;
    video_width = 1280;
    video_height = 720;
}

void VideoPlayer::SetVideoSize() {
    // dsyslog("[vdrweb] SetVideoSize in video player: x=%d, y=%d, width=%d, height=%d", video_x, video_y, video_width, video_height);

    // fullscreen
    cRect r = {0,0,0,0};
    cDevice::PrimaryDevice()->ScaleVideo(r);

    // calculate the new coordinates
    /*
    if (isVideoFullscreen()) {
        // fullscreen
        cRect r = {0,0,0,0};
        cDevice::PrimaryDevice()->ScaleVideo(r);
    } else {
        int osdWidth;
        int osdHeight;
        double osdPh;
        cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, osdPh);

        int newX, newY, newWidth, newHeight;
        calcVideoPosition(&newX, &newY, &newWidth, &newHeight);

        cRect r = {newX, newY, newWidth, newHeight};
        cDevice::PrimaryDevice()->ScaleVideo(r);
    }
    */
}

void VideoPlayer::calcVideoPosition(int *x, int *y, int *width, int *height) {
    int osdWidth;
    int osdHeight;
    double osdPh;
    cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, osdPh);

    *x = (video_x * osdWidth) / 1280;
    *y = (video_y * osdHeight) / 720;
    *width = (video_width * osdWidth) / 1280;
    *height = (video_height * osdHeight) / 720;
}

bool VideoPlayer::isVideoFullscreen() {
    if ((video_x == 0) && (video_y == 0) && (video_width == 1280) && (video_height == 720)) {
        return true;
    }

    return false;
}

void VideoPlayer::PlayPacket(uint8_t *buffer, int len) {
    if (len) { // at least one tspacket
        int result = PlayTs(buffer, len);
        if (result < 0) {
            esyslog("[vdrweb] Error playing ts: %d", result);
        }
    }
}

/*
VideoControl::VideoControl(cPlayer* Player, bool Hidden) : cControl(Player, Hidden) {
    dsyslog("[vdrweb] Create Control...");
}

VideoControl::~VideoControl() {
    dsyslog("[vdrweb] Delete Control...");
    delete player;
}

void VideoControl::Hide() {
    dsyslog("[vdrweb] Hide Control...");
}

cOsdObject* VideoControl::GetInfo() {
    dsyslog("[vdrweb] GetInfo Control...");
    return nullptr;
}

cString VideoControl::GetHeader() {
    dsyslog("[vdrweb] Get Header Control...");
    return "";
}

eOSState VideoControl::ProcessKey(eKeys Key) {
    switch (int(Key)) {
        case kBack:
            // stop player mode and return to TV
            // TODO: Implement me
            return osEnd;
            break;
        default:
            // send all other keys to the browser
            // browserComm->SendKey(Key);
            return osContinue;
    }
}
 */