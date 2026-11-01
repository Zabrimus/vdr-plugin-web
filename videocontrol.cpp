#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include "BrowserClient.h"
#include "videocontrol.h"
#include <vdr/remux.h>

bool isPlayerActivated;

static uchar buf[188], bufsize;

VideoPlayer::VideoPlayer() {
    dsyslog("[vdrweb] Create Player...");
    pause = false;
    mse = false;
    bufsize = 0;
    tsPlayed = false;
    ResetStatus();
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
    // Only after a pause a new stream follows. Otherwise (e.g. unfreeze while
    // playing) clearing the device would throw away the running stream.
    // A MSE stream continues after the pause.
    if (pause && !mse) {
        ResetVideo();
        bufsize = 0;
    }

    DevicePlay();
    pause = false;
}

void VideoPlayer::setVideoFullscreen() {
    // fullscreen
    cDevice::PrimaryDevice()->ScaleVideo(cRect::Null);
}

void VideoPlayer::ResetStatus() {
    firstPts = -1;
    lastPts = -1;
    endOfStream = false;
    lastStc = -1;
    stcStall.Set();
    lastData.Set();
}

void VideoPlayer::SetEndOfStream() {
    dsyslog("[vdrweb] end of stream");
    endOfStream = true;
}

// remember the first and the last PTS of the video stream
void VideoPlayer::ScanPts(const uint8_t *data, int len) {
    for (; len >= TS_SIZE; data += TS_SIZE, len -= TS_SIZE) {
        if (data[0] != TS_SYNC_BYTE || !TsHasPayload(data) || !TsPayloadStart(data)) {
            continue;
        }

        const uchar *payload = data;
        int length = TsGetPayload(&payload);

        // PES of a video stream (stream id 0xE0 - 0xEF)
        if (length >= 14 && payload[0] == 0 && payload[1] == 0 && payload[2] == 1 &&
            (payload[3] & 0xF0) == 0xE0 && PesHasPts(payload)) {
            lastPts = PesGetPts(payload);
            if (firstPts < 0) {
                firstPts = lastPts;
            }
        }
    }

    lastData.Set();
}

void VideoPlayer::GetStatus(int64_t &position, int &state) {
    int64_t stc = (int64_t) DeviceGetSTC();
    bool stcValid = stc >= 0 && stc != (int64_t) -1 && firstPts >= 0;

    if (stcValid && stc != lastStc) {
        lastStc = stc;
        stcStall.Set();
    }

    position = -1;
    if (stcValid) {
        int64_t diff = PtsDiff(firstPts, stc);
        // ignore an STC which doesn't belong to this stream yet
        if (diff >= 0 && diff < 24LL * 3600 * PTSTICKS) {
            position = diff / (PTSTICKS / 1000);
        }
    }

    if (pause) {
        state = STATE_PAUSED;
    } else if (endOfStream && (stcValid ? stcStall.Elapsed() >= 1000 : lastData.Elapsed() >= 3000)) {
        // the decoder has shown the last picture
        state = STATE_ENDED;
    } else if (!endOfStream && stcValid && stcStall.Elapsed() >= 1500) {
        // waiting for data
        state = STATE_BUFFERING;
    } else {
        state = STATE_PLAYING;
    }
}

void VideoPlayer::ResetVideo() {
    if (tsPlayed) {
        dsyslog("VideoPlayer::ResetVideo");
        DeviceClear();
        DevicePlay();
        tsPlayed = false;
    }

    bufsize = 0;

    // a new stream starts
    ResetStatus();
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
    if (pause && !mse) {
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
    ScanPts(data, len);

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

// Select a track by its index in the list of audio (subtitle) tracks ordered by PID.
// The transcoder and the channel list (live TV) report the tracks in this order.
// Works for the web video player and for live TV, the tracks belong to the device.
bool VideoPlayer::SelectTrack(bool subtitle, int index) {
    cDevice *device = cDevice::PrimaryDevice();

    if (subtitle && index < 0) {
        return device->SetCurrentSubtitleTrack(ttNone, true);
    }

    std::vector<std::pair<uint16_t, eTrackType>> tracks;

    if (subtitle) {
        for (int t = ttSubtitleFirst; t <= ttSubtitleLast; ++t) {
            const tTrackId *track = device->GetTrack(eTrackType(t));
            if (track && track->id) {
                tracks.emplace_back(track->id, eTrackType(t));
            }
        }
    } else {
        for (int t = ttAudioFirst; t <= ttDolbyLast; ++t) {
            const tTrackId *track = device->GetTrack(eTrackType(t));
            if (track && track->id) {
                tracks.emplace_back(track->id, eTrackType(t));
            }
        }
    }

    std::sort(tracks.begin(), tracks.end());

    if (index < 0 || index >= (int)tracks.size()) {
        esyslog("[vdrweb] %s track %d not available (%d tracks)", subtitle ? "subtitle" : "audio", index, (int)tracks.size());
        return false;
    }

    dsyslog("[vdrweb] select %s track %d, pid %d", subtitle ? "subtitle" : "audio", index, tracks[index].first);

    if (subtitle) {
        return device->SetCurrentSubtitleTrack(tracks[index].second, true);
    }

    return device->SetCurrentAudioTrack(tracks[index].second);
}
