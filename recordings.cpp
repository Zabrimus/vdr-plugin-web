/*
 * recordings.cpp: recordings of VDR from downloaded videos
 *
 * See the README file for copyright information and how to reach the author.
 */

#include "recordings.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vdr/channels.h>
#include <vdr/i18n.h>
#include <vdr/recording.h>
#include <vdr/tools.h>
#include <vdr/videodir.h>

namespace recordings {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trim(const std::string& s, const char *chars = " ") {
    size_t a = s.find_first_not_of(chars);
    if (a == std::string::npos) {
        return "";
    }
    size_t b = s.find_last_not_of(chars);
    return s.substr(a, b - a + 1);
}

// a part of the name: no directory separators, not too long
std::string part(const std::string& s) {
    std::string result;
    for (char c : s) {
        result += (c == '~' || c == '/' || c == '\n' || c == '\r') ? '-' : c;
    }
    result = trim(result);

    // at most 80 bytes, don't cut an UTF-8 character
    if (result.size() > 80) {
        size_t n = 80;
        while (n > 0 && ((unsigned char) result[n] & 0xC0) == 0x80) {
            --n;
        }
        result = trim(result.substr(0, n));
    }

    return result;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) {
        s.replace(pos, from.size(), to);
    }
    return s;
}

int64_t freeBytes(const std::string& dir) {
    struct statvfs st {};
    if (statvfs(dir.c_str(), &st) != 0) {
        return -1;
    }
    return (int64_t) st.f_bavail * (int64_t) st.f_frsize;
}

// another file system: copy, then delete the source
std::string copy(const std::string& from, const std::string& to, const std::atomic<bool>& stop) {
    int in = open(from.c_str(), O_RDONLY);
    if (in < 0) {
        return "The download directory is not reachable by VDR";
    }

    int out = open(to.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (out < 0) {
        close(in);
        return "The recording can't be created";
    }

    std::string error;
    std::vector<char> buffer(1024 * 1024);
    while (error.empty()) {
        ssize_t n = read(in, buffer.data(), buffer.size());
        if (n == 0) {
            break;
        }
        if (n < 0 || stop) {
            error = n < 0 ? "The recording can't be created" : "Interrupted";
            break;
        }
        for (ssize_t done = 0; done < n;) {
            ssize_t w = write(out, buffer.data() + done, n - done);
            if (w <= 0) {
                error = errno == ENOSPC ? "Not enough disk space" : "The recording can't be created";
                break;
            }
            done += w;
        }
    }

    close(in);
    if (close(out) != 0 && error.empty()) {
        error = "The recording can't be created";
    }

    if (!error.empty()) {
        unlink(to.c_str());
        return error;
    }

    unlink(from.c_str());
    return "";
}

// VDR creates the index in a process of its own: it can be stopped at any time
bool generateIndex(const std::string& dir, const std::atomic<bool>& stop) {
    std::string video = std::string("-v") + cVideoDirectory::Name();
    std::string index = "--genindex=" + dir;
    const char *argv[] = { "vdr", video.c_str(), index.c_str(), nullptr };

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        // only async-signal-safe calls until exec
        for (int fd = 3; fd < 1024; ++fd) {
            close(fd);
        }
        execv("/proc/self/exe", (char * const *) argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, WNOHANG) == 0) {
        if (stop) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            unlink((dir + "/index").c_str());
            return false;
        }
        usleep(100000);
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}

Names MakeNames(const std::string& topic, const std::string& title) {
    // topics that are no series
    static const std::set<std::string> generic = {
        "film", "filme", "spielfilm", "spielfilme", "fernsehfilm", "fernsehfilme", "filme im ersten", "kino"
    };

    std::string t = part(topic);
    std::string s = part(title);

    // the title often repeats the topic: "tagesschau - tagesschau 20:00 Uhr"
    std::string rest = s;
    if (!t.empty() && lower(s).rfind(lower(t), 0) == 0) {
        rest = trim(s.substr(t.size()), " -:|,");
    }

    Names names;
    if (s.empty()) {
        s = t.empty() ? "Video" : t;
        t.clear();
    }

    if (t.empty() || generic.count(lower(t)) > 0 || lower(t) == lower(s) || rest.empty()) {
        names.name = "Mediathek~" + s;
        names.title = s;
    } else {
        names.name = "Mediathek~" + t + "~" + rest;
        names.title = t;
        names.shortText = rest;
    }

    return names;
}

