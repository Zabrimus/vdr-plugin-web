#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vdr/osdbase.h>
#include "TranscoderClient.h"

// shared: a lookup thread may still use it when the plugin stops
extern std::shared_ptr<TranscoderClient> transcoderClient;

// Rec while a video plays: the entries of MediathekView with the same duration.
// One entry or only the stream: the download starts without a question.
class cMenuWebDownloadSelect : public cOsdMenu {
private:
    struct Lookup {
        std::mutex mutex;
        bool done = false;
        bool ok = false;
        DownloadLookupResult result;
    };

    std::shared_ptr<Lookup> lookup;
    DownloadLookupResult result;
    bool listed = false;

    eOSState Start(const std::string& url, const std::string& name, int duration);
    eOSState ShowResult();

public:
    explicit cMenuWebDownloadSelect(const std::string& channel);

    eOSState ProcessKey(eKeys Key) override;
};

// Rec without a video: the running downloads and the downloaded files
class cMenuWebDownloads : public cOsdMenu {
private:
    std::vector<DownloadInfo> downloads;
    cTimeMs refresh;

    void Load();
    const DownloadInfo *Selected();
    void SetHelpKeys();
    eOSState Remove();

public:
    cMenuWebDownloads();

    eOSState ProcessKey(eKeys Key) override;
};
