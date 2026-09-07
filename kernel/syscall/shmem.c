#include "syscall.h"
#include "shmem.h"
#include "process.h"
#include "mm/shmem.h"

i32 syscall_shmget(u32 key, u32 size, i32 shmflg) {
    return shmem_get(key, size, shmflg);
}

void *syscall_shmat(i32 shmid, const void *shmaddr, i32 shmflg) {
    (void)shmflg;
    uintptr_t address = shmem_attach(process_current(), shmid, (uintptr_t)shmaddr);
    return address ? (void *)address : (void *)-12;
}

i32 syscall_shmdt(const void *shmaddr) {
    return shmem_detach(process_current(), (uintptr_t)shmaddr);
}

i32 syscall_shmctl(i32 shmid, i32 cmd, void *buffer) {
    return shmem_control(process_current(), shmid, cmd, buffer);
}