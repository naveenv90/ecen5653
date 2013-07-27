#include <stdio.h>
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

double getBitRate(FILE *fp);

int main (int argc, char *argv[])
{
   double estimatedBitRate;
   FILE *fp;

   if(argc < 2)
   {
       printf("Usage: %s <transport stream file>\n", argv[0]);
       exit(-1);
   }

   if((fp = fopen(argv[1],"r+b")) == NULL)
   {
       perror("failure on MPEG-2 TS file open");
       exit(-1);
   }
   else
   {
       printf("opened %s\n", argv[1]);
   }

    estimatedBitRate=getBitRate(fp);
    //printf("estimatedBitRate=%lf\n", estimatedBitRate);

    fclose(fp);

    return OK;
}

/**
 * getBitRate determines bit rate for an MPEG-2 transport
 * stream.
 */
double getBitRate(FILE *fp)
{
    // variables used to get bit rate from content file
    uint64_t PCR0 = 0, PCR1;
    uint64_t PCRdelta;
    double bitRate = 0.0, estBitRate=0.0, lastBitRate=0.0;
    double maxBitRate=0.0, minBitRate=100000000.0;
    uint32_t mpegPackets = 0, totalPackets=0;
    uint32_t gotPCR = 0;
    unsigned int PID, bytesRead;
    long curOffset;
    int noSync=TRUE;
    uint8_t packetBuffer[MPEG2_PACKET_SIZE];

    curOffset=ftell(fp);

    if(curOffset > 0)
    {
        printf("curOffset=%ld, will rewind\n", curOffset);
        if(fseek(fp, 0, SEEK_SET) < 0)
        {
            perror("bitstream fseek");
            printf("\nbitstream fseek failed\n");
            exit(-1);
        }
    }

    //used to control "." output
    bytesRead=0;

    while (!feof(fp))
    {
        bytesRead+=fread(&packetBuffer[0], sizeof(uint8_t), 1, fp);

        if((packetBuffer[0] == TS_SYNC_BYTE) && (bytesRead > 0))
        {
            printf("Found sync on byte=%d\n", bytesRead);
            noSync=FALSE;
            bytesRead+=fread(&packetBuffer[1], sizeof(uint8_t), (MPEG2_PACKET_SIZE-1), fp);
	    mpegPackets++; totalPackets++;
        
	    break;
        }
    }


    //printf("\nScanning content file for bit rate\n");
    while (!feof(fp))
    {

        // PCR rates are typically 10-25 Hz, so 100 milliseconds to 40 millisec.
        // 
        // This code will can through the first 10 PCRs in this content, about
	// 0.4 to 1 second worth of content to get the bit-rate.  This is not
	// ideal for VBR bitstreams, but of course works great for CBR.
        // 

        // First see if the TS header indicates the presence of an
        // adaptation field.
        // 
        if(GET_MPEG2TS_ADAPT(packetBuffer) & 0x02)
        {
            // Get the adaptation field and look for the presence of
            // a PCR in the adaptation field
            if(GET_MPEG2TS_ADAPTLENGTH(packetBuffer) == 0)
            {
                printf("Zero length AF\n");
                continue;
            }

            if(GET_MPEG2TS_ADAPTFIELD(packetBuffer) & 0x10)
            {
                // Get the initial PCR and reset the packet counter
                if(gotPCR == 1)
                {
                    PID = GET_MPEG2TS_PID(packetBuffer);
                    GET_MPEG2TS_PCR(packetBuffer, PCR0);
                    mpegPackets = 0;
                }

                // Look for the next packet containing a PCR to compute bitrate
                if((gotPCR == 2) && PID == GET_MPEG2TS_PID(packetBuffer))
                {
                    GET_MPEG2TS_PCR(packetBuffer, PCR1);                                                              
                    PCRdelta = (uint64_t) ((uint64_t)PCR1 - (uint64_t)PCR0);
                    printf("PCR0=%lld, PCR1=%lld, packet=%lu\n", PCR0, PCR1, totalPackets); 

                    // bitRate is derived from the transport_rate(i) equation in
		    // 13818-1 page 11
		    //
                    // 40608000000 = (188 bytes/packet) x (27000000 Hz) x (8 bits/byte)
                    // 
                    bitRate=(double)(((double)(40608000000ULL)*((double)mpegPackets))/(double)(PCRdelta));

		    if(bitRate > maxBitRate)
			    maxBitRate=bitRate;

		    if(bitRate < minBitRate)
			    minBitRate=bitRate;

                    //printf("\nFound bitRate=%llf\n", bitRate);

		    if(lastBitRate > 0.0)
			    estBitRate=(bitRate+lastBitRate)/2;

		    lastBitRate=bitRate;
		    gotPCR=0;
                    //break;
                }
                gotPCR++;

            }  // GET_MPEG2TS_ADAPTFIELD

        } //GET_MPEG2TS_ADAPT

	// Read next MPEG2 TS packet
        bytesRead+=fread(&packetBuffer[0], sizeof(uint8_t), MPEG2_PACKET_SIZE, fp);
	mpegPackets++; totalPackets++;

    } // while read

    printf("\nFound maxBitRate=%9.1lf bps, minBitRate=%9.1lf bps\n", maxBitRate, minBitRate);
    printf("Estimated estBitRate=%3.6lf Mbps\n", (estBitRate/1000000.0));

    //rewind file
    rewind(fp);
    fflush(fp);

    return(bitRate);
}
