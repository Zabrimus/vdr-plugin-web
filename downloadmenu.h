#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vdr/osdbase.h>
#include "TranscoderClient.h"
#include "BrowserClient.h"

// shared: a lookup thread may still use it when the plugin stops
extern std::shared_ptr<TranscoderClient> transcoderClient;

// A connection of its own: a call waits for the running call of the same connection.
// The slow lookup and the watcher must not block VDR.
std::shared_ptr<TranscoderClient> NewTranscoderClient();
std::shared_ptr<BrowserClient> NewBrowserClient();

// While downloads run, a thread asks the transcoder for their progress
// and shows a message when one has finished or failed.
void StartDownloadWatcher();
void StopDownloadWatcher();

// a download was started or removed: ask the transcoder again
void WatchDownloads();

// "Download 45%", "2 downloads" or empty
std::string DownloadStatus();

// Rec while a video plays: the entries of MediathekView with the same duration.
// The video for sure, one entry or only the stream: the download starts without a question.
class cMenuWebDownloadSelect : public cOsdMenu {
private:
    struct Lookup {
        std::mutex mutex;
        bool done = false;
        bool ok = false;
        DownloadLookupResult result;
    };

    static constexpr int itemStream = -1;

    std::shared_ptr<Lookup> lookup;
    DownloadLookupResult result;
    std::vector<int> items;             // the candidate of an item, or itemStream
    bool listed = false;

    eOSState Start(const DownloadCandidate *candidate);
    eOSState ShowResult();
    void AddCandidate(int index);

public:
    // the channel name and its EPG are filled in, the page title is asked for
    explicit cMenuWebDownloadSelect(const DownloadLookupType& input);

    eOSState ProcessKey(eKeys Key) override;
};

// SVDRP DOWNLOADS: the running downloads and the downloaded files
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
