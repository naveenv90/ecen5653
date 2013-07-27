#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include "md5.h"

#define SCHED_POLICY SCHED_FIFO
#define THREAD_ITERATIONS 100000
#define ITERATIONS 100000
#define MAX_THREADS 256
#define TRUE 1
#define FALSE 0

typedef unsigned long long int UINT64;

UINT64 startTSC = 0;
UINT64 stopTSC = 0;
UINT64 cycleCnt = 0;

UINT64 readTSC(void)
{
   UINT64 ts;

   __asm__ volatile(".byte 0x0f,0x31" : "=A" (ts));
   return ts;
}

UINT64 cyclesElapsed(UINT64 stopTS, UINT64 startTS)
{
   return (stopTS - startTS);
}


static const char test[512]="#0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ#";
char outfilename[256]="test_frame.ppm";
typedef struct _threadParamsType
{
	int threadID;
	unsigned int microsecs;
	double digestsPerSec;
} threadParamsType;

pthread_t threads[MAX_THREADS];
pthread_t tstThreadHandle[MAX_THREADS];
threadParamsType threadParams[MAX_THREADS];
pthread_attr_t rt_sched_attr;
pthread_attr_t tstThread_sched_attr;
int rt_max_prio, rt_min_prio, min, old_ceiling, new_ceiling;
struct sched_param rt_param;
struct sched_param nrt_param;
struct sched_param tstThread_param;
int threadActiveCnt=0, threadActive=FALSE;
sem_t startIOWorker[MAX_THREADS];
unsigned char testFrameBuffer[3*345600];
unsigned char thread0_buffer[259200];
unsigned char thread1_buffer[259200];
unsigned char thread2_buffer[259200];
unsigned char thread3_buffer[259200];
unsigned char header[22];

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

void setSchedPolicy(void)
{
   int rc, scope;

   // Set up scheduling
   //
   print_scheduler();

   pthread_attr_init(&rt_sched_attr);
   pthread_attr_init(&tstThread_sched_attr);
   pthread_attr_setinheritsched(&rt_sched_attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&rt_sched_attr, SCHED_POLICY);
   pthread_attr_setinheritsched(&tstThread_sched_attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&tstThread_sched_attr, SCHED_POLICY);

   rt_max_prio = sched_get_priority_max(SCHED_POLICY);
   rt_min_prio = sched_get_priority_min(SCHED_POLICY);

   rc=sched_getparam(getpid(), &nrt_param);
   rt_param.sched_priority = rt_max_prio;

#ifdef SET_SCHED_POLICY
   rc=sched_setscheduler(getpid(), SCHED_POLICY, &rt_param);

   if (rc)
   {
       printf("ERROR: sched_setscheduler rc is %d\n", rc);
       perror("sched_setscheduler");
   }
   else
   {
       printf("SCHED_POLICY SET: sched_setscheduler rc is %d\n", rc);
   }

   print_scheduler();
#endif

   printf("min prio = %d, max prio = %d\n", rt_min_prio, rt_max_prio);
   pthread_attr_getscope(&rt_sched_attr, &scope);

   if(scope == PTHREAD_SCOPE_SYSTEM)
     printf("PTHREAD SCOPE SYSTEM\n");
   else if (scope == PTHREAD_SCOPE_PROCESS)
     printf("PTHREAD SCOPE PROCESS\n");
   else
     printf("PTHREAD SCOPE UNKNOWN\n");

   rt_param.sched_priority = rt_max_prio-1;
   pthread_attr_setschedparam(&rt_sched_attr, &rt_param);
   tstThread_param.sched_priority = rt_max_prio;
   pthread_attr_setschedparam(&tstThread_sched_attr, &tstThread_param);
}

