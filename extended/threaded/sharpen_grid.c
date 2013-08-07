#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include<malloc.h>

#define IMG_HEIGHT 480
#define IMG_WIDTH 720
#define NUM_ROW_THREADS (6)
#define NUM_COL_THREADS (8)
#define IMG_H_SLICE (IMG_HEIGHT/NUM_ROW_THREADS)
#define IMG_W_SLICE (IMG_WIDTH/NUM_COL_THREADS)


#define NUMBER_THREADS 1800
#define TOTAL_FRAMES 24
typedef double FLOAT;

//pthread_t threads[NUM_ROW_THREADS*NUM_COL_THREADS];
pthread_t threads[NUMBER_THREADS];
typedef struct _threadArgs
{
    int thread_idx;
    int i;
    int j;
    int h;
    int w;
} threadArgsType;

threadArgsType threadarg[NUM_ROW_THREADS*NUM_COL_THREADS];
pthread_attr_t fifo_sched_attr;
pthread_attr_t orig_sched_attr;
struct sched_param fifo_param;




// POSIX thread declarations and scheduling attributes
//
pthread_t main_thread;
pthread_attr_t rt_sched_attr;
pthread_attr_t main_sched_attr;
int rt_max_prio, rt_min_prio, min, old_ceiling, new_ceiling;
struct sched_param rt_param;
struct sched_param nrt_param;
struct sched_param main_param;







typedef unsigned int UINT32;
typedef unsigned long long int UINT64;
typedef unsigned char UINT8;

// PPM Edge Enhancement Code


#define K 4.0

//FLOAT PSF[9] = {-K/8.0, -K/8.0, -K/8.0, -K/8.0, K+1.0, -K/8.0, -K/8.0, -K/8.0, -K/8.0};
//FLOAT PSF[9] = {-K/80.0, -K/80.0, -K/80.0, -K/80.0, K+10.0, -K/80.0, -K/80.0, -K/80.0, -K/80.0};


void *frameThread(void * threadNumber)
{


int fdin,fdout,bytesLeft,thNum,i,j,bytesRead;
char fileNameIn[50], fileNameOut[50];
UINT8 header[22];
        UINT8 *R=(UINT8 *)malloc(sizeof(UINT8)*IMG_HEIGHT*IMG_WIDTH);
UINT8 *G=(UINT8 *)malloc(sizeof(UINT8)*IMG_HEIGHT*IMG_WIDTH);
UINT8 *B=(UINT8 *)malloc(sizeof(UINT8)*IMG_HEIGHT*IMG_WIDTH);
UINT8 *convR=(UINT8 *)malloc(sizeof(UINT8)*IMG_HEIGHT*IMG_WIDTH);
UINT8 *convG=(UINT8 *)malloc(sizeof(UINT8)*IMG_HEIGHT*IMG_WIDTH);
UINT8 *convB=(UINT8 *)malloc(sizeof(UINT8)*IMG_HEIGHT*IMG_WIDTH);
FLOAT temp;

        //printf("Threadnumber is %d",threadNumber);
     //fflush(stdout);
for (thNum = (int) threadNumber; thNum< TOTAL_FRAMES; thNum+= NUMBER_THREADS)
{

/* open ops*/
sprintf(fileNameIn,"image%04d.ppm",thNum+1);
//printf("input file : %s\n\r",fileNameIn);
if((fdin = open(fileNameIn, O_RDONLY, 0644))<0)
{
//printf("no file : %s\n\r",fileNameIn);
exit(1);
}


/*read ops*/
bytesLeft=21;
do
{
         //printf("bytesRead=%d, bytesLeft=%d\n", bytesRead, bytesLeft);
         bytesRead=read(fdin, (void *)header, bytesLeft);
         bytesLeft -= bytesRead;
} while(bytesLeft > 0);
header[21] = '\0';

// Read RGB data
for(i=0; i<IMG_HEIGHT*IMG_WIDTH; i++)
    {	
        read(fdin, (void *)&R[i], 1);
        read(fdin, (void *)&G[i], 1);
        read(fdin, (void *)&B[i], 1);
    }
close(fdin);




     // Skip first and last row, no neighbors to convolve with
     for(i=0; i<IMG_HEIGHT; i++)
     {

         // Skip first and last column, no neighbors to convolve with
         for(j=0; j<IMG_WIDTH; j++)
         {
          
             temp=(G[((i)*IMG_WIDTH)+j]*0.769)+(R[((i)*IMG_WIDTH)+j]*0.393)+(B[((i)*IMG_WIDTH)+j]*0.189);
            
	     if(temp>255.0) temp=255.0;
	     convR[(i*IMG_WIDTH)+j]=(UINT8)temp;

             temp=(0.686*G[((i)*IMG_WIDTH)+j])+(0.168*B[((i)*IMG_WIDTH)+j])+(0.349*R[((i)*IMG_WIDTH)+j]);
             
	     if(temp>255.0) temp=255.0;
	     convG[(i*IMG_WIDTH)+j]=(UINT8)temp;

             temp= (0.272*R[((i)*IMG_WIDTH)+j])+(0.534*G[((i)*IMG_WIDTH)+j])+(0.131*B[((i)*IMG_WIDTH)+j]);
            
	     if(temp>255.0) temp=255.0;
	     convB[(i*IMG_WIDTH)+j]=(UINT8)temp;
         }
     }
sprintf(fileNameOut,"OUTimage%04d.ppm",thNum+1);
if((fdout = open(fileNameOut, (O_RDWR | O_CREAT), 0666))<0)
{
printf("output file could not be created\n\r");
}

write(fdout, (void *)header, 21);
      //printf("output file : %s fdout : %d\n\r",fileNameOut,fdout);
 
     // Write RGB data
    for(i=0; i<(IMG_HEIGHT*IMG_WIDTH); i++)
     {
         write(fdout, (void *)&convR[i], 1);
        write(fdout, (void *)&convG[i], 1);
         write(fdout, (void *)&convB[i], 1);
     }	
     close(fdout);

}


free(R);
free(G);
free(B);
free(convR);
free(convG);
free(convB);
}



