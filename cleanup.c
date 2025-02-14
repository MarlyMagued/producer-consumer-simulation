#include <semaphore.h>
#include <sys/mman.h>
#include <stdio.h>

void cleanup() {
    // Unlink semaphores
    sem_unlink("/sem_empty");
    sem_unlink("/sem_full");

    // Unlink shared memory
    shm_unlink("/shared_buffer");

    printf("Cleanup completed: semaphores and shared memory unlinked.\n");
}

int main() {
    cleanup();
    return 0;
}
