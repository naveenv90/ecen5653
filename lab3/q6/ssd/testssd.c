#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define TEST_RAID_STRING "#0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ##0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ#"

static char testLBA1[512]= TEST_RAID_STRING;
static char testLBA2[512]= TEST_RAID_STRING;




int main(void)
{
    int ssd1, ssd2, ssd3, ssd4, ssd5, ssd6, ssd7;

    if((ssd1=open("/dev/ssd2", O_RDWR)) < 0)
    {
        printf("ERROR opening SSD1\n");
        exit(-1);
    }
    else
    {
        printf("ssd=%d opened\n", ssd1);
    }

   
    unsigned int i=0;
    // Read and print out first N bytes
    printf("READ FROM SSD:\n");
    read(ssd1, &testLBA1[0], 512);
    //printf("test = %2X %2X %2X %2X\n", testLBA1[508], testLBA1[509], testLBA1[510], testLBA1[511]);
    for (i=0;i<512;i++)
    printf("%c",testLBA1[i]);
     
    printf("\n");
    printf("WRITE TO SSD:\n");
    lseek(ssd1, 0, SEEK_SET);
    write(ssd1, &testLBA2[0], 512);

 
   printf("READ FROM SSD:\n");
    printf("READ FROM SSD:\n");
    lseek(ssd1,0,SEEK_SET);
    read(ssd1, &testLBA1[0], 512);
    //printf("test = %2X %2X %2X %2X\n", testLBA1[508], testLBA1[509], testLBA1[510], testLBA1[511]);
     
    for (i=0;i<512;i++)
    printf("%c",testLBA1[i]);
     
    printf("\n");
    lseek(ssd1,0,SEEK_SET);
    ioctl(ssd1,29440);
    printf("\n DATA ERASED FROM SSD");
    lseek(ssd1,0,SEEK_SET);
     printf("READ FROM SSD AFTER ERASURE:\n");
    read(ssd1, &testLBA1[0], 512);

    for (i=0;i<512;i++)
    printf("%2X",testLBA1[i]);
     
    printf("\n");
    //printf("test = %2X %2X %2X %2X\n", testLBA1[1], testLBA1[2], testLBA1[3], testLBA1[4]);

    close(ssd1);
}
