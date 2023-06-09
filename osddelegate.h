#pragma once

#include <string>
#include <vdr/osdbase.h>

enum OSDType { MENU, HTML, CLOSE, OPEN_VIDEO };

class OSDDelegate {
public:
    static OSDType osdType;

public:
    OSDDelegate();
    cOsdObject* get(const char *title);
};
