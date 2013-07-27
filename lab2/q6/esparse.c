#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include <syslog.h>

#include "ts.h"
#include "parse.h"

#define OFF 0
#define PRINT_HEX 1

int parseES(playbackContext *pb, uint32_t numPkts, uint16_t searchPID, int printHex);

playbackContext pb;

int main (int argc, char *argv[])
{
   int numMPEG;

   if(argc < 3)
   {
       printf("Usage: %s <transport stream file> <PID>\n", argv[0]);
       SYSLOG_ASSERT(0, strerror(errno));
   }

   if((pb.fp = fopen(argv[1],"r+b")) == NULL)
   {
       perror("failure on MPEG-2 TS file open");
       SYSLOG_ASSERT(0, strerror(errno));
   }

    sscanf(argv[2], "%hu", &pb.videoPID[0]);
    printf("Will search for PID=%d\n", pb.videoPID[0]);

    numMPEG=parseES(&pb, 0xFFFFFFFF, pb.videoPID[0], PRINT_HEX);

    printf("\nnum PES=%d\n", pb.numPES);

    fclose(pb.fp);

    return OK;
}

int parseES(playbackContext *pb, uint32_t numPkts, uint16_t searchPID, int printHex)
{
    // variables used to get bit rate from content file
    uint32_t mpegPackets = 0;
    unsigned char mpegPkt[256];
    unsigned int PID, CC, FLAGS, i;
    unsigned char ScrambleCtl, AdaptFieldCtl;
    unsigned char AdaptLength, AdaptFlags;
    int discIndicator, randAccIndicator, pesPrio;
    unsigned long long byteCnt;
    long curOffset;

    int sync = 0;

    curOffset=ftell(pb->fp);

    if(curOffset > 0)
    {
        printf("curOffset=%ld, will rewind\n", curOffset);
        if(fseek(pb->fp, 0, SEEK_SET) < 0)
        {
            //perror("bitstream fseek");
            printf("\nbitstream fseek failed\n");
            SYSLOG_ASSERT(0, strerror(errno));
        }
    }

    //printf("\nScanning content file for GOP offsets\n");
    do
    {
        fread(mpegPkt, sizeof(uint8_t), MPEG2_PACKET_SIZE, pb->fp);
        mpegPackets++;

        byteCnt = mpegPackets * MPEG2_PACKET_SIZE;

        if ((sync % (MPEG2_PACKET_SIZE*100) == 0) && !printHex)
        {
            printf(".");
            fflush(stdout);
        }

        sync++;

        // look for the sync byte
        if(mpegPkt[0] != TS_SYNC_BYTE)
        {
            printf("vpSendspts: data corruption in content file\n");
            syslog(LOG_ERR, "TSLIB - vpSendspts: data corruption in content file");
            return ERROR;
        }
        else
        {

            FLAGS=mpegPkt[1] & 0xE0;
            PID=((mpegPkt[1] & 0x1F)<<8) | (mpegPkt[2]);

            ScrambleCtl=((mpegPkt[3] & 0xC0)>>6);
            AdaptFieldCtl=((mpegPkt[3] & 0x30)>>4);
            CC=mpegPkt[3] & 0x0F;
            AdaptLength=(mpegPkt[4]);
            AdaptFlags=(mpegPkt[5]);
            discIndicator=((AdaptFlags & 0x80) > 0);
            randAccIndicator=((AdaptFlags & 0x40) > 0);
            pesPrio=(AdaptFlags & 0x20);
        }

        if(printHex && 0)
        {
            printf("\nMPEG Pkt=%lu, PID=%04X(%04u), CC=%02X, AdaptFieldCtl=%01X, AdaptLength=%d, AdaptFlags=%02X, discIndicator=%d:\n",
                   mpegPackets, PID, PID, CC, AdaptFieldCtl, AdaptLength, AdaptFlags, discIndicator);
        }

        if(PID == searchPID)
        {
            pb->matchCnt++;

            // Detect PES header for MPEG-2 video
            // Will be: 0x00 0x00 0x01 StremID
            for(i=0; i<=180; i++)
            {
                if((mpegPkt[i] == 0x00) &&
                   (mpegPkt[i+1] == 0x00) &&
                   (mpegPkt[i+2] == 0x01))
                {

                    if(mpegPkt[i+3] == PICTURE_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES PICTURE Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if((mpegPkt[i+3] >= SLICE_START_CODE_LOWER_BOUND) && (mpegPkt[i+3] <= SLICE_START_CODE_UPPER_BOUND))
                    {
                        if(printHex)
                        {
                            printf("PES SLICE Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }

                    else if(mpegPkt[i+3] == USER_DATA_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES USER DATA Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }

                    else if(mpegPkt[i+3] == SEQUENCE_HEADER_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES SEQ Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == SEQUENCE_ERROR_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES SEQ ERR Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == EXTENSION_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES EXT Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == SEQUENCE_END_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES SEQ END Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == GOP_START_CODE)
                    {
                        if(printHex)
                        {
                            printf("PES GOP Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == PROGRAM_END_STREAM_ID)
                    {
                        if(printHex)
                        {
                            printf("PES PROG END Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == PACK_HEADER_STREAM_ID)
                    {
                        if(printHex)
                        {
                            printf("PES PACK Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == SYSTEM_HEADER_STREAM_ID)
                    {
                        if(printHex)
                        {
                            printf("PES SYS Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == PROGRAM_STREAM_MAP)
                    {
                        if(printHex)
                        {
                            printf("PES PROG STREAM Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if((mpegPkt[i+3] >= AUDIO_STREAM_LOWER_BOUND) && (mpegPkt[i+3] <= AUDIO_STREAM_UPPER_BOUND))
                    {
                        if(printHex)
                        {
                            printf("PES AUDIO Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    // 0xE0-0xEF and 16-bit packet length
                    else if((mpegPkt[i+3] >= VIDEO_STREAM_LOWER_BOUND) && (mpegPkt[i+3] <= VIDEO_STREAM_UPPER_BOUND))
                    {
                        pb->numPES++;
                        if(printHex)
                        {
                            printf("******** PES VIDEO Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
    
                            //for(j=0;j<(uint16_t)(*(&mpegPkt[i+4]));i++)
                            //    fwrite(&mpegPkt[i+j+6], (sizeof(unsigned char)), 1, fpES);
                        }
                    }
                    else if(mpegPkt[i+3] == ECM_STREAM)
                    {
                        if(printHex)
                        {
                            printf("PES ECM Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == EMM_STREAM)
                    {
                        if(printHex)
                        {
                            printf("PES EMM Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
                    else if(mpegPkt[i+3] == PROGRAM_STREAM_DIRECTORY)
                    {
                        if(printHex)
                        {
                            printf("PES PROG STREAM DIR Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
    
                    else
                    {
                        if(printHex)
                        {
                            printf("PES RESERVED Hdr: ID=0x%02X, length=%d, byteCnt=%llu\n",
                                   mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])), byteCnt+i);
                        }
                    }
     
                }

            }

            if(printHex && 0)
            {
                //dumpMPEG2Packet(mpegPkt);
            }

        }

    } while (!feof(pb->fp) && (pb->matchCnt < numPkts)); // while read

    //rewind file
    rewind(pb->fp);
    fflush(pb->fp);

    return pb->numPES;
}
