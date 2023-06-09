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

    bool ProcessKey(std::string key);
    bool InsertHbbtv(std::string json);
    bool InsertChannel(std::string json);

private:
    httplib::Client* client;
};

extern BrowserClient* browserClient;
