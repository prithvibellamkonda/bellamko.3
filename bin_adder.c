#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>

#define KEY 0x1111

#define check_error(expr,userMsg)\
	do {\
		if (!(expr)) {\
			perror(userMsg);\
			exit(EXIT_FAILURE);\
		}\
	} while (0)

typedef struct {
	sem_t sem_t;
	int array[2048];
} sharedData_t;


void* getMemoryBlock(char* fpath, unsigned size) {
	int memFd = shm_open(fpath, O_RDWR, 0600);
	check_error(memFd != -1, "...");

	void* addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, memFd, 0);
	check_error(addr != MAP_FAILED, "mmap");

	close(memFd);

	return addr;
}

void writeLog(int x, int y, int* array){
    FILE* log = fopen("adder.log", "a");
	check_error(log != NULL, "failed opening log file");
    time_t now = time(NULL);
    fprintf(log, "pid: %u, index: %d, size: %d, %s", getpid(), x, y, ctime(&now));
    fprintf(log, "\tadding array[%d](%d) and array[%d](%d) and writting result(%d) in array[%d]\n",
            x, array[x], x+y, array[y], array[x] + array[y], x);
    fclose(log);
}

void writteAttempt(unsigned i){
    time_t now = time(NULL);
    fprintf(stderr, "pid: %u, trying to get into cricital section, attempt number: %u, %s", getpid(), i+1, ctime(&now));
}

void writteCritical(unsigned i){
    time_t now = time(NULL);
    fprintf(stderr, "pid: %u, got in, %u. try, %s", getpid(), i+1, ctime(&now));
}

int main(int argc, char *argv[]) {
    check_error(argc==3, "./bin_adder yy xx");
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);

    time_t now = time(NULL);
    srand(now);
    sleep(rand()%4); // each child sleeps for 0-3 seconds

	sharedData_t* wrapper = getMemoryBlock("/shared_m", sizeof(sharedData_t));

    unsigned i;
    for(i=0; i<5; i++){
	    //int err = sem_wait(&(wrapper->sem_t)); //makes sure every child goes into critical section
	    int err = sem_trywait(&(wrapper->sem_t));
        writteAttempt(i);
        if(err == -1){
            sleep(1); // fait for next turn
            continue;
        }
        writteCritical(i);
        sleep(1); // sleep one sec on entering critical section
        writeLog(x,y,wrapper->array);
        wrapper->array[x] = wrapper->array[x] +  wrapper->array[y];
        sleep(1); // sleep one sec before exiting critical section
	    check_error(sem_post(&(wrapper->sem_t)) != -1, "failed indicating critical section has been freed");
        break;
    }

	check_error(munmap(wrapper, sizeof(sharedData_t)) != -1, "munmap failed");

    if(i<5){
        exit(EXIT_SUCCESS);
    }
    else{
        fprintf(stderr, "pid: %u, didn't get to enter critical section\n",getpid());
        exit(EXIT_FAILURE);
    }

    return 0;
}

