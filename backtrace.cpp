#include <vdr/tools.h>
#include <vdr/thread.h>
#include "backtrace.h"

void printBacktrace() {
    cStringList stringList;

    cBackTrace::BackTrace(stringList, 0, false);
    esyslog("[vdrweb] Backtrace size: %d", stringList.Size());
    for (int i = 0; i < stringList.Size(); ++i) {
        esyslog("[vdrweb] ==> %s", stringList[i]);
    }

    esyslog("[vdrweb] ==> Caller: %s", *cBackTrace::GetCaller());
}