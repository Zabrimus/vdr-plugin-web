#pragma once

#include <cstdio>
#include <cstdint>

class SharedMemory {
private:
    uint8_t *shmp;

public:
    explicit SharedMemory();
    ~SharedMemory();

    uint8_t* Get();
};

extern SharedMemory sharedMemory;