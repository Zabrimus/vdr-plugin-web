#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include "sharedmemory.h"

const std::string sharedMemoryFile("/cefbrowser");
const size_t sharedMemorySize = 3840 * 2160 * 4; // 4K

SharedMemory sharedMemory;

SharedMemory::SharedMemory() {
    int shmid = shm_open(sharedMemoryFile.c_str(), O_RDWR, 0666);
    if (shmid < 0) {
        shmid = shm_open(sharedMemoryFile.c_str(), O_EXCL | O_CREAT | O_RDWR, 0666);
        if (shmid >= 0) {
            ftruncate(shmid, sharedMemorySize);
        }
    }

    shmp = (uint8_t*)mmap(nullptr, sharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
    close(shmid);
}

SharedMemory::~SharedMemory() {
    // munmap(shmp, sharedMemorySize);
}

uint8_t* SharedMemory::Get() {
    return shmp;
}