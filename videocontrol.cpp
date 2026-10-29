#include <chrono>
#include <thread>
#include "BrowserClient.h"
#include "videocontrol.h"

bool isPlayerActivated;

static uchar buf[188], bufsize;

VideoPlayer::VideoPlayer() {
    dsyslog("[vdrweb] Create Player...");
    pause = false;
    bufsize = 0;
    tsPlayed = false;
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
    pause = true;
    DeviceFreeze();
}

void VideoPlayer::Resume() {
    ResetVideo();
    bufsize = 0;

    DevicePlay();
    pause = false;
}

void VideoPlayer::setVideoFullscreen() {
    // fullscreen
    cDevice::PrimaryDevice()->ScaleVideo(cRect::Null);
}

void VideoPlayer::ResetVideo() {
    if (tsPlayed) {
        dsyslog("VideoPlayer::ResetVideo");
        DeviceClear();
        DevicePlay();
        tsPlayed = false;
    }

    bufsize = 0;
}

void VideoPlayer::SetVideoSize(int x, int y, int width, int height) {
    dsyslog("[vdrweb] SetVideoSize in video player: x=%d, y=%d, width=%d, height=%d", x, y, width, height);

    int newX, newY, newWidth, newHeight;
    calcVideoPosition(x, y, width,height, &newX, &newY, &newWidth, &newHeight);

    dsyslog("[vdrweb] SetVideoSize calculated: x=%d, y=%d, width=%d, height=%d", newX, newY, newWidth, newHeight);

    cRect r = {newX, newY, newWidth, newHeight};
    cRect availableRect = cDevice::PrimaryDevice()->CanScaleVideo(r);
    cDevice::PrimaryDevice()->ScaleVideo(availableRect);
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
    tsError = false;

    // if player is paused, discard all incoming packets
    if (pause) {
        // drop all packets if video is paused
        return;
    }

    if (len) { // at least one tspacket
        // play saved partial packet
        if (bufsize) {
            if (len < 188 - bufsize) {
                // still not a complete packet
                memcpy(buf + bufsize, buffer, len);
                bufsize += len;
                return;
            }

            memcpy(buf + bufsize, buffer, 188 - bufsize);
            if (!PlayData(buf, 188)) {
                bufsize = 0;
                return;
            }

            buffer += 188 - bufsize;
            len -= 188 - bufsize;
            bufsize = 0;
        }

        // save partial packet
        int rest = len % 188;
        if (rest) {
            memcpy(buf, buffer + len - rest, rest);
            len -= rest;
            bufsize = rest;
        }

        // now play packets
        PlayData(buffer, len);
    }


}

bool VideoPlayer::PlayData(uint8_t *data, int len) {
    // the device buffer may be full, wait up to 5 seconds without progress
    int waited = 0;

    while (len >= 188) {
        if (videoPlayer == nullptr) {
            return false;
        }

        int result = PlayTs(data, len);

        // play error
        if (result < 0) {
            tsError = true;
            return false;
        }

        // packets not played, retry after a short time
        if (result == 0) {
            if (waited >= 5000) {
                tsError = true;
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            waited += 5;
            continue;
        }

        // packets accepted
        len -= result;
        data += result;
        waited = 0;

        tsPlayed = true;
    }

    return true;
}

void VideoPlayer::SelectAudioTrack(std::string track) {
    // TODO: Wie wechselt man den Audiotrack?
    // printf("Anzahl der Audio-Tracks: %d\n", cDevice::PrimaryDevice()->NumAudioTracks());
}
