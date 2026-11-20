#include "downloadmenu.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <thread>
#include <vdr/i18n.h>
#include <vdr/interface.h>
#include <vdr/skins.h>
#include "web.h"
#include "setup.h"

std::shared_ptr<TranscoderClient> transcoderClient;

// errors of the transcoder, translated here
static const char *transcoderErrors[] = {
    trNOOP("No download directory"),
    trNOOP("No video or a live stream"),
    trNOOP("MediathekView is not reachable"),
    trNOOP("This video can't be downloaded"),
    trNOOP("The download directory is not writable"),
    trNOOP("Not enough disk space"),
};

// versions of a broadcast, named by the transcoder
static const char *variants[] = {
    trNOOP("audio description"),
    trNOOP("sign language"),
    trNOOP("original version"),
    trNOOP("plain language"),
    trNOOP("trailer"),
};

static const char *translateVariant(const std::string& variant) {
    for (const char *v : variants) {
        if (variant == v) {
            return tr(v);
        }
    }
    return variant.c_str();
}

static const char *translate(const std::string& error) {
    for (const char *e : transcoderErrors) {
        if (error == e) {
            return tr(e);
        }
    }
    return error.c_str();
}

static std::string formatDuration(int seconds) {
    return *cString::sprintf("%d:%02d", seconds / 60, seconds % 60);
}

static std::string formatHeight(int height) {
    return height > 0 ? *cString::sprintf("%dp", height) : "";
}

static std::string formatSize(int64_t bytes) {
    if (bytes <= 0) {
        return "";
    }
    if (bytes >= 1000LL * 1000 * 1000) {
        return *cString::sprintf("%.1f GB", bytes / 1e9);
    }
    return *cString::sprintf("%d MB", (int) (bytes / 1000000));
}

// --- the watcher -----------------------------------------------------------------------------

namespace {
std::mutex watchMutex;
std::condition_variable watchCondition;
std::thread watchThread;
bool watchStop = false;
bool watchActive = false;
bool watchWake = false;            // ask now, not after the interval
std::string watchStatus;
std::map<std::string, std::string> watchRunning;   // id -> file name
}

static void Watch(std::shared_ptr<TranscoderClient> client) {
    std::unique_lock<std::mutex> lock(watchMutex);

    while (!watchStop) {
        if (!watchActive) {
            watchCondition.wait(lock, [] { return watchStop || watchActive; });
            continue;
        }
        watchWake = false;

        lock.unlock();
        std::vector<DownloadInfo> downloads;
        bool ok = client->DownloadList(downloads);
        lock.lock();

        std::map<std::string, std::string> running;
        int progress = 0;
        for (const auto& d : downloads) {
            if (d.state == "running" || d.state == "queued") {
                running[d.id] = d.name;
                progress = d.progress;
            }
        }

        // a download that doesn't run anymore: finished, failed or cancelled
        std::vector<std::pair<eMessageType, std::string>> messages;
        for (const auto& r : watchRunning) {
            if (!ok || running.count(r.first) > 0) {
                continue;
            }

            for (const auto& d : downloads) {
                if (d.name == r.second && d.state == "done") {
                    messages.emplace_back(mtInfo, *cString::sprintf(tr("Download finished: %s"), d.name.c_str()));
                } else if (d.id == r.first && d.state == "failed") {
                    // a known reason, not the message of ffmpeg
                    std::string reason = translate(d.error) != d.error.c_str() ? std::string(" - ") + translate(d.error) : "";
                    messages.emplace_back(mtError, *cString::sprintf(tr("Download failed: %s"), (d.name + reason).c_str()));
                }
            }
        }

        watchRunning = running;
        if (running.empty()) {
            watchStatus.clear();
            watchActive = false;
        } else if (running.size() == 1) {
            watchStatus = *cString::sprintf(tr("Download %d%%"), progress);
        } else {
            watchStatus = *cString::sprintf(tr("%d downloads"), (int) running.size());
        }

        // queued: shown when no menu is open, without waiting
        lock.unlock();
        for (const auto& m : messages) {
            Skins.QueueMessage(m.first, m.second.c_str());
        }
        lock.lock();

        watchCondition.wait_for(lock, std::chrono::seconds(3), [] { return watchStop || watchWake; });
    }
}

void StartDownloadWatcher() {
    std::lock_guard<std::mutex> lock(watchMutex);
    if (transcoderClient == nullptr || watchThread.joinable()) {
        return;
    }

    // downloads may still run from before a restart of VDR
    watchStop = false;
    watchActive = true;
    watchThread = std::thread(Watch, NewTranscoderClient());
}

void StopDownloadWatcher() {
    {
        std::lock_guard<std::mutex> lock(watchMutex);
        watchStop = true;
    }
    watchCondition.notify_all();

    if (watchThread.joinable()) {
        watchThread.join();
    }
}

