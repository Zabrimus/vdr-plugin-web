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

service RemoteTranscoder extends common.CommonService {
    string Probe(1: ProbeType input) throws (1:common.OperationFailed err),
    bool StreamUrl(1: StreamUrlType input) throws (1:common.OperationFailed err),
    bool Pause(1: PauseType input) throws (1:common.OperationFailed err),
    bool SeekTo(1: SeekToType input) throws (1:common.OperationFailed err),
    bool Resume(1: ResumeType input) throws (1:common.OperationFailed err),
    bool Stop(1: StopType input) throws (1:common.OperationFailed err),
    string AudioInfo(1: AudioInfoType input) throws (1:common.OperationFailed err),
    string GetVideo(1: VideoType input) throws (1:common.OperationFailed err),
}
