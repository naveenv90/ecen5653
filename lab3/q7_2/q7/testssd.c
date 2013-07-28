#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define SSD_IOCTL_MAGIC  's'

#define SSD_IOCTL_ERASE    _IO(SSD_IOCTL_MAGIC, 0)

#define SSD_IOCTL_RECOVER _IO(SSD_IOCTL_MAGIC, 1)

#define TEST_RAID_STRING "#0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ#"

static char testLBA1[512]= TEST_RAID_STRING;
static char testLBA2[512]= TEST_RAID_STRING;
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



int main(void)
{
    int ssd1, ssd2, ssd3, ssd4, ssd5, ssd6, ssd7,ssdr;

    if((ssdr=open("/dev/ssdr", O_RDWR)) < 0)
    {
        printf("ERROR opening SSDr\n");
        exit(-1);
    }
    else
    {
        printf("ssd=%d opened\n", ssdr);
    }

   
    unsigned int i=0;
  
    
    printf("\n");
    printf("\n RAID 6 IMPLEMENTATION");
    startTSC=readTSC();
    printf("WRITE TO SSD:\n");
   // lseek(ssd1, 0, SEEK_SET);
    write(ssdr, &testLBA2[0], 512);
    stopTSC=readTSC();
    cycleCnt=cyclesElapsed(stopTSC,startTSC);
    printf("\n Time taken to write 512 bytes %llu",cycleCnt);
    

  // Read and print out first N bytes
    printf("READ FROM SSD:\n");
     startTSC=readTSC();
    read(ssdr, &testLBA1[0], 512);
    stopTSC=readTSC();
    cycleCnt=cyclesElapsed(stopTSC,startTSC);
    printf("\n Time taken to read 512 bytes %llu",cycleCnt);
    //printf("test = %2X %2X %2X %2X\n", testLBA1[508], testLBA1[509], testLBA1[510], testLBA1[511]);
    for (i=0;i<512;i++)
    printf("%c",testLBA1[i]);
     




    close(ssdr);
}
