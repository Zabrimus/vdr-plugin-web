#pragma once

#include <string>
#include <vdr/tools.h>
#include "httplib.h"

class BrowserClient {
public:
    explicit BrowserClient(std::string browserIp, int browserPort);
    ~BrowserClient();

    bool LoadUrl(std::string url);
    bool RedButton(std::string channelId);
    bool ReloadOSD();

    bool ProcessKey(std::string key);
    bool InsertHbbtv(std::string json);
    bool InsertChannel(std::string json);
    bool StartApplication(std::string channelId, std::string appId);

    void HelloFromBrowser();

private:
    bool CheckConnection(std::string method);

private:
    bool helloReceived;
    httplib::Client* client;
};

extern BrowserClient* browserClient;
