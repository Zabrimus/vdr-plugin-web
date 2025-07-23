#pragma once

#include <iostream>
#include <mutex>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "CefBrowser.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace ::cefbrowser;

class BrowserClient {
public:
    BrowserClient(std::string vdrIp, int vdrPort);
    ~BrowserClient();

    bool ping();

    bool LoadUrl(const std::string& url);
    bool RedButton(const std::string& channelId);
    bool ReloadOSD();
    bool StartApplication(const std::string& channelId, const std::string &appId, const std::string& appCookie, const std::string& appReferrer, const std::string& appUserAgent, const std::string& url);
    bool ProcessKey(const std::string& key);
    bool StreamError(const std::string& reason);
    bool InsertHbbtv(const std::string& hbbtv);
    bool InsertChannel(const std::string& channel);
    bool StopVideo(const std::string& reason);

private:
    bool connect();
    template <typename F> bool processInternal(F &&request);

private:
    CefBrowserClient *client;
    std::shared_ptr<TSocket> socket;
    std::shared_ptr<TTransport> transport;
    std::recursive_mutex browser_send_mutex;
};