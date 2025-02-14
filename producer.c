#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>  // For toupper

typedef struct {
    char commodity_name[11];
    double mean;
    double stddev;
    int sleep_time;
} ProducerArgs;

typedef struct {
    char commodityName[20];
    float price;
} BufferItem;

typedef struct {
    int buffer_size;
    int in;
    int out;
    sem_t *empty;
    sem_t *full;
    pthread_mutex_t mutex;
    BufferItem buffer[];  
} SharedBuffer;

SharedBuffer *sharedBuffer;

// Function to convert a string to uppercase
void to_uppercase(char *str) {
    for (; *str; ++str) {
        *str = toupper((unsigned char)*str);
    }
}

// Function to generate a random price based on normal distribution
double generate_price(double mean, double stddev) {
    double u1 = ((double) rand() / RAND_MAX);
    double u2 = ((double) rand() / RAND_MAX);
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + z0 * stddev;
}

// Function to get the current timestamp
void get_timestamp(char *buffer, size_t size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *timeinfo = localtime(&ts.tv_sec);
    snprintf(buffer, size, "%02d/%02d/%04d %02d:%02d:%02d.%03ld",
             timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, ts.tv_nsec / 1000000);
}

// Initialize shared memory and semaphores
void initializeSharedSpace(int buffer_size){
    
    // Create shared memory
    int shm_fd = shm_open("/shared_buffer", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to create shared memory");
        exit(EXIT_FAILURE);
    }

    // Get the current size of the shared memory
    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1) {
        perror("Failed to get shared memory size");
        exit(EXIT_FAILURE);
    }

    

    size_t shm_size = shm_stat.st_size;
    
    // If shared memory size is less than required, resize it
    if (shm_size < sizeof(SharedBuffer) + buffer_size * sizeof(BufferItem)) {
        if (ftruncate(shm_fd, sizeof(SharedBuffer) + buffer_size * sizeof(BufferItem)) == -1) {
            perror("Failed to resize shared memory");
            exit(EXIT_FAILURE);
        }
    }


    // Map the shared memory
    sharedBuffer = mmap(0, sizeof(SharedBuffer) + buffer_size * sizeof(BufferItem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sharedBuffer == MAP_FAILED) {
        perror("Failed to map shared memory");
        exit(EXIT_FAILURE);
    }

    // Initialize shared buffer
    sharedBuffer->buffer_size = buffer_size;
    sharedBuffer->in = 0;
    sharedBuffer->out = 0;

    // Initialize full and empty semaphores
    sharedBuffer->empty = sem_open("/sem_empty", O_CREAT | O_RDWR, 0666, buffer_size);
    if (sharedBuffer->empty == SEM_FAILED) {
        perror("Failed to create empty semaphore");
        exit(EXIT_FAILURE);
    }

    sharedBuffer->full = sem_open("/sem_full", O_CREAT | O_RDWR, 0666, 0);
    if (sharedBuffer->full == SEM_FAILED) {
        perror("Failed to create full semaphore");
        exit(EXIT_FAILURE);
    }


    // Initialize the mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sharedBuffer->mutex, &attr);

    //printf("Shared memory initialized with buffer size %d.\n", buffer_size);
}
// Producer thread function
void *producer_thread_func(void *arg) {
    ProducerArgs *params = (ProducerArgs *) arg;

    while (1) {
        double price = generate_price(params->mean, params->stddev);

        char timestamp[64];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(stderr, "\033[;31m[%s] %s: Generating new value %.2f\033[0m\n", 
                timestamp, params->commodity_name, price);

        fprintf(stderr, "\033[;31m[%s] %s: Trying to get mutex on shared buffer\033[0m\n", 
                timestamp, params->commodity_name);

        // Wait to access the shared buffer
        if (sem_wait(sharedBuffer->empty) == -1) {
            perror("sem_wait failed on empty semaphore");
            exit(EXIT_FAILURE);
        }

        if (pthread_mutex_lock(&sharedBuffer->mutex) != 0) {
            perror("Failed to lock mutex");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "\033[;31m[%s] %s: Placing %.2f on shared buffer\033[0m\n", 
                timestamp, params->commodity_name, price);

        // Convert commodity name to uppercase before placing in buffer
        char commodity_upper[20];
        strncpy(commodity_upper, params->commodity_name, sizeof(commodity_upper)-1);
        commodity_upper[sizeof(commodity_upper)-1] = '\0';
        to_uppercase(commodity_upper);

        // Add the price and commodity to the buffer
        BufferItem *item = &sharedBuffer->buffer[sharedBuffer->in];
        strncpy(item->commodityName, commodity_upper, sizeof(item->commodityName)-1);
        item->commodityName[sizeof(item->commodityName)-1] = '\0';
        item->price = (float)price;

        sharedBuffer->in = (sharedBuffer->in + 1) % sharedBuffer->buffer_size;

        if (pthread_mutex_unlock(&sharedBuffer->mutex) != 0) {
            perror("Failed to unlock mutex");
            exit(EXIT_FAILURE);
        }

        if (sem_post(sharedBuffer->full) == -1) {
            perror("sem_post failed on full semaphore");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "\033[;31m[%s] %s: Sleeping for %d ms\033[0m\n", 
                timestamp, params->commodity_name, params->sleep_time);

        // Sleep for the specified interval
        usleep(params->sleep_time * 1000);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <CommodityName> <Mean> <StdDev> <SleepTime(ms)> <BufferSize>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command-line arguments
    ProducerArgs producer_args;
    strncpy(producer_args.commodity_name, argv[1], sizeof(producer_args.commodity_name)-1);
    producer_args.commodity_name[sizeof(producer_args.commodity_name)-1] = '\0';
    producer_args.mean = atof(argv[2]);
    producer_args.stddev = atof(argv[3]);
    producer_args.sleep_time = atoi(argv[4]);
    int buffer_size = atoi(argv[5]);

    // Initialize shared space (create_flag = 1)
    initializeSharedSpace(buffer_size);

    // Start producer thread
    pthread_t producer_thread;
    if (pthread_create(&producer_thread, NULL, producer_thread_func, &producer_args) != 0) {
        perror("Failed to create producer thread");
        exit(EXIT_FAILURE);
    }

    // Join producer thread (infinite loop, so it runs indefinitely)
    pthread_join(producer_thread, NULL);

    return 0;
}
