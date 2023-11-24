#include <chrono>
#include <thread>
#include "browserclient.h"
#include "videocontrol.h"

bool isPlayerActivated;

static uchar buf[188], bufsize;

uint8_t pausePacketBuffer[64 * 1024];
int pausePacketBufferIdx;

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
    pausePacketBufferIdx = 0;
    DeviceFreeze();
}

void VideoPlayer::Resume() {
    DevicePlay();
    pause = false;

    if (pausePacketBufferIdx > 0) {
        dsyslog("[vdrweb] Play pause buffer of length %d", pausePacketBufferIdx);
        // play at first the saved packets
        PlayPacket(pausePacketBuffer, pausePacketBufferIdx);
        pausePacketBufferIdx = 0;
    }
}

void VideoPlayer::setVideoFullscreen() {
    // fullscreen
    cDevice::PrimaryDevice()->ScaleVideo(cRect::Null);
}

void VideoPlayer::ResetVideo() {
    if (tsPlayed) {
        DeviceClear();
        tsPlayed = false;
    }

    bufsize = 0;
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
        dsyslog("[vdrweb] Video paused, drop TS packets with len %d", len);
        if (len + pausePacketBufferIdx < 64 * 1024) {
            memcpy(pausePacketBuffer + pausePacketBufferIdx, buffer, len);
            pausePacketBufferIdx += len;
        } else {
            dsyslog("[vdrweb] unable to fill pause buffer, length is too large %d", len + pausePacketBufferIdx);
        }

        return;
    }

    if (len) { // at least one tspacket
        // play saved partial packet
        if (bufsize) {
            memcpy(buf + bufsize, buffer, 188 - bufsize);
            PlayTs(buf, 188);
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
        int retry_loop_count = 0;
        while (len >= 188) {
            int result = PlayTs(buffer, len);

            // play error.
            if (result < 0) {
                tsError = true;
                return;
            }

            // packets not played
            if (result == 0) {
                // retry after some time, but increase retry_loop_count
                if (retry_loop_count >= 10) {
                    tsError = true;
                    return;
                } else {
                    retry_loop_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    continue;
                }
            }

            // packets accepted
            len -= result;
            buffer += result;

            tsPlayed = true;
        }
    }
}
