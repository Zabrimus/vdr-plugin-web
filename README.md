# vdr-plugin-web
A VDR plugin which works together with [cefbrowser](https://github.com/Zabrimus/cefbrowser) and [remotetranscode](https://github.com/Zabrimus/remotetranscode) to show HbbTV application and stream videos.

## Build
At first the libraries for Apache Thrift to be build. This step is only once necessary.
Choose either the build with or without the Thrift compiler. The build without the Thrift compiler is much faster.
```
build-thrift.sh
or
build-thrift-with-compiler.sh
```
Then the plugin can be build
```
make -j && make install
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
-b / --bindall                        (optional parameter)     
-n / --name                           (optional parameter)
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

### -b / --bindall
If set, then bind the internal HTTP server to all available network devices. Otherwise use the configured IP.

### - / --name
If set, then the menu entry will have a different name. Instead default ```web``` another name can be choosed.

## Setup

### HbbTV hint
Like a TV, a small hint "HbbTV available" is shown after a channel switch, if the channel
signals an HbbTV application which starts automatically (AIT control code AUTOSTART).
The hint waits until other OSDs (e.g. the channel display) are closed and disappears
after the configured time, on the next channel switch or if a menu is opened.

VDR uses the red key itself. To start HbbTV with a key, map it to the plugin in
`keymacros.conf`, e.g. `Red @web`.

| Setup entry              | Default      | Description                    |
|--------------------------|--------------|--------------------------------|
| Show HbbTV hint          | yes          | Enables the hint               |
| HbbTV hint duration (s)  | 7            | Display time in seconds (1-60) |
| HbbTV hint position      | bottom right | Corner of the OSD area         |

### Display
| Setup entry             | Default         | Description |
|-------------------------|-----------------|-------------|
| Browser resolution      | browser default | Size of the page rendered by the browser. 1280x720 is the native size of HbbTV applications and the fastest one. Use it together with scaling by the output device. |
| Scale by output device  | command line -f | Only if built with `ENABLE_FAST_SCALE=1`. Scales the OSD of the browser with the output device (e.g. by the GPU) instead of GraphicsMagick. |
| Browser frame rate      | browser default | Frame rate of the browser (25, 30, 50, 60). A higher rate reacts faster to keys, but needs more CPU. |

"browser default" keeps the values the browser was started with (`--zoom`, 25 fps).

## Logging
Uses the VDR logging mechanism. Log entries can be found in ```/var/log/syslog``` with ```[vdrweb]``` prefix.

