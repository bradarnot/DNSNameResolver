/*
 * File: multi-lookup.c
 * Author: Bradley Arnot
 * Create Date: 02/27/2016
 * Modify Date: 03/06/2016
 * Description:
 *	Multi-threaded domain name resolver
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "queue.h"
#include "util.h"
#include "multi-lookup.h"

#define MINARGS 3
#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define USAGE "<inputFilePath1> ... <inputFilePathN> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"

//Global vars
//Counts the number of running requester threads
unsigned int reqThreadCount = 0;

int main(int argc, char* argv[]){

	unsigned int i = 0;
	// Number of input files and number of requester threads to run
	unsigned int requesterSize = ((unsigned int)argc)-2;

	// Keep track of struct pointers passed to threads
	// For purpose of being able to free the memory
	readArg **reqArgs = malloc(sizeof(readArg*) * requesterSize);
	writeArg **resArgs = malloc(sizeof(writeArg*) * MAX_RESOLVER_THREADS);

	// Thread Pools
	pthread_t *requesterPool = malloc(sizeof(pthread_t)*requesterSize);
	pthread_t *resolverPool = malloc(sizeof(pthread_t)*MAX_RESOLVER_THREADS);

	// Mutex locks
	pthread_mutex_t outLock;	//Synchro writing to output file
	pthread_mutex_t qLock;		//Synchro pushing and popping data in queue
	pthread_mutex_t countLock;	//Synchro access to reqThreadCount variable
	pthread_mutex_init(&outLock, NULL);
	pthread_mutex_init(&qLock, NULL);
	pthread_mutex_init(&countLock, NULL);

	// Declare and initialize the queue
	queue q;
	queue_init(&q, 50);

	
	// Check if there are too little or too many input files
	if(argc < MINARGS){
		fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
	} else if(argc-2 >= MAX_INPUT_FILES) {
		fprintf(stderr, "Too many files: %d\n", (argc - 2));
		fprintf(stderr, "Please use no more than 10 files");
		return EXIT_FAILURE;
	}

	// Create requester threads
	for(i=1; i<(((unsigned int)argc)-1); i++){
		// Add arguments to struct
		readArg *arg = malloc(sizeof(readArg));
		arg->fileName = argv[i];
		arg->q = &q;
		arg->qLock = &qLock;
		arg->countLock= &countLock;
		pthread_mutex_lock(&countLock);
		reqThreadCount++;
		pthread_mutex_unlock(&countLock);
		pthread_create(requesterPool + (i-1), NULL, threadRead, arg);
		//Keep track of dem pointers too
		reqArgs[i-1] = arg;
	}

	//Create resovler threads
	FILE *outFile = fopen(argv[argc-1], "w");
	for(i = 0; i < MAX_RESOLVER_THREADS; i++) {
		//Create struct to hold arguments
		writeArg *arg = malloc(sizeof(writeArg));
		arg->file = outFile;
		arg->q = &q;
		arg->qLock = &qLock;
		arg->outLock = &outLock;
		pthread_create(resolverPool + i, NULL, threadWrite, arg);
		// Keeping track of the pointers
		resArgs[i] = arg;
	}
	// Wait for requester threads to finish
	for(i=0; i<(((unsigned int)argc)-2); i++){
		pthread_join(requesterPool[i], NULL);
	}
	// Wait for resolver threads to finish
	for(i = 0; i < MAX_RESOLVER_THREADS; i++) {
		pthread_join(resolverPool[i], NULL);
	}	
	//Free all dat memory up
	//We don't want no memory leaks round here
	//Free argument memory
	for(i = 0; i < requesterSize; i++) {
		free(reqArgs[i]);
	}
	free(reqArgs);
	for(i = 0; i < MAX_RESOLVER_THREADS; i++) {
		free(resArgs[i]);
	}
	free(resArgs);

	fclose(outFile);
	
	//Free thread pools and queue
	free(requesterPool);
	free(resolverPool);
	queue_cleanup(&q);

	//Destroy Mutex Locks
	pthread_mutex_destroy(&qLock);
	pthread_mutex_destroy(&countLock);
	pthread_mutex_destroy(&outLock);
	return EXIT_SUCCESS;
}


/*
 * @param args		struct pointer that holds arguments
 *
 * Description		Gets data from the queue, finds the ip
 *					for each URL, then writes the URL and 
 * 					ip to the output file
 */
void* threadWrite(void *args) {
	//Seed random number generator
	srand(time(NULL));
	writeArg *arg = (writeArg*)(args);
	char ipStr[MAX_IP_LENGTH];		//Holds next ip address
	//Count number of ip's processed by thread
	int count = 0;
	//Loop while the requester threads are still working
	// or while there is still stuff to pop from the queue
	while(!queue_is_empty(arg->q) || reqThreadCount > 0) {
		while(queue_is_empty(arg->q)) {
			usleep(rand() % 101);
		}
		//Lock qLock and pop data from the queue
		pthread_mutex_lock(arg->qLock);
		char *nextURL = (char*)queue_pop(arg->q);
		pthread_mutex_unlock(arg->qLock);
		// Lookup hostname and get IP string 
		if(dnslookup(nextURL, ipStr, sizeof(ipStr)) == UTIL_FAILURE){
			fprintf(stderr, "dnslookup error: %s\n", nextURL);
			strncpy(ipStr, "", sizeof(ipStr));
		}
		//Write URL and IP to result file
		//Lock outLock to prevent multiple threads from writing
		//at the same time
		pthread_mutex_lock(arg->outLock);
		fprintf(arg->file, "%s,%s\n", nextURL, ipStr);
		pthread_mutex_unlock(arg->outLock);
		count++;
		free(nextURL);
	}
	printf("Resolver thread processed %d hostnames from queue.\n", count);
	return NULL;
}

/*
 * @param args		struct pointer that holds arguments
 *
 * Description		Reads in data from inputFile and adds each
 *					URL to the queue
 */
void* threadRead(void* args) {
	//Seed random number generator
	srand(time(NULL));
	readArg *arg = (readArg*) args;
	//Holds next line of file
	char *line = malloc(MAX_NAME_LENGTH*sizeof(char));
	//Input file to read from
	FILE* inputfp = fopen(arg->fileName, "r");
	//Count number of hostnames read by each thread
	int count = 0;
	//Read URL's line by line
	while(!feof(inputfp) && fscanf(inputfp, "%1024s", line)) {
		if(feof(inputfp))
			break;
		//While the queue is full, wait
		while(queue_is_full(arg->q)) {
			usleep(rand() % 101);
		}
		//Push data to the queue
		//Lock qLock to prevent other threads from accessing it
		//while data is being pushed
		pthread_mutex_lock(arg->qLock);
		if(queue_push(arg->q, (void*)line) == QUEUE_FAILURE) {
			fprintf(stderr, "Failed to push to queue: %s\n", line);
		}
		pthread_mutex_unlock(arg->qLock);
		line = malloc(MAX_NAME_LENGTH*sizeof(char));
		count++;
	}
	//Free memory
	free(line);
	fclose(inputfp);
	//Decrement running requester thread count
	//Lock to prevent other threads from changing the count
	//at the same time
	pthread_mutex_lock(arg->countLock);
	reqThreadCount--;
	pthread_mutex_unlock(arg->countLock);
	printf("Requester thread processed %d hostnames from queue.\n", count);
	return NULL;
}