std::string Create(const remotetranscoder::DownloadInfo& download, int priority, int lifetime, int reserveMB,
                   const std::atomic<bool>& stop, std::string& recordingName) {
    if (access(download.path.c_str(), R_OK) != 0) {
        return "The download directory is not reachable by VDR";
    }

    std::string title = download.title;
    if (download.topic.empty() && title.empty()) {
        std::string file = download.name;
        title = file.size() > 3 && lower(file.substr(file.size() - 3)) == ".ts" ? file.substr(0, file.size() - 3) : file;
    }
    Names names = MakeNames(download.topic, title);

    std::string channelName;
    int channelNumber = 0;
    if (!download.channel.empty()) {
        LOCK_CHANNELS_READ
        const cChannel *channel = Channels->GetByChannelID(tChannelID::FromString(download.channel.c_str()));
        if (channel != nullptr) {
            channelName = channel->Name();
            channelNumber = channel->Number();
        }
    }

    // the directory like one of a recording of VDR
    char *fsName = ExchangeChars(strdup(names.name.c_str()), true);
    time_t start = download.timestamp > 0 ? (time_t) download.timestamp : time(nullptr);
    struct tm tm {};
    localtime_r(&start, &tm);

    std::string dir;
    for (int instance = 0; instance < 100; ++instance) {
        dir = *cString::sprintf("%s/%s/%04d-%02d-%02d.%02d.%02d.%d-%d.rec", cVideoDirectory::Name(), fsName,
                                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, channelNumber, instance);
        if (access(dir.c_str(), F_OK) != 0) {
            break;
        }
    }
    free(fsName);

    // copied to another file system: the size of the file and the reserve must be free
    struct stat from {}, to {};
    bool sameFileSystem = stat(download.path.c_str(), &from) == 0 && stat(cVideoDirectory::Name(), &to) == 0 && from.st_dev == to.st_dev;
    if (!sameFileSystem) {
        int64_t free = freeBytes(cVideoDirectory::Name());
        if (free >= 0 && free < (int64_t) from.st_size + (int64_t) reserveMB * 1024 * 1024) {
            return "Not enough disk space";
        }
    }

    if (!MakeDirs(dir.c_str(), true)) {
        return "The recording can't be created";
    }

    std::string ts = dir + "/00001.ts";
    if (rename(download.path.c_str(), ts.c_str()) != 0) {
        std::string error = errno == EXDEV ? copy(download.path, ts, stop) : "The recording can't be created";
        if (!error.empty()) {
            RemoveFileOrDir(dir.c_str());
            return error;
        }
    }

    // the info: the index generator adds the frame rate and size
    FILE *f = fopen((dir + "/info").c_str(), "w");
    if (f != nullptr) {
        if (!download.channel.empty()) {
            fprintf(f, "C %s%s%s\n", download.channel.c_str(), channelName.empty() ? "" : " ", channelName.c_str());
        }
        fprintf(f, "E 0 %ld %d 4E 0\n", (long) start, download.duration);
        fprintf(f, "T %s\n", names.title.c_str());
        if (!names.shortText.empty()) {
            fprintf(f, "S %s\n", names.shortText.c_str());
        }
        if (!download.description.empty()) {
            std::string description = replaceAll(replaceAll(download.description, "\r", ""), "\n", "|");
            fprintf(f, "D %s\n", description.c_str());
        }
        if (download.height > 0) {
            // H.264, 16:9, HD or SD
            fprintf(f, "X 5 %s deu\n", download.height >= 720 ? "0B" : "03");
        }
        fprintf(f, "P %d\n", priority);
        fprintf(f, "L %d\n", lifetime);
        fclose(f);
    }

    // without an index VDR creates it at the playback
    if (!generateIndex(dir, stop) && !stop) {
        esyslog("[vdrweb] unable to create the index of %s", dir.c_str());
    }

    {
        LOCK_RECORDINGS_WRITE
        Recordings->AddByName(dir.c_str());
    }

    recordingName = names.name;
    return "";
}

}
