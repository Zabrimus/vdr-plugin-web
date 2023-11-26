# vdr-plugin-web
A VDR plugin which works together with [cefbrowser](https://github.com/Zabrimus/cefbrowser) and [remotetranscode](https://github.com/Zabrimus/remotetranscode) to show HbbTV application and stream videos.

## Build
```
make; make install
```
## Optional patches
A patch for VDR and several output devices can be found in directory ```patches```. These patches allows the usage of image
scaling using OpenGL instead of the slower GraphicsMagick.

## Configuration
A default configuration can be found in folder config: ```sockets.ini```.

:fire: All ports/ip addresses in ```sockets.ini``` must be the same as for ```cefbrowser``` and ```remotetranscoder```.
It's safe to use the same sockets.ini for all of the three parts (vdr-plugin-web, cefbrowser, remotetranscoder). 

## Parameters of the plugin
```
-c / --config </path/to/sockets.ini>  (mandatory parameter)
-f / --fastscale                      (optional parameter)
-o / --dummyosd                       (optional parameter)
-s / --savets                         (optional parameter)            
```
### -c / --config
This parameter is mandatory and must point to a sockets.ini file which contains 
the host and port configuration of the other components.

### -f / --fastscale
Let the outputdevice scale images from browser. Much faster than using GraphicsMagick.
But it's only available if the VDR patch and the outputdevice patch has been applied.
See [patches/README](patches%2FREADME).

### -o / --dummyosd
Creates a dummy osd while streaming a video. It could be necessary.

### -s / --savets
Used for debugging. The incoming TS streams will be saved in the recordings/web directory. 

## Logging
Uses the VDR logging mechanism. Log entries can be found in ```/var/log/syslog``` with ```[vdrweb]``` prefix.

## HTTP commands (used by the cefbrowser and remotetranscode)
### /ProcessOsdUpdate
#### Parameters
>disp_width (width of the browser page)
> 
>disp_height: (height of the browser page)
>
>x (x-coordinate of the incoming image)
> 
>y (y-coordinate of the incoming image)
>
>width (width of the incoming image)
>
>height (height of the incoming image)

Reads the OSD image part of the out the shared memory.
### /ProcessOsdUpdateQOI
Reads the OSD image part (encoded with qoi) out the the request body.

### /ProcessTSPacket
Reads the TS packet out of the request body.

### /StartVideo
#### Parameters
>videoInfo String containing the dimension of the incoming video 

Starts a new video stream. 
### /StopVideo
Stop a video stream.
### /PauseVideo
Pause a video stream.
### /ResumeVideo
Resume a video stream after pause.
### /VideoSize
#### Parameters
>x x-coordinate of the TV window
>
>y y-coordinate of the TV window
>
>w width of the TV window
>
>h height of the TV window

Scales the live TV window to the requested coordinates and dimension.
### /VideoFullscreen
Scales the live TV window to fullscreen.
### /Hello
Heartbeat
### /ResetVideo
Starts a new video stream without recreating a video player
### /Seeked
Called after jumping in the video. Calls DeviceClear in the output device