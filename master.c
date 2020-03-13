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

int sig_int = 0;
unsigned cn = 0;

typedef struct {
    // from man 2 frok: The child does not inherit semaphore adjustments from its parent
	sem_t sem_t; // so sem_t goes in shared memory
	int array[2048];
} sharedData_t;

void processSignal(int signum) {
	switch (signum) {
		case SIGINT:
            sig_int =1 ;
			break;
        default:
            fprintf(stderr,"ignored signal: %d\n", signum);
            //all other signals to be ignored
            break;
	}
}


void* createMemoryBlock(char* fpath, unsigned size) {
	int memFd = shm_open(fpath, O_RDWR|O_CREAT, 0600);
	check_error(memFd != -1, "shm_open failed");

	check_error(ftruncate(memFd, size) != -1, "ftruncate failed");

	void* addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, memFd, 0);
	check_error(addr != MAP_FAILED, "mmap failed");

	close(memFd);
	return addr;
}

unsigned number_of_ints(char *fpath){
	FILE* f = fopen(fpath, "r");
	check_error(f != NULL, "fopen failed");

	unsigned x = 0;
    for(;fscanf(f,"%*d")!=EOF;x++){};

	fclose(f);
    return x;
}

void child_part_1(unsigned n, sharedData_t* wrapper){
    if(n == 1)
        return;

    n /= 2;

    time_t start = time(NULL);

    for(unsigned i = 0; i < n; i++) {
        pid_t cur_pid = fork();
        check_error(cur_pid != -1, "fork failed");
        cn += 1; // keep track of active processes
        if(cn > 20){
            fprintf(stderr,"can't allow more than 20 processes at a time!\n");
            check_error(killpg(getpid(), SIGQUIT) != -1, "killing failed"); // kills all child processes
            exit(EXIT_FAILURE);
        }
        if(cur_pid == 0) {
            char x[16];
            char y[16];
            snprintf(x,16,"%u",i); //index number 1
            snprintf(y,16,"%u",i+n); //index number 2
            check_error(execl("./bin_adder", "bin_adder", x,  y, (char *) NULL) != -1, "executing bin_adder failed");
            exit(0);
        }
    }

    while(cn>0){
        check_error(signal(SIGINT,processSignal) != SIG_ERR, "setting signal failed");
        check_error(wait(NULL) != -1, "waiting for godot"); //waits for any child to finish
        cn -= 1;

        if(time(NULL)-start > 100){
            check_error(killpg(getpid(), SIGQUIT) != -1, "killing failed");
            printf("Children took more than 100 seconds to execute, terminating...\n");
            exit(EXIT_FAILURE);
        }
        if(sig_int == 1){
            printf("\n^C captured, terminating...\n");
            check_error(killpg(getpid(), SIGQUIT) != -1, "killing failed");
            exit(EXIT_FAILURE);
        }
    }

    child_part_1(n, wrapper);
}

void child_part_2(unsigned n, sharedData_t* wrapper){
    n /= 2;
    for(unsigned i=0; i<n; i++){
        pid_t cur_pid = fork();
        check_error(cur_pid != -1, "fork failed");
        if(cur_pid == 0) {
            char x[16];
            char y[16];
            snprintf(x,16,"%u",2*i);    // sum two sequential numbers
            snprintf(y,16,"%u",2*i+1);
            check_error(execl("./bin_adder", "bin_adder", x,  y, (char *) NULL) != -1, "executing bin_adder failed");
            exit(0);
        }
    }
    for(unsigned i=0; i<n; i++){
        wait(NULL);
    }

    child_part_1(n, wrapper);
}

void child_argv(unsigned n, sharedData_t* wrapper, char* part){
    int a = atoi(part);
    if( a==1 ){
        child_part_1(n, wrapper);
    }
    else if(a == 2){
        child_part_2(n, wrapper);
    }
    else{
        check_error(1, "there are only part one and two");
    }
}

int main(int argc, char *argv[]) {
    check_error(argc == 2, "./master [1,2]");

    unsigned n = number_of_ints("integers.txt");

    sharedData_t* wrapper = createMemoryBlock("/shared_m", sizeof(sharedData_t));

	FILE* f = fopen("integers.txt", "r");
	check_error(f != NULL, "fopen on integers failed");
    for(unsigned i=0; fscanf(f, "%d", &wrapper->array[i]) != EOF; i++){};
	fclose(f); //integers are in shared memory now

	check_error((sem_init(&(wrapper->sem_t), 1, 1) != -1), "initializing semaphore failed");
    check_error((sem_post(&(wrapper->sem_t)) != -1), "sem_post failed");

    child_argv(n, wrapper, argv[1]);

    printf("Sum of the numbers is: %d\n", wrapper->array[0]);

	check_error((sem_destroy(&(wrapper->sem_t)) != -1), "destroying semaphore failed");
	check_error(munmap(wrapper, sizeof(sharedData_t)) != -1, "unpaming wrapper failed");
	check_error(shm_unlink("/shared_m") != -1 , "unlinking /sharem_m failed");
    return 0;
}
