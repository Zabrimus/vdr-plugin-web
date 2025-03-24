#pragma once

#include <iostream>
#include <mutex>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "RemoteTranscoder.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::remotetranscoder;

extern bool transcoderClientLogThriftMessages;

class TranscoderClient {
public:
    TranscoderClient(std::string vdrIp, int vdrPort);
    ~TranscoderClient();

    bool ping();

    bool Probe(std::string&result,
               const std::string& url,
               const std::string& cookies,
               const std::string& referer,
               const std::string& userAgent,
               const std::string& responseIp,
               const std::string& responsePort,
               const std::string& vdrIp,
               const std::string& vdrPort,
               const std::string& postfix);

    bool StreamUrl(const std::string& url,
                   const std::string& cookies,
                   const std::string& referer,
                   const std::string& userAgent,
                   const std::string& responseIp,
                   const std::string& responsePort,
                   const std::string& vdrIp,
                   const std::string& vdrPort,
                   const std::string& mpdStart);


    bool Pause(const std::string& streamId);

    bool SeekTo(const std::string& streamId,
                const std::string& seekTo);

    bool Resume(const std::string& streamId,
                const std::string& position);

    bool Stop(const std::string& streamId,
              const std::string& reason);

    bool AudioInfo(std::string& result, const std::string& streamId);

    bool GetVideo(std::string& result, const VideoType& input);

private:
    bool connect();
    template <typename F> bool processInternal(F &&request);

private:
    RemoteTranscoderClient *client;
    std::shared_ptr<TTransport> transport;
    std::recursive_mutex transcoder_send_mutex;
};