void WatchDownloads() {
    {
        std::lock_guard<std::mutex> lock(watchMutex);
        watchActive = true;
        watchWake = true;
    }
    watchCondition.notify_all();
}

std::string DownloadStatus() {
    std::lock_guard<std::mutex> lock(watchMutex);
    return watchStatus;
}

// --- cMenuWebDownloadSelect ------------------------------------------------------------------

cMenuWebDownloadSelect::cMenuWebDownloadSelect(const DownloadLookupType& input)
        : cOsdMenu(tr("Download"), 9, 6, 6, 8) {
    lookup = std::make_shared<Lookup>();

    Add(new cOsdItem(tr("Searching in MediathekView..."), osUnknown, false));
    Display();

    // the query takes a while, don't block VDR. The menu may be closed before it ends.
    auto l = lookup;
    auto client = transcoderClient != nullptr ? NewTranscoderClient() : nullptr;
    auto browser = NewBrowserClient();
    std::thread([l, client, browser, input]() {
        DownloadLookupType in = input;
        browser->PageTitle(in.pageTitle);

        DownloadLookupResult r;
        bool ok = client != nullptr && client->DownloadLookup(r, in);

        std::lock_guard<std::mutex> lock(l->mutex);
        l->result = r;
        l->ok = ok;
        l->done = true;
    }).detach();
}

eOSState cMenuWebDownloadSelect::Start(const DownloadCandidate *candidate) {
    DownloadStartType input;
    input.url = candidate != nullptr ? candidate->url : "";
    input.name = candidate != nullptr ? candidate->name : result.streamName;
    input.duration = candidate != nullptr ? candidate->duration : 0;
    input.maxRate = webSetup.downloadRate * 1000;
    input.reserve = webSetup.downloadReserve * 1024;
    input.maxParallel = webSetup.downloadParallel;
    input.width = candidate != nullptr ? candidate->width : result.streamWidth;
    input.height = candidate != nullptr ? candidate->height : result.streamHeight;

    std::string error;
    if (transcoderClient == nullptr || !transcoderClient->DownloadStart(error, input)) {
        Skins.Message(mtError, tr("Transcoder not available"));
    } else if (!error.empty()) {
        Skins.Message(mtError, translate(error));
    } else {
        WatchDownloads();
        Skins.Message(mtInfo, *cString::sprintf(tr("Download started: %s"), input.name.c_str()));
    }

    return osEnd;
}

void cMenuWebDownloadSelect::AddCandidate(int index) {
    const auto& c = result.candidates[index];

    std::string title = c.title.find(c.topic) == std::string::npos ? c.topic + " - " + c.title : c.title;
    if (!c.variant.empty()) {
        title += std::string(" (") + translateVariant(c.variant) + ")";
    }

    std::string text = *cString::sprintf("%s\t%s\t%s\t%s\t%s - %s", *ShortDateString(c.timestamp), formatDuration(c.duration).c_str(),
                                         formatHeight(c.height).c_str(), formatSize(c.size).c_str(), c.channel.c_str(), title.c_str());
    Add(new cOsdItem(text.c_str()));
    items.push_back(index);
}

eOSState cMenuWebDownloadSelect::ShowResult() {
    bool ok;
    {
        std::lock_guard<std::mutex> lock(lookup->mutex);
        ok = lookup->ok;
        result = lookup->result;
    }

    if (!ok) {
        Skins.Message(mtError, tr("Transcoder not available"));
        return osEnd;
    }

    if (result.candidates.empty() && !result.stream) {
        Skins.Message(mtError, translate(result.error));
        return osEnd;
    }

    // MediathekView not reachable: the stream is saved
    if (!result.error.empty()) {
        Skins.Message(mtWarning, translate(result.error));
    }

    if (result.candidates.empty()) {
        return Start(nullptr);
    }

    // one entry is the video for sure, or the only one
    const DownloadCandidate *exact = nullptr;
    int count = 0;
    for (const auto& c : result.candidates) {
        if (c.exact) {
            exact = &c;
            ++count;
        }
    }

    if (count == 1) {
        return Start(exact);
    }

    if (result.candidates.size() == 1) {
        return Start(&result.candidates[0]);
    }

    // the normal versions first, then the other versions like audio description
    Clear();
    items.clear();

    for (size_t i = 0; i < result.candidates.size(); ++i) {
        if (result.candidates[i].variant.empty() || result.candidates[i].exact) {
            AddCandidate((int) i);
        }
    }

    if (result.stream) {
        std::string text = *cString::sprintf("\t\t%s\t%s\t%s", formatHeight(result.streamHeight).c_str(), formatSize(result.streamSize).c_str(),
                                             tr("Stream without MediathekView entry"));
        Add(new cOsdItem(text.c_str()));
        items.push_back(itemStream);
    }

    bool header = false;
    for (size_t i = 0; i < result.candidates.size(); ++i) {
        if (!result.candidates[i].variant.empty() && !result.candidates[i].exact) {
            if (!header) {
                Add(new cOsdItem(tr("Other versions:"), osUnknown, false));
                items.push_back(itemStream - 1);
                header = true;
            }
            AddCandidate((int) i);
        }
    }

    listed = true;
    Display();
    return osContinue;
}

