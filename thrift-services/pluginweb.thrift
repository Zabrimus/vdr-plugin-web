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
    bool SelectAudioTrack(1: SelectAudioTrackType input) throws (1:common.OperationFailed err)
}
