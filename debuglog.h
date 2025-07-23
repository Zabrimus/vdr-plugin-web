#pragma once

#ifdef DEBUG_THRIFTCLIENT

#ifdef VDRLOG
#include "vdr/tools.h"
#define DEBUGLOG(...) dsyslog(__VA_ARGS__)
#else
#define DEBUGLOG(format, ...) fprintf (stderr, format "\n", ##__VA_ARGS__)
#endif // VDRLOG

#else
#define DEBUGLOG(...)
#endif // DEBUG_THRIFTCLIENT
