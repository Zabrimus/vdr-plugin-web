#include <iostream>
#include <mutex>

#include "debuglog.h"
#include "VdrClient.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::pluginweb;
using namespace ::common;


void vdrClientOutputFunction(const char* msg) {
    DEBUGLOG("%s\n", msg);
}

VdrClient::VdrClient(std::string vdrIp, int vdrPort) {
    DEBUGLOG("Construct VdrClient");

    std::shared_ptr<TSocket> socket(new TSocket(vdrIp, vdrPort));
    socket->setConnTimeout(3000);
    socket->setSendTimeout(10000);
    socket->setRecvTimeout(10000);   // ProcessTSPacket may wait up to 5s for the device
    transport = static_cast<const std::shared_ptr<TTransport>>(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    client = new VdrPluginWebClient(protocol);

    TOutput::instance().setOutputFunction(vdrClientOutputFunction);
}

VdrClient::~VdrClient() {
    DEBUGLOG("Destruct VdrClient");

    if (transport->isOpen()) {
        transport->close();
    }

    delete client;
}

bool VdrClient::connect() {
    // DEBUGLOG("VdrClient::connect");

    if (!transport->isOpen()) {
        // connection closed. Try to connect
        try {
            transport->open();
        } catch (TTransportException& err) {
            // connection not possible
            return false;
        }
    }

    return true;
}

bool VdrClient::ping() {
    if (!connect()) {
        return false;
    }

    try {
        client->ping();
        return true;
    } catch (OperationFailed &err) {
        return false;
    } catch (TException &err) {
        transport->close();
        return false;
    }
}

template <typename F> bool VdrClient::processInternal(F&& request) {
    std::lock_guard<std::recursive_mutex> lock(vdr_send_mutex);

    // no ping before every request, that doubles the round trips.
    // If the connection is broken, reconnect once and repeat the request.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!connect()) {
            return false;
        }

        try {
            return request();
        } catch (OperationFailed &err) {
            return false;
        } catch (TTransportException &err) {
            transport->close();
            if (err.getType() == TTransportException::TIMED_OUT) {
                // the request may have been processed, don't repeat it
                return false;
            }
        } catch (TException &err) {
            transport->close();
            return false;
        }
    }

    return false;
}

bool VdrClient::ProcessOsdUpdate(int disp_width, int disp_height, int x, int y, int width, int height, std::string& data) {
    // DEBUGLOG("VdrClient::ProcessOsdUpdate");

    return processInternal([&]() -> bool {
        ProcessOsdUpdateType input;
        input.disp_width = disp_width;
        input.disp_height = disp_height;
        input.x = x;
        input.y = y;
        input.width = width;
        input.height = height;
        input.data = data;

        return client->ProcessOsdUpdate(input);
    });
}

bool VdrClient::ProcessOsdUpdateQoi(int disp_width, int disp_height, int x, int y, const std::string &imageQoi) {
    // DEBUGLOG("VdrClient::ProcessOsdUpdateQoi");

    return processInternal([&]() -> bool {
        ProcessOsdUpdateQOIType input;
        input.x = x;
        input.y = y;
        input.render_width = disp_width;
        input.render_height = disp_height;
        input.image_data = imageQoi;

        return client->ProcessOsdUpdateQOI(input);
    });
}

bool VdrClient::ProcessTSPacket(std::string& packets) {
    // DEBUGLOG("VdrClient::ProcessTSPacket");

    return processInternal([&]() -> bool {
        ProcessTSPacketType input;
        input.ts = packets;

        return client->ProcessTSPacket(input);
    });
}

bool VdrClient::StartVideo(std::string& videoInfo) {
    DEBUGLOG("VdrClient::StartVideo");

    return processInternal([&]() -> bool {
        StartVideoType input;
        input.videoInfo = videoInfo;

        return client->StartVideo(input);
    });
}

bool VdrClient::StopVideo() {
    DEBUGLOG("VdrClient::StopVideo");

    return processInternal([&]() -> bool {
        return client->StopVideo();
    });
}

bool VdrClient::Pause() {
    DEBUGLOG("VdrClient::Pause");

    return processInternal([&]() -> bool {
        return client->PauseVideo();
    });
}

bool VdrClient::Resume() {
    DEBUGLOG("VdrClient::Resume");

    return processInternal([&]() -> bool {
        return client->ResumeVideo();
    });
}

bool VdrClient::ResetVideo(std::string& videoInfo) {
    DEBUGLOG("VdrClient::ResetVideo");

    return processInternal([&]() -> bool {
        ResetVideoType input;
        input.videoInfo = videoInfo;

        return client->ResetVideo(input);
    });
}

bool VdrClient::Seeked() {
    DEBUGLOG("VdrClient::Seeked");

    return processInternal([&]() -> bool {
        return client->Seeked();
    });
}

bool VdrClient::VideoSize(int x, int y, int w, int h) {
    DEBUGLOG("VdrClient::VideoSize");

    return processInternal([&]() -> bool {
        VideoSizeType input;
        input.x = x;
        input.y = y;
        input.w = w;
        input.h = h;

        return client->VideoSize(input);
    });
}

bool VdrClient::VideoFullscreen() {
    DEBUGLOG("VdrClient::VideoFullscreen");

    return processInternal([&]() -> bool {
        return client->VideoFullscreen();
    });
}

bool VdrClient::SelectAudioTrack(std::string& nr) {
    DEBUGLOG("VdrClient::SelectAudioTrack");

    return processInternal([&]() -> bool {
        SelectAudioTrackType input;
        input.audioTrack = nr;

        return client->SelectAudioTrack(input);
    });
}

bool VdrClient::ProcessOsdFrame(int render_width, int render_height, bool qoi, const std::vector<OsdRectType>& rects) {
    return processInternal([&]() -> bool {
        ProcessOsdFrameType input;
        input.render_width = render_width;
        input.render_height = render_height;
        input.qoi = qoi;
        input.rects = rects;

        return client->ProcessOsdFrame(input);
    });
}

bool VdrClient::SelectSubtitleTrack(const std::string& nr) {
    DEBUGLOG("VdrClient::SelectSubtitleTrack");

    return processInternal([&]() -> bool {
        SelectSubtitleTrackType input;
        input.subtitleTrack = nr;

        return client->SelectSubtitleTrack(input);
    });
}

bool VdrClient::EndOfStream() {
    DEBUGLOG("VdrClient::EndOfStream");

    return processInternal([&]() -> bool {
        return client->EndOfStream();
    });
}

bool VdrClient::StreamEventFilter(int componentTag, bool enable) {
    DEBUGLOG("VdrClient::StreamEventFilter");

    return processInternal([&]() -> bool {
        StreamEventFilterType input;
        input.componentTag = componentTag;
        input.enable = enable;

        return client->StreamEventFilter(input);
    });
}

bool VdrClient::IsWebActive() {
    // DEBUGLOG("VdrClient::IsWebActive");

    return processInternal([&]() -> bool {
        return client->IsWebActive();
    });
}