void *digestThread(void *threadparam)
{
    int i;
    md5_state_t state;
    md5_byte_t digest[16];
    struct timeval StartTime, StopTime;
    unsigned int microsecs;
    double rate;
    threadParamsType *tp = (threadParamsType*)threadparam;

    //printf("Digest thread %d started, active=%d, waiting for release\n", tp->threadID, threadActiveCnt);
    if(tp->threadID == 0)
    {
      sem_wait (&(startIOWorker[tp->threadID]));


    gettimeofday(&StartTime, 0);
    //for(i=0;i<THREAD_ITERATIONS;i++)
    //{
	// Can I just compute this one time?
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)thread0_buffer, strlen(thread0_buffer));

	md5_finish(&state, digest);

	//if(threadActive == FALSE)
	  //  break;
    //}
    gettimeofday(&StopTime, 0);

    microsecs=((StopTime.tv_sec - StartTime.tv_sec)*1000000);

    if(StopTime.tv_usec > StartTime.tv_usec)
	microsecs+=(StopTime.tv_usec - StartTime.tv_usec);
    else
	microsecs-=(StartTime.tv_usec - StopTime.tv_usec);

    rate=((double)ITERATIONS)/(((double)microsecs)/1000000.0);

    tp->microsecs=microsecs;
    tp->digestsPerSec=rate;

    threadActiveCnt--;
    //threadActiveCnt--;
    printf("Thread %d exit\n", tp->threadID);
    printf("%lf MD5 digests computed per second\n", rate);

	printf("MD5 Digest=");

	for(i=0;i<16;i++)
		printf("0x%2x ", digest[i]);

    pthread_exit(NULL);
     }

    else if(tp->threadID == 1)
    {
      sem_wait (&(startIOWorker[tp->threadID]));


    gettimeofday(&StartTime, 0);
    //for(i=0;i<THREAD_ITERATIONS;i++)
    //{
	// Can I just compute this one time?
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)thread1_buffer, strlen(thread1_buffer));

	md5_finish(&state, digest);

	//if(threadActive == FALSE)
	  //  break;
    //}
    gettimeofday(&StopTime, 0);

    microsecs=((StopTime.tv_sec - StartTime.tv_sec)*1000000);

    if(StopTime.tv_usec > StartTime.tv_usec)
	microsecs+=(StopTime.tv_usec - StartTime.tv_usec);
    else
	microsecs-=(StartTime.tv_usec - StopTime.tv_usec);

    rate=((double)ITERATIONS)/(((double)microsecs)/1000000.0);

    tp->microsecs=microsecs;
    tp->digestsPerSec=rate;

    //threadActiveCnt--;
    threadActiveCnt--;
    printf("Thread %d exit\n", tp->threadID);
    printf("%lf MD5 digests computed per second\n", rate);

	printf("MD5 Digest=");

	for(i=0;i<16;i++)
		printf("0x%2x ", digest[i]);

    pthread_exit(NULL);
     }

      else if(tp->threadID == 2)
    {
      sem_wait (&(startIOWorker[tp->threadID]));


    gettimeofday(&StartTime, 0);
    //for(i=0;i<THREAD_ITERATIONS;i++)
    //{
	// Can I just compute this one time?
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)thread2_buffer, strlen(thread2_buffer));

	md5_finish(&state, digest);

	//if(threadActive == FALSE)
	   // break;
    //}
    gettimeofday(&StopTime, 0);

    microsecs=((StopTime.tv_sec - StartTime.tv_sec)*1000000);

    if(StopTime.tv_usec > StartTime.tv_usec)
	microsecs+=(StopTime.tv_usec - StartTime.tv_usec);
    else
	microsecs-=(StartTime.tv_usec - StopTime.tv_usec);

    rate=((double)ITERATIONS)/(((double)microsecs)/1000000.0);

    tp->microsecs=microsecs;
    tp->digestsPerSec=rate;

    threadActiveCnt--;
    
    printf("Thread %d exit\n", tp->threadID);
    printf("%lf MD5 digests computed per second\n", rate);

	printf("MD5 Digest=");

	for(i=0;i<16;i++)
		printf("0x%2x ", digest[i]);

    pthread_exit(NULL);
     }

    else
    {
      sem_wait (&(startIOWorker[tp->threadID]));


    gettimeofday(&StartTime, 0);
    //for(i=0;i<THREAD_ITERATIONS;i++)
    //{
	// Can I just compute this one time?
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)thread3_buffer, strlen(thread3_buffer));

	md5_finish(&state, digest);

	//if(threadActive == FALSE)
	  //  break;
    //}
    gettimeofday(&StopTime, 0);

    microsecs=((StopTime.tv_sec - StartTime.tv_sec)*1000000);

    if(StopTime.tv_usec > StartTime.tv_usec)
	microsecs+=(StopTime.tv_usec - StartTime.tv_usec);
    else
	microsecs-=(StartTime.tv_usec - StopTime.tv_usec);

    rate=((double)ITERATIONS)/(((double)microsecs)/1000000.0);

    tp->microsecs=microsecs;
    tp->digestsPerSec=rate;
    threadActiveCnt--;
    printf("Thread %d exit\n", tp->threadID);
