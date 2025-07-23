#include <memory>
#include <mutex>
#include <sys/socket.h>

#include "debuglog.h"
#include "BrowserClient.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::cefbrowser;
using namespace ::common;

void browserClientOutputFunction(const char* msg) {
    DEBUGLOG("%s", msg);
}

BrowserClient::BrowserClient(std::string vdrIp, int vdrPort) {
    DEBUGLOG("Construct BrowserClient");

    socket = std::make_shared<TSocket>(vdrIp, vdrPort);
    transport = static_cast<const std::shared_ptr<TTransport>>(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    client = new CefBrowserClient(protocol);

    GlobalOutput.setOutputFunction(browserClientOutputFunction);
}

BrowserClient::~BrowserClient() {
    DEBUGLOG("Destruct BrowserClient");

    CefBrowserClient *clientOld = client;
    client = nullptr;

    if (transport && transport->isOpen()) {
        transport->close();
        transport = nullptr;
    }

    delete clientOld;
}

bool BrowserClient::connect() {
    // DEBUGLOG("BrowserClient::connect");

    if (transport == nullptr) {
        return false;
    }

    if (transport && !transport->isOpen()) {
        // connection closed. Try to connect
        try {
            transport->open();
        } catch (TTransportException& err) {
            // connection not possible
            return false;
        }
    }

    if (socket && !socket->isOpen()) {
        return false;
    }

    int error_code;
    socklen_t error_code_size = sizeof(error_code);
    getsockopt(socket->getSocketFD(), SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);

    if (error_code < 0) {
        return false;
    }

    return true;
}

bool BrowserClient::ping() {
    // DEBUGLOG("BrowserClient::ping");

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

template <typename F> bool BrowserClient::processInternal(F&& request) {
    std::lock_guard<std::recursive_mutex> lock(browser_send_mutex);

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

bool BrowserClient::LoadUrl(const std::string& url) {
    DEBUGLOG("BrowserClient::LoadUrl: %s", url.c_str());

    return processInternal([&]() -> bool {
        LoadUrlType input;
        input.url = url;

        return client->LoadUrl(input);
    });
}

bool BrowserClient::RedButton(const std::string& channelId) {
    DEBUGLOG("BrowserClient::RedButton");

    return processInternal([&]() -> bool {
        RedButtonType input;
        input.channelId = channelId;

        return client->RedButton(input);
    });
}

bool BrowserClient::ReloadOSD() {
    DEBUGLOG("BrowserClient::ReloadOSD");

    return processInternal([&]() -> bool {
        return client->ReloadOSD();
    });
}

bool BrowserClient::StartApplication(const std::string& channelId, const std::string& appId, const std::string& appCookie, const std::string& appReferrer, const std::string& appUserAgent, const std::string& url) {
    DEBUGLOG("BrowserClient::StartApplication");

    return processInternal([&]() -> bool {
        StartApplicationType input;
        input.channelId = channelId;
        input.appId = appId;
        input.appCookie = appCookie;
        input.appReferrer = appReferrer;
        input.appUserAgent = appUserAgent;
        input.url = url;

        if (!url.empty()) {
            input.__isset.url = true;
        }

        return client->StartApplication(input);
    });
}

bool BrowserClient::ProcessKey(const std::string& key) {
    DEBUGLOG("BrowserClient::ProcessKey: %s", key.c_str());

    return processInternal([&]() -> bool {
        ProcessKeyType input;
        input.key = key;

        return client->ProcessKey(input);
    });
}

bool BrowserClient::StreamError(const std::string& reason) {
    DEBUGLOG("BrowserClient::StreamError");

    return processInternal([&]() -> bool {
        StreamErrorType input;
        input.reason = reason;

        return client->StreamError(input);
    });
}

bool BrowserClient::InsertHbbtv(const std::string& hbbtv) {
    DEBUGLOG("BrowserClient::InsertHbbtv: %s", hbbtv.c_str());

    return processInternal([&]() -> bool {
        InsertHbbtvType input;
        input.hbbtv = hbbtv;

        return client->InsertHbbtv(input);
    });
}

bool BrowserClient::InsertChannel(const std::string& channel) {
    DEBUGLOG("BrowserClient::InsertChannel: %s", channel.c_str());

    return processInternal([&]() -> bool {
        InsertChannelType input;
        input.channel = channel;

        return client->InsertChannel(input);
    });
}

bool BrowserClient::StopVideo(const std::string& reason) {
    DEBUGLOG("BrowserClient::StopVideo: %s", reason.c_str());

    return processInternal([&]() -> bool {
        StopVideoType input;
        input.reason = reason;

        return client->StopVideo(input);
    });
}
