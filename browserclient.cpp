#include <vdr/skins.h>
#include "browserclient.h"

BrowserClient* browserClient;

BrowserClient::BrowserClient(std::string browserIp, int browserPort) {
    client = new httplib::Client(browserIp, browserPort);
    client->set_read_timeout(15, 0);

    browserClient = this;
    helloReceived = true; // be optimistic
}

BrowserClient::~BrowserClient() {
    delete client;
    browserClient = nullptr;
}

bool BrowserClient::LoadUrl(std::string url) {
    if (!CheckConnection("LoadURL")) {
        return false;
    }

    httplib::Params params;
    params.emplace("url", url);

    if (auto res = client->Post("/LoadUrl", params)) {
        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (LoadURL): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::RedButton(std::string channelId) {
    if (!CheckConnection("RedButton")) {
        Skins.QueueMessage(mtInfo, tr("Browser not available"));
        return false;
    }

    httplib::Params params;
    params.emplace("channelId", channelId);

    if (auto res = client->Post("/RedButton", params)) {
        if (res->status != 200) {
            Skins.QueueMessage(mtInfo, tr("RedButton Application not available"));
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (RedButton): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::ReloadOSD() {
    if (!CheckConnection("ReloadOSD")) {
        Skins.QueueMessage(mtInfo, tr("Browser not available"));
        return false;
    }

    if (auto res = client->Get("/ReloadOSD")) {
        if (res->status != 200) {
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (ReloadOSD): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::ProcessKey(std::string key) {
    if (!CheckConnection("ProcessKey")) {
        return false;
    }

    httplib::Params params;
    params.emplace("key", key);

    if (auto res = client->Post("/ProcessKey", params)) {
        helloReceived = true;

        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (ProcessKey): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::InsertHbbtv(std::string json) {
    if (!CheckConnection("InsertHbbtv")) {
        return false;
    }

    if (auto res = client->Post("/InsertHbbtv", json, "text/plain")) {
        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (InsertHbbtv): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::InsertChannel(std::string json) {
    if (!CheckConnection("InsertChannel")) {
        return false;
    }

    if (auto res = client->Post("/InsertChannel", json, "text/plain")) {
        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (InsertChannel): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::StartApplication(std::string channelId, std::string appId) {
    if (!CheckConnection("StartApplication")) {
        return false;
    }

    httplib::Params params;
    params.emplace("channelId", channelId);
    params.emplace("appId", appId);

    if (auto res = client->Post("/StartApplication", params)) {
        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (StartApplication): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::SetVolume(int currentVolume, int maxVolume) {
    if (!CheckConnection("SetVolume")) {
        return false;
    }

    httplib::Params params;
    params.emplace("currentVolume", std::to_string(currentVolume));
    params.emplace("maxVolume", std::to_string(maxVolume));

    if (auto res = client->Post("/SetVolume", params)) {
        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (SetVolume): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

bool BrowserClient::StopVideo() {
    if (!CheckConnection("StopVideo")) {
        return false;
    }

    dsyslog("Request StopVideo");

    if (auto res = client->Get("/StopVideo")) {
        if (res->status != 200) {
            dsyslog("[vdrweb] Http result: %d", res->status);
            return false;
        }
    } else {
        auto err = res.error();
        dsyslog("[vdrweb] HTTP error (StopVideo): %s", httplib::to_string(err).c_str());
        helloReceived = false;
        return false;
    }

    return true;
}

void BrowserClient::HelloFromBrowser() {
    helloReceived = true;
}

bool BrowserClient::CheckConnection(std::string method) {
    if (!helloReceived) {
        // do nothing, browser is not available
        dsyslog("[vdrweb] %s, browser is not available", method.c_str());
        return false;
    }

    return true;
}