printf("%lf MD5 digests computed per second\n", rate);

	printf("MD5 Digest=");

	for(i=0;i<16;i++)
		printf("0x%2x ", digest[i]);

    pthread_exit(NULL);
    }
    
     }




   


void thread_shutdown(int signum)
{
    int i;
    
    threadActive=FALSE;

    while(threadActiveCnt)
    {
        threadActive=FALSE;
        printf("Waiting for threads to shut down ... (%d threads running)\n", threadActiveCnt);
        sleep(1);
    }
}


int main(int argc, char *argv[])
{
	int i,j=0,k=0,l=0,m=0,n=0,p=0, numThreads, rc,bytesRead,bytesLeft;
        bytesRead=0;
        bytesLeft=21;
	double clkRate = 0.0;
        double rate=0.0;
	double totalRate=0.0, aveRate=0.0;
	struct timeval StartTime, StopTime;
	unsigned int microsecs;
	md5_state_t state;
	md5_byte_t digest[16];
	int frameIdx=0;
    	unsigned char red, green, blue;
        char ch; 
        FILE *fptr;


        if(argc < 2)
	{
		numThreads=4;
		printf("Will default to 4 synthetic IO workers\n");
                printf("Will default to output file=test_frame.ppm\n");
        }
        else
        {
            sscanf(argv[1], "%d", &numThreads);
       	    printf("Will start %d synthetic IO workers\n", numThreads);
        }

        if((signal(SIGINT, thread_shutdown)) == SIG_ERR)
        {
            perror("SIGINT install error");
        }

   startTSC = readTSC();
   usleep(1000000);
   stopTSC = readTSC();
   cycleCnt = cyclesElapsed(stopTSC, startTSC);
   printf("Cycle Count=%llu\n", cycleCnt);
   clkRate = ((double)cycleCnt)/1000000.0;
   //printf("Based on usleep accuracy, CPU clk rate = %lu clks/sec,",
          
   printf(" %7.1f Mhz\n", clkRate);
     if((fptr=fopen(outfilename, "wb")) == (FILE *)0)
    {
        printf("file open failure\n");
        exit(-1);
    }

    fprintf(fptr, "P6\n");
    fprintf(fptr, "#test\n");
    fprintf(fptr, "720 480\n");
    fprintf(fptr, "255\n");
   
   for(i=0;i<345600;i++)
    {
	red = i / 1356;
	green = 255 - red;
	blue = (red+green) / 2;

	// Fill out in memory frame buffer
	testFrameBuffer[frameIdx]=red; frameIdx++;
	testFrameBuffer[frameIdx]=green; frameIdx++;
	testFrameBuffer[frameIdx]=blue; frameIdx++;

        

        fwrite(&testFrameBuffer[(i*3)], sizeof(unsigned char), 1, fptr);
	fwrite(&testFrameBuffer[(i*3)+1], sizeof(unsigned char), 1, fptr);
	fwrite(&testFrameBuffer[(i*3)+2], sizeof(unsigned char), 1, fptr);

     }
//#define SINGLE_THREAD_TESTS
     //;
    fclose(fptr);
     if((fptr=fopen(outfilename, "r")) == (FILE *)0)
    {
        printf("file open failure\n");
        exit(-1);
    }
    printf("Reading Header\n");
    do
      {
        printf("bytesRead=%d bytesLeft=%d \n",bytesRead,bytesLeft);
        bytesRead=fread((void *)header,1,bytesLeft,fptr);
        if(bytesRead<0)
        printf("\n Error Reading file");
        bytesLeft-=bytesRead;
      }while(bytesLeft > 0);

    header[21]='\0';
    printf("%s\n",header);
    


	printf("\nSINGLE THREAD TESTS\n");

	printf("\nMD5 Digest Test\n");

	//gettimeofday(&StartTime, 0);
     //   startTSC=readTSC();
	//for(i=0;i<ITERATIONS;i++)
	//{
		// Can I just compute this one time?
		/*md5_init(&state);
		md5_append(&state, (const md5_byte_t *)fptr, strlen(fptr));

		md5_finish(&state, digest);
	//}
	stopTSC=readTSC();
        cycleCnt=cyclesElapsed(stopTSC, startTSC);
        microsecs=(cycleCnt/clkRate);
        fclose(fptr); 
     
      //gettimeofday(&StopTime, 0);*/

      /*  microsecs=((StopTime.tv_sec - StartTime.tv_sec)*1000000);

	if(StopTime.tv_usec > StartTime.tv_usec)
		microsecs+=(StopTime.tv_usec - StartTime.tv_usec);
	else
		microsecs-=(StartTime.tv_usec - StopTime.tv_usec);*/

	/*printf("Test Done in %u microsecs for 1 iteration\n",   		microsecs);

	rate=((double)ITERATIONS)/(((double)microsecs)/1000000.0);
	printf("%lf MD5 digests computed per second\n", rate);

	printf("MD5 Digest=");

	for(i=0;i<16;i++)
		printf("0x%2x ", digest[i]);*/



	
        setSchedPolicy();
 for(n=0;n<480;n++)
{
 for(j=0;j<2160;j++)
 {
  if(n<240)
  {
   if(j<1080) //Quadrant II
   {
    thread1_buffer[k]=testFrameBuffer[j+(2160*n)];k++;
   
    
   }
   else     //QuadrantI
   {
     thread0_buffer[p]=testFrameBuffer[j+(2160*n)];p++;
     
   }
  } 
  else
  {
   if(j<1080)  //QuadrantIII
   {
     thread2_buffer[l]=testFrameBuffer[j+(2160*n)];l++;
    }
   else      //QuadrantIV
   {
     thread3_buffer[m]=testFrameBuffer[j+(2160*n)];m++;
     
    } 
  }
 } 

}       
      
     
      printf("k:%d \n",k);
       printf("p:%d \n",p); 
        printf("l:%d \n",l);
       printf("m:%d \n",m);
     for(i=0;i<numThreads;i++)
        {
            if (sem_init (&(startIOWorker[i]), 0, 0))
	    {
		    printf ("Failed to initialize startIOWorker semaphore %d\n", i);
		    exit (-1);
	    }

	    //printf("calling pthread_create\n");

	    threadParams[i].threadID=i;

            rc = pthread_create(&tstThreadHandle[i], &tstThread_sched_attr,
                                  digestThread, &(threadParams[i]));
            if (rc)
            {
                perror("pthread create");
            }
            else
	    {
                threadActiveCnt++;
                threadActive=TRUE;
	        //printf("Thread %d created\n", threadActiveCnt);
	    }

        }

         // Release and detach threads since the will loop or finish and exit
         for(i=0; i<numThreads;i++)
	 {
	     sem_post (&(startIOWorker[i]));	 
             pthread_detach(tstThreadHandle[i]);
	 }

        do
        {
            sleep(1);
        } while(threadActiveCnt > 0);


	printf("\n\n***************** TOTAL PERFORMANCE SUMMARY\n");

        for(i=0;i<numThreads;i++)
	{
            sem_destroy (&(startIOWorker[i]));
            totalRate+=threadParams[i].digestsPerSec;
            printf("Thread %d done in %u microsecs for 1 iterations\n", i, threadParams[i].microsecs);
            printf("%lf MD5 digests computed per second\n", threadParams[i].digestsPerSec);
	}

	printf("\nFor %d threads, Total rate=%lf\n", numThreads, totalRate);

}