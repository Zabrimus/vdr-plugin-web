/*
 * recordings.h: recordings of VDR from downloaded videos
 *
 * See the README file for copyright information and how to reach the author.
 */

#pragma once

#include <atomic>
#include <string>
#include "remotetranscoder_types.h"

namespace recordings {

// "Mediathek~Topic~Title" and the title and short text of the info
struct Names {
    std::string name;
    std::string title;
    std::string shortText;
};

Names MakeNames(const std::string& topic, const std::string& title);

// A TS file of the transcoder becomes a recording: it is moved into the video
// directory, the info is written and VDR creates the index. This takes a while,
// it is not called in the main thread. stop cancels the copy and the index.
// Returns an error, empty if the recording was created.
std::string Create(const remotetranscoder::DownloadInfo& download, int priority, int lifetime, int reserveMB,
                   const std::atomic<bool>& stop, std::string& recordingName);

}
