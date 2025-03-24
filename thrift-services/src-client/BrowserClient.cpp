#include <iostream>
#include <utility>
#include <mutex>

#include "BrowserClient.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::cefbrowser;
using namespace ::common;

bool browserClientLogThriftMessages = false;

void browserClientOutputFunction(const char* msg) {
    if (browserClientLogThriftMessages) {
        fprintf(stderr, "%s\n", msg);
    }
}

BrowserClient::BrowserClient(std::string vdrIp, int vdrPort) {
    std::shared_ptr<TTransport> socket(new TSocket(vdrIp, vdrPort));
    transport = static_cast<const std::shared_ptr<TTransport>>(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    client = new CefBrowserClient(protocol);

    GlobalOutput.setOutputFunction(browserClientOutputFunction);
}

BrowserClient::~BrowserClient() {
    if (transport->isOpen()) {
        transport->close();
    }

    delete client;
}

bool BrowserClient::connect() {
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

bool BrowserClient::ping() {
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
    return processInternal([&]() -> bool {
        LoadUrlType input;
        input.url = url;

        return client->LoadUrl(input);
    });
}

bool BrowserClient::RedButton(const std::string& channelId) {
    return processInternal([&]() -> bool {
        RedButtonType input;
        input.channelId = channelId;

        return client->RedButton(input);
    });
}

bool BrowserClient::ReloadOSD() {
    return processInternal([&]() -> bool {
        return client->ReloadOSD();
    });
}

bool BrowserClient::StartApplication(const std::string& channelId, const std::string& appId, const std::string& appCookie, const std::string& appReferrer, const std::string& appUserAgent, const std::string& url) {
    return processInternal([&]() -> bool {
        StartApplicationType input;
        input.channelId = channelId;
        input.appId = appId;
        input.appCookie = appCookie;
        input.appReferrer = appReferrer;
        input.appUserAgent = appUserAgent;
        input.url = url;

        return client->StartApplication(input);
    });
}

bool BrowserClient::ProcessKey(const std::string& key) {
    return processInternal([&]() -> bool {
        ProcessKeyType input;
        input.key = key;

        return client->ProcessKey(input);
    });
}

bool BrowserClient::StreamError(const std::string& reason) {
    return processInternal([&]() -> bool {
        StreamErrorType input;
        input.reason = reason;

        return client->StreamError(input);
    });
}

bool BrowserClient::InsertHbbtv(const std::string& hbbtv) {
    return processInternal([&]() -> bool {
        InsertHbbtvType input;
        input.hbbtv = hbbtv;

        return client->InsertHbbtv(input);
    });
}

bool BrowserClient::InsertChannel(const std::string& channel) {
    return processInternal([&]() -> bool {
        InsertChannelType input;
        input.channel = channel;

        return client->InsertChannel(input);
    });
}

bool BrowserClient::StopVideo(const std::string& reason) {
    return processInternal([&]() -> bool {
        StopVideoType input;
        input.reason = reason;

        return client->StopVideo(input);
    });
}
