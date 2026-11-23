# Thrift definition for remotetranscoder

include "common.thrift"

namespace cpp remotetranscoder

struct ProbeType {
    1: string url,
    2: string cookies,
    3: string referer,
    4: string userAgent,
    5: string responseIp,
    6: string responsePort,
    7: string vdrIp,
    8: string vdrPort,
    9: string postfix
}

struct StreamUrlType {
    1: string url,
    2: string cookies,
    3: string referer,
    4: string userAgent,
    5: string responseIp,
    6: string responsePort,
    7: string vdrIp,
    8: string vdrPort,
    9: string mpdStart
}

struct PauseType {
    1: string streamId
}

struct SeekToType {
    1: string streamId,
    2: string seekTo
}

struct ResumeType {
    1: string streamId,
    2: string position
}

struct StopType {
    1: string streamId,
    2: string reason
}

struct AudioInfoType {
    1: string streamId
}

struct VideoType {
    1: string filename
}

// Media Source Extensions: the browser appends the segments of the player
// (fragmented MP4) and the transcoder remuxes them to TS.
struct MseStartType {
    1: string responseIp,
    2: string responsePort,
    3: string vdrIp,
    4: string vdrPort,
    5: string postfix,
    6: string duration,          // seconds, N/A if unknown (live)
    7: list<string> tracks       // mime type with codecs of each SourceBuffer
}

struct MseAppendType {
    1: string streamId,
    2: i32 track,                // index in MseStartType.tracks
    3: binary data
}

struct MseResetType {
    1: string streamId           // a seek: drop all data, the next segments start a new stream
}

struct MseEndOfStreamType {
    1: string streamId
}

// Downloads: the transcoder saves videos as MP4 or TS in its download directory,
// independent of the playback.

// an event of the VDR channel, the applications mostly show the last days
struct DownloadEpgEvent {
    1: string title,
    2: i64 start                 // unix time
}

struct DownloadLookupType {
    1: string channel,           // name of the VDR channel
    2: string pageTitle,         // title of the page of the application
    3: list<DownloadEpgEvent> epg,
    4: i32 maxHeight             // height of the video at most, 0: the best
}

// an entry of MediathekView with the same duration as the current video
struct DownloadCandidate {
    1: string name,              // file name without extension
    2: string url,
    3: i32 duration,             // seconds
    4: string channel,
    5: string topic,
    6: string title,
    7: i64 timestamp,            // broadcast, unix time
    8: bool exact,               // for sure the video of the stream
    9: string variant,           // e.g. "Audiodeskription", empty: the normal version
    10: i32 width,               // 0: unknown
    11: i32 height,
    12: i64 size,                // bytes, estimated, 0: unknown
    13: string description
}

struct DownloadLookupResult {
    1: bool stream,              // the current stream can be downloaded
    2: string streamName,        // its file name without extension
    3: list<DownloadCandidate> candidates,
    4: string error,
    5: i32 streamWidth,          // 0: unknown
    6: i32 streamHeight,
    7: i64 streamSize            // bytes, estimated, 0: unknown
}

struct DownloadStartType {
    1: string url,               // empty: the current stream
    2: string name,
    3: i32 duration,
    4: i32 maxRate,              // kbit/s, 0: unlimited
    5: string format,            // "mp4" or "ts", empty: mp4
    6: i32 reserve,              // MB to keep free on the disk, 0: no check
    7: i32 maxParallel,          // downloads at the same time, 0: no limit
    8: string topic,             // for a recording of VDR
    9: string title,
    10: string channel,          // VDR channel ID
    11: i64 timestamp,
    12: string description,
    13: i32 width,
    14: i32 height
}

struct DownloadInfo {
    1: string id,
    2: string name,              // file name
    3: string state,             // queued, running, converting, done, failed
    4: i32 progress,             // percent
    5: i64 size,                 // bytes
    6: string error,
    7: string format,            // "mp4" or "ts"
    8: string path,              // the file in the file system of the transcoder
    9: string topic,
    10: string title,
    11: string channel,
    12: i64 timestamp,
    13: string description,
    14: i32 duration,
    15: i32 width,
    16: i32 height
}

struct DownloadRemoveType {
    1: string id                 // a running download is cancelled, a file deleted
}

struct DownloadRetryType {
    1: string id                 // a failed or interrupted download
}

struct DownloadConvertType {
    1: string id,                // a MP4 file, converted to TS
    2: i32 reserve               // MB to keep free on the disk, 0: no check
}

service RemoteTranscoder extends common.CommonService {
    string Probe(1: ProbeType input) throws (1:common.OperationFailed err),
    bool StreamUrl(1: StreamUrlType input) throws (1:common.OperationFailed err),
    bool Pause(1: PauseType input) throws (1:common.OperationFailed err),
    bool SeekTo(1: SeekToType input) throws (1:common.OperationFailed err),
    bool Resume(1: ResumeType input) throws (1:common.OperationFailed err),
    bool Stop(1: StopType input) throws (1:common.OperationFailed err),
    string AudioInfo(1: AudioInfoType input) throws (1:common.OperationFailed err),
    string GetVideo(1: VideoType input) throws (1:common.OperationFailed err),
    string MseStart(1: MseStartType input) throws (1:common.OperationFailed err),
    bool MseAppend(1: MseAppendType input) throws (1:common.OperationFailed err),     // false: buffer full
    bool MseReset(1: MseResetType input) throws (1:common.OperationFailed err),
    bool MseEndOfStream(1: MseEndOfStreamType input) throws (1:common.OperationFailed err),
    DownloadLookupResult DownloadLookup(1: DownloadLookupType input) throws (1:common.OperationFailed err),
    string DownloadStart(1: DownloadStartType input) throws (1:common.OperationFailed err),   // an error, empty if started
    list<DownloadInfo> DownloadList() throws (1:common.OperationFailed err),
    bool DownloadRemove(1: DownloadRemoveType input) throws (1:common.OperationFailed err),
    string DownloadRetry(1: DownloadRetryType input) throws (1:common.OperationFailed err),      // an error, empty if started
    string DownloadConvert(1: DownloadConvertType input) throws (1:common.OperationFailed err),  // an error, empty if started
}
