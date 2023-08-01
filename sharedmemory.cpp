#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <vdr/tools.h>
#include "sharedmemory.h"

const std::string sharedMemoryFile("/cefbrowser");
const size_t sharedMemorySize = 3840 * 2160 * 4; // 4K

SharedMemory sharedMemory;

SharedMemory::SharedMemory() {
    int shmid = shm_open(sharedMemoryFile.c_str(), O_RDWR, 0666);
    if (shmid < 0) {
        mode_t old_umask = umask(0);
        shmid = shm_open(sharedMemoryFile.c_str(), O_EXCL | O_CREAT | O_RDWR, 0666);
        umask(old_umask);
        if (shmid >= 0) {
            ftruncate(shmid, sharedMemorySize);
        } else if (errno == EEXIST) {
            shmid = shm_open(sharedMemoryFile.c_str(), O_RDWR, 0666);
        }
    }

    if (shmid < 0) {
        esyslog("Could not open shared memory (shm_open): %s", strerror(errno));
        exit(1);
    }

    shmp = (uint8_t*)mmap(nullptr, sharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);

    if (shmp == MAP_FAILED) {
        esyslog("Could not open shared memory (mmap): %s", strerror(errno));
        exit(1);
    }

    close(shmid);
}

SharedMemory::~SharedMemory() {
    munmap(shmp, sharedMemorySize);
    shm_unlink(sharedMemoryFile.c_str());
}

uint8_t* SharedMemory::Get() {
    return shmp;
}