#pragma once

#include <iostream>
#include <mutex>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "VdrPluginWeb.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::pluginweb;

extern bool vdrClientLogThriftMessages;

class VdrClient {
public:
    VdrClient(std::string vdrIp, int vdrPort);
    ~VdrClient();

    bool ping();

    bool ProcessOsdUpdate(int disp_width, int disp_height, int x, int y, int width, int height, std::string& data);
    bool ProcessOsdUpdateQoi(int disp_width, int disp_height, int x, int y, const std::string& imageQoi);
    bool ProcessTSPacket(std::string& packets);

    bool StartVideo(std::string& videoInfo);
    bool StopVideo();
    bool Pause();
    bool Resume();
    bool ResetVideo(std::string& videoInfo);
    bool Seeked();

    bool VideoSize(int x, int y, int w, int h);
    bool VideoFullscreen();

    bool SelectAudioTrack(std::string& nr);

private:
    bool connect();
    template <typename F> bool processInternal(F &&request);

private:
    VdrPluginWebClient *client;
    std::shared_ptr<TTransport> transport;
    std::recursive_mutex vdr_send_mutex;
};