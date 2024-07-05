#pragma once

struct WebApp_Main_v1_0 {
    std::string section;
    std::string userAgent;
    std::string referrer;
    std::string cookie;
};

struct WebApp_Url_v1_0 {
    std::string section;
    std::string url;
    std::string userAgent;
    std::string referrer;
    std::string cookie;
};

struct WebApp_M3U_v1_0 {
    std::string section;
    std::string m3uContent;
    std::string userAgent;
    std::string referrer;
    std::string cookie;
};
