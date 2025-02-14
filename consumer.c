#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>
#include <ctype.h>  // For tolower or toupper
#include <errno.h>
#include <locale.h>

#define HISTORY_SIZE  4
#define NUM_COMMODITIES 11

// To track each buffer commodity
typedef struct {
    float history[HISTORY_SIZE]; // Keep history of current and last 4 updates
    int count;
    float avgPrice;
    float lastAvgPrice;
} CommodityData;

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

// Commodities array (all uppercase)
const char *commodities[] = {
    "ALUMINIUM", "COPPER", "COTTON", "CRUDEOIL", "GOLD",
    "LEAD", "MENTHAOIL", "NATURALGAS", "NICKEL", "SILVER", "ZINC"
};

SharedBuffer *sharedBuffer;
// Used to keep track of commodity history
CommodityData commodityData[NUM_COMMODITIES];

// Function to convert a string to uppercase
void to_uppercase(char *str) {
    for (; *str; ++str) {
        *str = toupper((unsigned char)*str);
    }
}


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
int get_commodity_index(const char *commodity) {
    for (int i = 0; i < NUM_COMMODITIES; i++) {
        if (strcmp(commodities[i], commodity) == 0) {
            return i;
        }
    }
    return -1;
}
void display(int commodityIndex, float price) {
    if (commodityIndex != -1) {// make sure that the commodity exists
        // Updating price history with last commodity added 
        CommodityData *data = &commodityData[commodityIndex];
        data->history[data->count % HISTORY_SIZE] = price;
        data->count++;
        
        float sum = 0.0;
        int n = (data->count < HISTORY_SIZE) ? data->count : HISTORY_SIZE;// get number of commodity
        for (int i = 0; i < n; i++) {
            sum += data->history[i];
        }
        data->lastAvgPrice = data->avgPrice;//last avg price before adding last commodity
        data->avgPrice = sum / n;//calculate avg of all commodities

        // Print dashboard
        system("clear");
        printf("+-------------------+-----------+------------+\n");
        printf("| Commodity         | Price     | AvgPrice   |\n");
        printf("+-------------------+-----------+------------+\n");
        for (int i = 0; i < NUM_COMMODITIES; i++) {
            CommodityData *data = &commodityData[i];

            //check that the history array is not emty and get current price
            float currentPrice = (data->count > 0) ? data->history[(data->count - 1) % HISTORY_SIZE] : 0.00;
            //check that the history array is not emty and get price before the current price
            float lastPrice = (data->count > 1) ? data->history[(data->count - 2) % HISTORY_SIZE] : currentPrice;

            // Determine price trend and color 
            const char *priceColor = (data->count > 1) ? 
                (currentPrice > lastPrice ? "\033[;32m" : "\033[;31m") : "\033[;34m"; // Green ↑, Red ↓, Blue for stable
            const char *priceTrend = (data->count > 1) ? 
                (currentPrice > lastPrice ? "↑" : "↓") : " ";

            // Determine avgprice trend and color 
            const char *avgColor = (data->count > 1) ? 
                (data->avgPrice > data->lastAvgPrice ? "\033[;32m" : "\033[;31m") : "\033[;34m"; // Green ↑, Red ↓, Blue for stable
            const char *avgTrend = (data->count > 1) ? 
                (data->avgPrice > data->lastAvgPrice ? "↑" : "↓") : " ";

            // Format strings with color and trends
            char priceStr[40], avgPriceStr[40];
            snprintf(priceStr, sizeof(priceStr), "%s%7.2lf %s\033[0m", priceColor, currentPrice, priceTrend);
            snprintf(avgPriceStr, sizeof(avgPriceStr), "%s%7.2lf %s\033[0m", avgColor, data->avgPrice, avgTrend);
            
        

            // Display commodity data
            printf("| %-17s | %-17s | %-17s  |\n", commodities[i], priceStr, avgPriceStr);

        }
        printf("+-------------------+-----------+------------+\n");
    }
}

void *consumer_thread_func(void *arg) {
    while (1) {
        fprintf(stderr, "Waiting on full semaphore...\n");
        if (sem_wait(sharedBuffer->full) == -1) {
            perror("sem_wait failed on full semaphore");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&sharedBuffer->mutex);
        fprintf(stderr, "Mutex locked. Consuming item...\n");

        // Retrieve the commodity from the buffer
        BufferItem item = sharedBuffer->buffer[sharedBuffer->out];
        sharedBuffer->out = (sharedBuffer->out + 1) % sharedBuffer->buffer_size;

        // Convert commodityName to uppercase to ensure case-insensitive comparison
        char commodity_upper[20];
        strncpy(commodity_upper, item.commodityName, sizeof(commodity_upper)-1);
        commodity_upper[sizeof(commodity_upper)-1] = '\0';
        to_uppercase(commodity_upper);

        int index = get_commodity_index(commodity_upper);
        if (index == -1) {
            fprintf(stderr, "Unrecognized commodity: %s\n", item.commodityName);
        } else {
            display(index, item.price);
        }

        pthread_mutex_unlock(&sharedBuffer->mutex);
        fprintf(stderr, "Item consumed. Releasing empty semaphore...\n");
        if (sem_post(sharedBuffer->empty) == -1) {
            perror("sem_post failed on empty semaphore");
            exit(EXIT_FAILURE);
        }

        // Simulate consumer processing delay
       usleep(500000);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <BufferSize>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int buffer_size = atoi(argv[1]);
    initializeSharedSpace(buffer_size);

    pthread_t consumer_thread;
    if (pthread_create(&consumer_thread, NULL, consumer_thread_func, NULL) != 0) {
        perror("Failed to create consumer thread");
        exit(EXIT_FAILURE);
    }
    pthread_join(consumer_thread, NULL);

    return 0;
}
