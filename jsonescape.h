#pragma once

#include <string>
#include <cstdio>

// escape a string for the use inside a json string literal
inline std::string jsonEscape(const char *in) {
    std::string out;
    if (in == nullptr) {
        return out;
    }

    for (const char *p = in; *p; ++p) {
        unsigned char c = (unsigned char) *p;
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char) c;
                }
        }
    }

    return out;
}
