#pragma once

#include <cstdint>

class SharedMemory {
    private:
        int shmid;
        uint8_t *shmp;

    public:
        explicit SharedMemory();
        ~SharedMemory();

        uint8_t* Get();

        void shutdown();
};

extern SharedMemory sharedMemory;
