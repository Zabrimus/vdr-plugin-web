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
}
