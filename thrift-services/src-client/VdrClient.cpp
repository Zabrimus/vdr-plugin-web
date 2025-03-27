#include <iostream>
#include <mutex>

#include "VdrClient.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::pluginweb;
using namespace ::common;

bool vdrClientLogThriftMessages = false;

void vdrClientOutputFunction(const char* msg) {
    if (vdrClientLogThriftMessages) {
        fprintf(stderr, "%s\n", msg);
    }
}

VdrClient::VdrClient(std::string vdrIp, int vdrPort) {
    std::shared_ptr<TTransport> socket(new TSocket(vdrIp, vdrPort));
    transport = static_cast<const std::shared_ptr<TTransport>>(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    client = new VdrPluginWebClient(protocol);

    GlobalOutput.setOutputFunction(vdrClientOutputFunction);
}

VdrClient::~VdrClient() {
    if (transport->isOpen()) {
        transport->close();
    }

    delete client;
}

bool VdrClient::connect() {
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

    if (!ping()) {
        return false;
    }

    try {
        return request();
    } catch (OperationFailed &err) {
        return false;
    } catch (TException &err) {
        transport->close();
        return false;
    }
}

bool VdrClient::ProcessOsdUpdate(int disp_width, int disp_height, int x, int y, int width, int height, std::string& data) {
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
    return processInternal([&]() -> bool {
        ProcessTSPacketType input;
        input.ts = packets;

        return client->ProcessTSPacket(input);
    });
}

bool VdrClient::StartVideo(std::string& videoInfo) {
    return processInternal([&]() -> bool {
        StartVideoType input;
        input.videoInfo = videoInfo;

        return client->StartVideo(input);
    });
}

bool VdrClient::StopVideo() {
    return processInternal([&]() -> bool {
        return client->StopVideo();
    });
}

bool VdrClient::Pause() {
    return processInternal([&]() -> bool {
        return client->PauseVideo();
    });
}

bool VdrClient::Resume() {
    return processInternal([&]() -> bool {
        return client->ResumeVideo();
    });
}

bool VdrClient::ResetVideo(std::string& videoInfo) {
    return processInternal([&]() -> bool {
        ResetVideoType input;
        input.videoInfo = videoInfo;

        return client->ResetVideo(input);
    });
}

bool VdrClient::Seeked() {
    return processInternal([&]() -> bool {
        return client->Seeked();
    });
}

bool VdrClient::VideoSize(int x, int y, int w, int h) {
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
    return processInternal([&]() -> bool {
        return client->VideoFullscreen();
    });
}

bool VdrClient::SelectAudioTrack(std::string& nr) {
    return processInternal([&]() -> bool {
        SelectAudioTrackType input;
        input.audioTrack = nr;

        return client->SelectAudioTrack(input);
    });
}