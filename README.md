# Warning
- highly instable 
- does not works as desired 
- could destroy your system 
- in development phase


## vdr-plugin-web (Part 1)
Works together with ```cefbrowser``` and ```remotetranscoder```

# Build
```
make; make install
```

# Configuration
A default configuration can be found in folder config: ```sockets.ini```.

:fire: All ports/ip addresses in ```sockets.ini``` must be the same as for ```cefbrowser``` and ```remotetranscoder```.
It's safe to use the same sockets.ini for all of the three parts (vdr-plugin-web, cefbrowser, remotetranscoder). 

# Start
Like all other VDR plugins.
```
[web]
-c /path/to/sockets.ini
```

# Logging
Uses the VDR logging mechanism. Log entries can be found in ```/var/log/syslog``` with ```[vdrweb]``` prefix.