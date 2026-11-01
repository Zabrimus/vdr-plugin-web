# Thrift definition for web plugin

include "common.thrift"

namespace cpp pluginweb

struct ProcessOsdUpdateType {
    1: i32 disp_width,    // width of page
    2: i32 disp_height,   // height of page
    3: i32 x,             // x, y coordinates of image
    4: i32 y,
    5: i32 width,         // height and width of image
    6: i32 height
    7: binary data        // the osd image
}

struct ProcessOsdUpdateQOIType {
    1: i32 render_width,  // width of image
    2: i32 render_height, // height of image
    3: i32 x,             // x, y coordinates of image
    4: i32 y,
    5: binary image_data
}

struct OsdRectType {
    1: i32 x,
    2: i32 y,
    3: i32 width,
    4: i32 height,
    5: binary data        // BGRA, QOI encoded if qoi is set
}

struct ProcessOsdFrameType {
    1: i32 render_width,  // width of page
    2: i32 render_height, // height of page
    3: bool qoi,
    4: list<OsdRectType> rects
}

struct ProcessTSPacketType {
    1: binary ts;
}

struct StartVideoType {
    1: string videoInfo;
}

struct VideoSizeType {
    1: i32 x,
    2: i32 y,
    3: i32 w,
    4: i32 h
}

struct ResetVideoType {
    1: string videoInfo;
}

struct SelectAudioTrackType {
    1: string audioTrack;
}

struct SelectSubtitleTrackType {
    1: string subtitleTrack;   // index of the subtitle track, -1 switches subtitles off
}

struct StreamEventFilterType {
    1: i32 componentTag,       // component tag of the elementary stream with the stream events
    2: bool enable
}

service VdrPluginWeb extends common.CommonService {
    bool ProcessOsdUpdate(1: ProcessOsdUpdateType input) throws (1:common.OperationFailed err),
    bool ProcessOsdUpdateQOI(1: ProcessOsdUpdateQOIType input) throws (1:common.OperationFailed err),
    bool ProcessTSPacket(1: ProcessTSPacketType input) throws (1:common.OperationFailed err),
    bool StartVideo(1: StartVideoType input) throws (1:common.OperationFailed err),
    bool StopVideo() throws (1:common.OperationFailed err),
    bool PauseVideo() throws (1:common.OperationFailed err),
    bool ResumeVideo() throws (1:common.OperationFailed err),
    bool Seeked() throws (1:common.OperationFailed err),
    bool VideoSize(1: VideoSizeType input) throws (1:common.OperationFailed err),
    bool VideoFullscreen() throws (1:common.OperationFailed err),
    bool ResetVideo(1: ResetVideoType input) throws (1:common.OperationFailed err),
    bool SelectAudioTrack(1: SelectAudioTrackType input) throws (1:common.OperationFailed err),
    bool IsWebActive() throws (1:common.OperationFailed err),
    bool SelectSubtitleTrack(1: SelectSubtitleTrackType input) throws (1:common.OperationFailed err),
    bool EndOfStream() throws (1:common.OperationFailed err),
    bool StreamEventFilter(1: StreamEventFilterType input) throws (1:common.OperationFailed err),
    bool ProcessOsdFrame(1: ProcessOsdFrameType input) throws (1:common.OperationFailed err)
}