void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
printf("Pthread Policy is SCHED_FIFO\n");
break;
     case SCHED_OTHER:
printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
printf("Pthread Policy is SCHED_OTHER\n");
break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }
}







void *testThread(void *threadid)
{
int i;
    for(i=0;i<NUMBER_THREADS;i++)
{

        pthread_create(&threads[i], &rt_sched_attr, frameThread, (void *)i);
        //printf("Thread Created %d\n",i);
        }

   for(i=0;i<NUMBER_THREADS;i++)
     pthread_join(threads[i], NULL);
     printf("im here1");

   if(pthread_attr_destroy(&rt_sched_attr) != 0)
     perror("attr destroy");
}
















int main(int argc, char *argv[])
{
   int rc, scope;

 

   printf("Before adjustments to scheduling policy:\n");
   print_scheduler();

   // Set POSIX Scheduling Policy
   //
   // Note that FIFO is essentially priority preemptive run to
   // completion on Linux with NPTL since each thread will run
   // uninterrupted at it's given priority level.
   //
   // RR allows threads to run in a Round Robin fashion.
   //
   // We set all threads to be run to completion at high
   // priority so that we can determine whether the hardware
   // provides speed-up by mapping threads onto multiple cores
   // and/or SMT (Symmetric Multi-threading) on each core.
   //
   pthread_attr_init(&rt_sched_attr);
   pthread_attr_init(&main_sched_attr);
   pthread_attr_setinheritsched(&rt_sched_attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&rt_sched_attr, SCHED_FIFO);
   pthread_attr_setinheritsched(&main_sched_attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&main_sched_attr, SCHED_FIFO);

   rt_max_prio = sched_get_priority_max(SCHED_FIFO);
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);

   rc=sched_getparam(getpid(), &nrt_param);
   rt_param.sched_priority = rt_max_prio;
   rc=sched_setscheduler(getpid(), SCHED_FIFO, &rt_param);

   if (rc)
   {
       printf("ERROR; sched_setscheduler rc is %d\n", rc); perror(NULL); exit(-1);
   }

   printf("After adjustments to scheduling policy:\n");
   print_scheduler();

   printf("min prio = %d, max prio = %d\n", rt_min_prio, rt_max_prio);
   pthread_attr_getscope(&rt_sched_attr, &scope);

   // Check the scope of the POSIX scheduling mechanism
   //
   if(scope == PTHREAD_SCOPE_SYSTEM)
     printf("PTHREAD SCOPE SYSTEM\n");
   else if (scope == PTHREAD_SCOPE_PROCESS)
     printf("PTHREAD SCOPE PROCESS\n");
   else
     printf("PTHREAD SCOPE UNKNOWN\n");

   // Note that POSIX priorities are such that the highest priority
   // thread has a large priority number. This is very different
   // than VxWorks for example where low priority numbers mean high
   // scheduling priority. As long as the sched_get_priority_max(<policy>)
   // function call is used, then the number is not important.
   //
   // IMPORTANT: for this test, note that the thread that creates all other
   // threads has higher priority than the workload threads
   // themselves - this prevents it from being preempted so that
   // it can continue to create all threads in order to get them
   // concurrently active.
   //
   rt_param.sched_priority = rt_max_prio-1;
   pthread_attr_setschedparam(&rt_sched_attr, &rt_param);

   main_param.sched_priority = rt_max_prio;
   pthread_attr_setschedparam(&main_sched_attr, &main_param);

   rc = pthread_create(&main_thread, &main_sched_attr, testThread, (void *)0);
   if (rc)
   {
       printf("ERROR; pthread_create() rc is %d\n", rc); perror(NULL); exit(-1);
   }

   pthread_join(main_thread, NULL);

   if(pthread_attr_destroy(&rt_sched_attr) != 0)
     perror("attr destroy");

   rc=sched_setscheduler(getpid(), SCHED_OTHER, &nrt_param);

   printf("TEST COMPLETE\n");
 
}
