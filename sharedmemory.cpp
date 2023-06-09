#include <cstdio>
#include <thread>
#include <sys/shm.h>
#include "sharedmemory.h"

const key_t sharedMemoryKey = (key_t)0xDEADC0DE;
const int sharedMemorySize = 1920 * 1080 * 4;

SharedMemory::SharedMemory() {
    // init shared memory
    shmid = -1;
    shmp = nullptr;

    shmid = shmget(sharedMemoryKey, sharedMemorySize, 0666 | IPC_CREAT | IPC_EXCL) ;

    if (errno == EEXIST) {
        shmid = shmget(sharedMemoryKey, sharedMemorySize, 0666);
    }

    if (shmid == -1) {
        perror("Unable to get shared memory");
        return;
    }

    shmp = (uint8_t *) shmat(shmid, nullptr, 0);
    if (shmp == (void *) -1) {
        perror("Unable to attach to shared memory");
        return;
    }
}

SharedMemory::~SharedMemory() {
    shutdown();
}

uint8_t* SharedMemory::Get() {
    return shmp;
}

void SharedMemory::shutdown() {
    shmdt(shmp);
    shmctl(shmid, IPC_RMID, 0);
}

SharedMemory sharedMemory;