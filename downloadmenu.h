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

// a TS file that becomes a recording: "importing", "failed" with the error, or empty
std::string ImportState(const std::string& file, std::string *error);

// a failed recording of a TS file is tried again
void ImportAgain(const std::string& file);

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
    std::string channelId;
    DownloadLookupResult result;
    std::vector<int> items;             // the candidate of an item, or itemStream
    bool listed = false;

    eOSState Start(const DownloadCandidate *candidate);
    eOSState ShowResult();
    void AddCandidate(int index);

public:
    // the channel name and its EPG are filled in, the page title is asked for
    cMenuWebDownloadSelect(const DownloadLookupType& input, const std::string& channelId);

    eOSState ProcessKey(eKeys Key) override;
};

// SVDRP DOWNLOADS: the running downloads and the downloaded files
class cMenuWebDownloads : public cOsdMenu {
private:
    std::vector<DownloadInfo> downloads;
    cTimeMs refresh;
    bool subMenu;                       // of the setup

    void Load();
    const DownloadInfo *Selected();
    void SetHelpKeys();
    eOSState Remove();
    eOSState Retry();
    eOSState Record();

public:
    explicit cMenuWebDownloads(bool subMenu = false);

    eOSState ProcessKey(eKeys Key) override;
};
