#include <iostream>
#include <mutex>
#include <utility>

#include "debuglog.h"
#include "TranscoderClient.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::remotetranscoder;
using namespace ::common;

void transcoderClientOutputFunction(const char* msg) {
    DEBUGLOG("%s\n", msg);
}

TranscoderClient::TranscoderClient(std::string vdrIp, int vdrPort) {
    DEBUGLOG("Construct TranscoderClient");

    std::shared_ptr<TTransport> socket(new TSocket(vdrIp, vdrPort));
    transport = static_cast<const std::shared_ptr<TTransport>>(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    client = new RemoteTranscoderClient(protocol);

    GlobalOutput.setOutputFunction(transcoderClientOutputFunction);
}

TranscoderClient::~TranscoderClient() {
    DEBUGLOG("Destruct TranscoderClient");

    if (transport->isOpen()) {
        transport->close();
    }

    delete client;
}

bool TranscoderClient::connect() {
    DEBUGLOG("TranscoderClient::connect");

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

bool TranscoderClient::ping() {
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

template <typename F> bool TranscoderClient::processInternal(F&& request) {
    std::lock_guard<std::recursive_mutex> lock(transcoder_send_mutex);

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

bool TranscoderClient::Probe(std::string&result,
                             const std::string &url, const std::string &cookies, const std::string &referer,
                             const std::string &userAgent, const std::string &responseIp,
                             const std::string &responsePort, const std::string &vdrIp,
                             const std::string &vdrPort, const std::string &postfix) {

    DEBUGLOG("TranscoderClient::Probe: %s", url.c_str());

    return processInternal([&]() -> bool {
        ProbeType input;
        input.url = url;
        input.cookies = cookies;
        input.referer = referer;
        input.userAgent = userAgent;
        input.responseIp = responseIp;
        input.responsePort = responsePort;
        input.vdrIp = vdrIp;
        input.vdrPort = vdrPort;
        input.postfix = postfix;

        client->Probe(result, input);
        return true;
    });
}

bool TranscoderClient::StreamUrl(const std::string &url, const std::string &cookies, const std::string &referer,
                                 const std::string &userAgent, const std::string &responseIp,
                                 const std::string &responsePort, const std::string &vdrIp, const std::string &vdrPort,
                                 const std::string &mpdStart) {

    DEBUGLOG("TranscoderClient::StreamUrl: %s", url.c_str());

    return processInternal([&]() -> bool {
        StreamUrlType input;
        input.url = url;
        input.cookies = cookies;
        input.referer = referer;
        input.userAgent = userAgent;
        input.responseIp = responseIp;
        input.responsePort = responsePort;
        input.vdrIp = vdrIp;
        input.vdrPort = vdrPort;
        input.mpdStart = mpdStart;

        return client->StreamUrl(input);
    });
}

bool TranscoderClient::Pause(const std::string &streamId) {
    DEBUGLOG("TranscoderClient::Pause");

    return processInternal([&]() -> bool {
        PauseType input;
        input.streamId = streamId;

        return client->Pause(input);
    });
}

bool TranscoderClient::SeekTo(const std::string &streamId, const std::string &seekTo) {
    DEBUGLOG("TranscoderClient::SeekTo: %s", seekTo.c_str());

    return processInternal([&]() -> bool {
        SeekToType input;
        input.streamId = streamId;
        input.seekTo = seekTo;

        return client->SeekTo(input);
    });
}

bool TranscoderClient::Resume(const std::string &streamId, const std::string& position) {
    DEBUGLOG("TranscoderClient::Resume: %s", position.c_str());

    return processInternal([&]() -> bool {
        ResumeType input;
        input.streamId = streamId;
        input.position = position;

        return client->Resume(input);
    });
}

bool TranscoderClient::Stop(const std::string& streamId, const std::string& reason) {
    DEBUGLOG("TranscoderClient::Stop: %s", reason.c_str());

    return processInternal([&]() -> bool {
        StopType input;
        input.streamId = streamId;
        input.reason = reason;

        return client->Stop(input);
    });
}

bool TranscoderClient::AudioInfo(std::string& result, const std::string& streamId) {
    DEBUGLOG("TranscoderClient::AudioInfo");

    return processInternal([&]() -> bool {
        AudioInfoType input;
        input.streamId = streamId;

        client->AudioInfo(result, input);
        return true;
    });
}

bool TranscoderClient::GetVideo(std::string &result, const VideoType &input) {
    DEBUGLOG("TranscoderClient::GetVideo");

    return processInternal([&]() -> bool {
        client->GetVideo(result, input);
        return true;
    });
}
