# Compiler
CC = gcc

# Flags
CFLAGS = -pthread -lm

all : producer consumer
# Compile the program
producer: producer.c
	$(CC) -o producer producer.c $(CFLAGS)
	
consumer: consumer.c
	$(CC) -o consumer consumer.c $(CFLAGS)
	
# Clean up compiled files and shared memory
clean:
	rm -f $(TARGET)
	# Optionally, you can clean the shared memory manually here
	ipcrm -M /shared_buffer || true
	
clean_sem:
	$(CC) -o cleanup cleanup.c $(CFLAGS)
		./cleanup  # This will call the cleanup function in C to unlink semaphores and shared memory
		rm -f cleanup  # Clean up the cleanup program after running it

