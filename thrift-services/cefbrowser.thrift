# Thrift definition for cefbrowser

include "common.thrift"

namespace cpp cefbrowser

struct LoadUrlType {
    1: string url
}

struct RedButtonType {
    1: string channelId
}

struct StartApplicationType {
    1: string channelId,
    2: string appId,
    3: string appCookie,
    4: string appReferrer,
    5: string appUserAgent,
    6: optional string url
}

struct ProcessKeyType {
    1:string key
}

struct StreamErrorType {
    1: string reason
}

struct InsertHbbtvType {
    1: string hbbtv
}

struct InsertChannelType {
    1: string channel
}

struct StopVideoType {
    1: string reason
}

struct PlayerStatusType {
    1: i64 position,           // position of the displayed picture in ms
    2: i32 state               // 0 = playing, 1 = paused, 2 = buffering, 3 = ended
}

struct RenderParametersType {
    1: i32 width,              // width of the page: 1280, 1920, 2560 or 3840, 0 = unchanged
    2: i32 frameRate           // frames per second, 0 = unchanged
}

struct StreamEventType {
    1: i32 componentTag,
    2: i32 eventId,
    3: binary privateData,
    4: string status           // "trigger" or "error"
}

service CefBrowser extends common.CommonService {
    bool LoadUrl(1: LoadUrlType input) throws (1:common.OperationFailed err),
    bool RedButton(1: RedButtonType input) throws (1:common.OperationFailed err),
    bool ReloadOSD() throws (1:common.OperationFailed err),
    bool StartApplication(1: StartApplicationType input) throws (1:common.OperationFailed err),
    bool ProcessKey(1: ProcessKeyType input) throws (1:common.OperationFailed err),
    bool StreamError(1: StreamErrorType input) throws (1:common.OperationFailed err),
    bool InsertHbbtv(1: InsertHbbtvType input) throws (1:common.OperationFailed err),
    bool InsertChannel(1: InsertChannelType input) throws (1:common.OperationFailed err),
    bool StopVideo(1: StopVideoType input) throws (1:common.OperationFailed err),
    bool PlayerStatus(1: PlayerStatusType input) throws (1:common.OperationFailed err),
    bool StreamEvent(1: StreamEventType input) throws (1:common.OperationFailed err),
    bool SetRenderParameters(1: RenderParametersType input) throws (1:common.OperationFailed err)
}