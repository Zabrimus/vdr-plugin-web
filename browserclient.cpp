#include "browserclient.h"

BrowserClient* browserClient;

BrowserClient::BrowserClient(std::string browserIp, int browserPort) {
    client = new httplib::Client(browserIp, browserPort);
    browserClient = this;
}

BrowserClient::~BrowserClient() {
    delete client;
    browserClient = nullptr;
}

bool BrowserClient::LoadUrl(std::string url) {
    httplib::Params params;
    params.emplace("url", url);

    if (auto res = client->Post("/LoadUrl", params)) {
        if (res->status != 200) {
            std::cout << "Http result: " << res->status << std::endl;
            return false;
        }
    } else {
        auto err = res.error();
        std::cout << "HTTP error (LoadURL): " << httplib::to_string(err) << std::endl;
        return false;
    }

    return true;
}

bool BrowserClient::RedButton(std::string channelId) {
    httplib::Params params;
    params.emplace("channelId", channelId);

    if (auto res = client->Post("/RedButton", params)) {
        if (res->status != 200) {
            std::cout << "Http result: " << res->status << std::endl;
            return false;
        }
    } else {
        auto err = res.error();
        std::cout << "HTTP error (RedButton): " << httplib::to_string(err) << std::endl;
        return false;
    }

    return true;
}

bool BrowserClient::ProcessKey(std::string key) {
    httplib::Params params;
    params.emplace("key", key);

    if (auto res = client->Post("/ProcessKey", params)) {
        if (res->status != 200) {
            std::cout << "Http result: " << res->status << std::endl;
            return false;
        }
    } else {
        auto err = res.error();
        std::cout << "HTTP error (ProcessKey): " << httplib::to_string(err) << std::endl;
        return false;
    }

    return true;
}

bool BrowserClient::InsertHbbtv(std::string json) {
    if (auto res = client->Post("/InsertHbbtv", json, "text/plain")) {
        if (res->status != 200) {
            std::cout << "Http result: " << res->status << std::endl;
            return false;
        }
    } else {
        auto err = res.error();
        std::cout << "HTTP error (InsertHbbtv): " << httplib::to_string(err) << std::endl;
        return false;
    }

    return true;
}

bool BrowserClient::InsertChannel(std::string json) {
    if (auto res = client->Post("/InsertChannel", json, "text/plain")) {
        if (res->status != 200) {
            std::cout << "Http result: " << res->status << std::endl;
            return false;
        }
    } else {
        auto err = res.error();
        std::cout << "HTTP error (InsertChannel): " << httplib::to_string(err) << std::endl;
        return false;
    }

    return true;
}

bool BrowserClient::StartApplication(std::string channelId, std::string appId) {
    httplib::Params params;
    params.emplace("channelId", channelId);
    params.emplace("appId", appId);

    if (auto res = client->Post("/StartApplication", params)) {
        if (res->status != 200) {
            std::cout << "Http result: " << res->status << std::endl;
            return false;
        }
    } else {
        auto err = res.error();
        std::cout << "HTTP error (StartApplication): " << httplib::to_string(err) << std::endl;
        return false;
    }

    return true;
}
