//---------------------------------------------------------------------------------------------------------------------
// poc.c
// proof of concept of the centralised algorithm 
// reference :- https://gist.github.com/inJeans/f432079f1eba4a632f23982716ac9943
//---------------------------------------------------------------------------------------------------------------------

#include <mpi.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#define COORDINATOR 0  // rank of the coordinator
// Function prototype
void WriteToFile(char *pFilename, int *pMatrix, int inRow);

int main(int argc, char* argv[]) {

    int numtasks, rank; // number of processors and the rank of current processor
    int send_buffer = 1;    // information we send for request
    int receive_buffer;     // to store the recieving value
    int mutex_request = 1;  // tag for the request
    int mutex_reply = 2;    // tag for the reply request
    int mutex_release = 3;  // tag for the release request
    int *request_queue;     // queue to store the requestes in fifo order
    int queue_length = 0;
    int num_completed_requests = 0; 
    bool mutex_avail = true;
    // Initialise MPI environment
    MPI_Status stat;  // this will help us retrive the tag and the rank of the sender

    MPI_Init(&argc,
             &argv);
    MPI_Comm_size(MPI_COMM_WORLD,
                  &numtasks);       // total number of proccessor
    MPI_Comm_rank(MPI_COMM_WORLD,   // rank of the current proccessor
                  &rank);

    // Dynamically allocate request queue, based on number of tasks
    request_queue = (int*) malloc(numtasks * sizeof(int));

    // Coordinator rank
    if(rank == COORDINATOR) {
        // Listen for requests until all remaining tasks have
        // asked for access to the critical section
        while(num_completed_requests < numtasks - 1) {
            MPI_Recv(&receive_buffer,               // listen to anyb request send to proccess 0
                          1,
                          MPI_INT,
                          MPI_ANY_SOURCE,
                          MPI_ANY_TAG,
                          MPI_COMM_WORLD,
                          &stat);
            // If a request has been made and the critical section is
            // available
            // send tag mutex reply so the proccessor know the critical section is available
            if(stat.MPI_TAG == mutex_request && mutex_avail) {
                printf("No queue, access granted to Rank %i\n", stat.MPI_SOURCE);
                fflush(stdout);
                MPI_Send(&send_buffer,
                              1,
                              MPI_INT,
                              stat.MPI_SOURCE,
                              mutex_reply,
                              MPI_COMM_WORLD);
                mutex_avail = false;
            }
            // If a request has been made but the critical sections is
            // not available
            // do not reply and add the request to queue
            else if(stat.MPI_TAG == mutex_request && !mutex_avail) {
                queue_length += 1;
                request_queue[queue_length-1] = stat.MPI_SOURCE;
                printf("Rank %i is waiting....\n", stat.MPI_SOURCE);
            }
            // If a process has finished with the critical section
            //make the critical section available
            else if(stat.MPI_TAG == mutex_release) {
                mutex_avail = true;
                num_completed_requests += 1;
                
            }
            

            // If there are processes waiting in the queue and the critical section is available
            // release the critical section to the next proccess in line
            // and remove that proccess from the queue
            if(queue_length > 0 &&  mutex_avail) {
                mutex_avail = false;
                MPI_Send(&send_buffer,
                              1,
                              MPI_INT,
                              request_queue[0],
                              mutex_reply,
                              MPI_COMM_WORLD);
                printf("access granted to Rank %i\n", request_queue[0]);
                fflush(stdout);
                for(int process=0; process<queue_length; process++) {
                    request_queue[process] = request_queue[process+1];
                    }
                 queue_length -= 1;
                
            }
        }
    } 
    else {
        // Request access to the critical section
        MPI_Send(&send_buffer,
                      1,
                      MPI_INT,
                      COORDINATOR,
                      mutex_request,
                      MPI_COMM_WORLD);
                printf("Rank %i has requested access to critical section\n", rank);
        fflush(stdout);
        // Block until we have received access to the
        // critical section
        //coordinator replied which means critical section is available
        MPI_Recv(&receive_buffer,
                      1,
                      MPI_INT,
                      COORDINATOR,
                      mutex_reply,
                      MPI_COMM_WORLD,
                      &stat);

        // critical section available
        // calculate the prime numbers for the range
        printf("Rank %i is in the critical section...\n", rank);
        fflush(stdout);
        int n=500000; // number till we have to found prime number
        int *primeArray = NULL; // array to store prime number
        int p = numtasks-1; // range has to be divided by toatal number of proccesses -1 
        int my_rank=rank-1; // starrting point of the rank
        int sqrt_i;
	    bool prime;
	
	    int rpt = n / p; 
	    int rptr = n % p; // rpt = rows per thread remainder
	
	    int sp = my_rank * rpt; // starting point for calculation
	    int ep = sp + rpt;  // endpoint of the calculation
	    if(my_rank == p-1)
		    {ep += rptr;}
	    int arraySize = ep-sp;
	    primeArray = (int*)malloc( arraySize* sizeof(int));  // initializing array to store the prime numbers
	    for (int z = 0; z< (ep-sp); z++){
	        primeArray[z] = 0;
	    }   
	
	    int k =0;
	    
	    // finding all the prime numbers
	    for( int i =sp ; i<=ep ; i=i+1)
        {
            if(i ==1){ 
            continue;}
            sqrt_i =sqrt(i);
            prime = true;
            for (int j = 2 ; j<= sqrt_i ; j++){
                if(i%j == 0){       //check if the remainder is zero 
                    prime = false;
                }
            }
            if (prime == true){
                primeArray[k++] =  i;     //storing the prime number only in array
            }
        }
	
	    printf(" rank %i finally completed calculation\n", rank);
	    char str[20];
        snprintf(str, 20, "process_%d.txt", rank); // puts string into buffer
        WriteToFile(str,primeArray,arraySize); // store the array into file

        printf("Rank %i finished with critical section, signalling Coordinator\n", rank);
        fflush(stdout);
        // Let the coordinator know we have finished with the
        // critical section
        MPI_Send(&send_buffer,
                      1,
                      MPI_INT,
                      COORDINATOR,
                      mutex_release,
                      MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}

// function to write the array of primes into file
void WriteToFile(char *pFilename, int *pMatrix, int inRow)
{
	int i;
	FILE *pFile = fopen(pFilename, "w");
	for(i = 0; i < inRow; i++){
	    if(pMatrix[i] != 0){     // writing only prime number in array into file
		fprintf(pFile, "%d\t", pMatrix[i]);
		fprintf(pFile, "\n");}
	}
	fclose(pFile);
}

