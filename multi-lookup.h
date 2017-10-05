#ifndef MULTI_LOOKUP_H 
#define MULTI_LOOKUP_H 

/*
 * Structures for passing arguments to threads
 */

/*
 * Description:		Holds arguments passed to threadRead function
 *
 * @*fileName		file name for input file
 * @*q			pointer to the queue
 * @*qLock		mutex lock for synchonization of queue
 * @*countLock		mutex lock for synchronization of the thread counter
 */
typedef struct {
        char *fileName;
        queue *q;
        pthread_mutex_t *qLock;
        pthread_mutex_t *countLock;
} readArg;

/*
 * Description:		Holds arguments passed to threadWrite function
 *
 * @*file		file pointer to output file
 * @*q			pointer to the queue
 * @*qLock		mutex lock for synchonization of queue
 * @*outLock		mutex lock for synchronization of output file
 */
typedef struct {
        FILE *file;
        queue *q;
        pthread_mutex_t *qLock;
        pthread_mutex_t *outLock;
} writeArg;


/*
 * Function Prototypes
 */

/* @param args		a struct writeArg containing arguments defined above
 *
 * Description:		Pops URL's from the queue, looks up the respective
 * 			ip addresses and writes them to the output file
 */
void* threadWrite(void* args);

/* @param args		a struct readArg containing arguments defined above
 *
 * Description:		Reads data from one of the input files and pushes
 * 			the URL's to the queue
 */
void* threadRead(void* args);

#endif
