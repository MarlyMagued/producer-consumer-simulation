# Producer-Consumer Problem
## Overview
**Producer-Consumer Problem:** A simulation of the bounded-buffer producer/consumer problem using shared memory and semaphores.


## Files in the Repository
- `producer.c`: Implements producer processes that generate commodity prices following a normal distribution and store them in shared memory.
- `consumer.c`: Implements a consumer process that reads prices from shared memory and displays them in a structured format.
- `cleanup.c`: A helper program to remove shared memory and semaphores after execution.
- `Makefile`: Contains rules for compiling the project.

## Compilation Instructions
To compile the project, use the provided Makefile:
```sh
make
```
This will generate the following executable files:
- `producer`
- `consumer`
- `cleanup`

## Running the Programs

### Producer
Each producer is started with the following command-line arguments:
```sh
./producer <commodity_name> <mean_price> <std_dev> <sleep_time_ms> <buffer_size>
```
Example:
```sh
./producer GOLD 1800 50 200 40
```

### Consumer
The consumer is started with the following command-line argument:
```sh
./consumer <buffer_size>
```
Example:
```sh
./consumer 40
```

### Cleanup
After execution, run the cleanup program to remove shared memory and semaphores:
```sh
make clean_sem
```