eOSState cMenuWebDownloadSelect::ProcessKey(eKeys Key) {
    if (Key == kMenu) {
        Key = kBack;
    }

    eOSState state = cOsdMenu::ProcessKey(Key);

    if (state == osBack || state == osEnd) {
        // closed, the lookup is ignored
    } else if (!listed) {
        bool done;
        {
            std::lock_guard<std::mutex> lock(lookup->mutex);
            done = lookup->done;
        }

        if (done) {
            state = ShowResult();
        }
    } else if (state == osUnknown && Key == kOk && Current() >= 0 && Current() < (int) items.size()) {
        int item = items[Current()];
        if (item >= 0) {
            state = Start(&result.candidates[item]);
        } else if (item == itemStream) {
            state = Start(nullptr);
        }
    }

    if (state == osBack || state == osEnd) {
        ReturnFromDownloads();
        return osEnd;
    }

    return state;
}

// --- cMenuWebDownloads -----------------------------------------------------------------------

cMenuWebDownloads::cMenuWebDownloads()
        : cOsdMenu(tr("Downloads"), 7, 9) {
    Load();
}

void cMenuWebDownloads::Load() {
    std::string current;
    if (const DownloadInfo *d = Selected()) {
        current = d->id;
    }

    downloads.clear();
    if (transcoderClient == nullptr || !transcoderClient->DownloadList(downloads)) {
        Skins.Message(mtError, tr("Transcoder not available"));
    }

    Clear();
    int select = 0;
    for (size_t i = 0; i < downloads.size(); ++i) {
        const auto& d = downloads[i];

        std::string state;
        if (d.state == "running") {
            state = *cString::sprintf("%d%%", d.progress);
        } else if (d.state == "queued") {
            state = tr("waiting");
        } else if (d.state == "failed") {
            state = tr("failed");
        }

        std::string text = *cString::sprintf("%s\t%.1f MB\t%s", state.c_str(), d.size / 1048576.0, d.name.c_str());
        Add(new cOsdItem(text.c_str()));

        if (d.id == current) {
            select = (int) i;
        }
    }

    if (downloads.empty()) {
        Add(new cOsdItem(tr("No downloads"), osUnknown, false));
    } else {
        SetCurrent(Get(select));
    }

    SetHelpKeys();
    Display();
    refresh.Set(2000);
}

const DownloadInfo *cMenuWebDownloads::Selected() {
    int index = Current();
    return index >= 0 && index < (int) downloads.size() ? &downloads[index] : nullptr;
}

void cMenuWebDownloads::SetHelpKeys() {
    const DownloadInfo *d = Selected();

    if (d == nullptr) {
        SetHelp(nullptr);
    } else if (d->state == "running" || d->state == "queued") {
        SetHelp(tr("Button$Cancel"));
    } else if (d->state == "failed") {
        SetHelp(tr("Button$Remove"));
    } else {
        SetHelp(tr("Button$Delete"));
    }
}

eOSState cMenuWebDownloads::Remove() {
    const DownloadInfo *d = Selected();
    if (d == nullptr) {
        return osContinue;
    }

    bool running = d->state == "running" || d->state == "queued";
    const char *question = running ? tr("Cancel download?") : d->state == "failed" ? nullptr : tr("Delete file?");
    if (question != nullptr && !Interface->Confirm(question)) {
        return osContinue;
    }

    if (transcoderClient == nullptr || !transcoderClient->DownloadRemove(d->id)) {
        Skins.Message(mtError, tr("Can't remove the download"));
    }
    WatchDownloads();

    Load();
    return osContinue;
}

eOSState cMenuWebDownloads::ProcessKey(eKeys Key) {
    if (Key == kMenu) {
        Key = kBack;
    }

    eOSState state = cOsdMenu::ProcessKey(Key);

    if (state == osUnknown) {
        switch (Key) {
            case kRed:
                state = Remove();
                break;

            case kOk:
                if (const DownloadInfo *d = Selected(); d != nullptr && d->state == "failed") {
                    Skins.Message(mtError, translate(d->error));
                }
                state = osContinue;
                break;

            default:
                break;
        }
    }

    // the progress of running downloads
    if (Key == kNone && refresh.TimedOut()) {
        bool running = false;
        for (const auto& d : downloads) {
            running = running || d.state == "running" || d.state == "queued";
        }

        if (running) {
            Load();
        } else {
            refresh.Set(2000);
        }
    }

    if (Key != kNone) {
        SetHelpKeys();
    }

    if (state == osBack || state == osEnd) {
        ReturnFromDownloads();
        return osEnd;
    }

    return state;
}
