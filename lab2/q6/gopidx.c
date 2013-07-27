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
#define PRINT_HEADER 1
#define PRINT_HEX 1
#define PRINT_GOPIDX 1
#define PRINT_PICIDX 1

int parsePAT(FILE *fp, playbackContext *pbc, streamContext *strc);
int getTrickIdx(playbackContext *pb, FILE *fp, FILE *fpTrick, iFramePointer *iframeList, uint32_t numPkts, uint16_t searchPID, int printHeader, int printHex, int printGOPIdx, int printPICIdx);

playbackContext pb;

int main (int argc, char *argv[])
{
   int i, numPIDs;
   streamContext strc;
   char trickName[128];
   unsigned long maxIframe=0;
   unsigned long maxIframeByteOffset=0;

   if(argc < 2)
   {
       printf("Usage: %s <transport stream file> [PID]\n", argv[0]);
       SYSLOG_ASSERT(0, strerror(errno));
   }

   if((pb.fp = fopen(argv[1],"r+b")) == NULL)
   {
       perror("failure on MPEG-2 TS file open");
       SYSLOG_ASSERT(0, strerror(errno));
   }

   strcpy(trickName, "trick_");
   strcat(trickName, argv[1]);
   if((pb.fpTrick = fopen(trickName,"w")) == NULL)
   {
       perror("failure on MPEG-2 TS trick file open");
       SYSLOG_ASSERT(0, strerror(errno));
   }

    if(argc >= 3)
    {
        sscanf(argv[2], "%hu", &pb.videoPID[0]);
        printf("Will search for PID=%d\n", pb.videoPID[0]);
    }
    else
    {
        numPIDs=parsePAT(pb.fp, &pb, &strc);
    }

    printf("After PID search:\n");
    printf("Number of PAT =%hu (0x%x) =%u\n", pb.patPID, pb.patPID, pb.numPAT);
    printf("Number of PMT =%hu (0x%x) =%u\n", pb.pmtPID[0], pb.pmtPID[0], pb.numPMT);
    printf("Number of VID =%hu (0x%x) =%u\n", pb.videoPID[0], pb.videoPID[0], pb.numVideo);
    printf("Number of AUD =%hu (0x%x) =%u\n", pb.audioPID[0], pb.audioPID[0], pb.numAudio);
    printf("Number of NULL=%hu (0x%x) =%u\n", pb.nullPID, pb.nullPID, pb.numNull);
    printf("Number of UNK =%hu (0x%x) =%u\n", pb.unknownPID, pb.unknownPID, pb.numUnknown);

    pb.numIframe=getTrickIdx(&pb, pb.fp, pb.fpTrick, pb.iframePtrs, 0xFFFFFFFF, pb.videoPID[0], 0, 0, 0, 0);
    printf("VOD stream=%s, built trick file with %d pointers for videoPID=%d\n", pb.streamName, pb.numIframe, pb.videoPID[0]);

    //pb.numIframe=getTrickIdx(&pb, pb.fp, pb.fpTrick, pb.iframePtrs, 0xFFFFFFFF, pb.videoPID[0], PRINT_HEADER, 0, 0, 0);

    //pb.numIframe=getTrickIdx(&pb, pb.fp, pb.fpTrick, pb.iframePtrs, 0xFFFFFFFF, pb.videoPID[0], PRINT_HEADER, PRINT_HEX, PRINT_GOPIDX, PRINT_PICIDX);
    //pb.numIframe=getTrickIdx(&pb, pb.fp, pb.fpTrick, pb.iframePtrs, 0xFFFFFFFF, pb.videoPID[0], PRINT_HEADER, PRINT_HEX, 0, 0);

    printf("\nTrick File Summary:\n");
    for(i=0;i<pb.numIframe;i++)
    {
        if(pb.iframePtrs[i].iframeLength > maxIframe)
        {
            maxIframe=pb.iframePtrs[i].iframeLength;
            maxIframeByteOffset=pb.iframePtrs[i].iframeOffset;
        }

        printf("%d) I-frame ptr at %lld byte offset, iframeLength=%d, iframePkts=%.1lf, gopLength=%d, gopPkts=%.1lf\n",
               i, pb.iframePtrs[i].iframeOffset, pb.iframePtrs[i].iframeLength,
               (double)(((double)pb.iframePtrs[i].iframeLength)/(double)MPEG2_PACKET_SIZE),
               pb.iframePtrs[i].gopLength,
               (double)(((double)pb.iframePtrs[i].gopLength)/(double)MPEG2_PACKET_SIZE)
              );
    }

    printf("\n%d I-frames, %d hdrs with PID=%d\n",
           pb.numIframe, pb.matchCnt, pb.videoPID[0]);
    printf("Maximum I-frame=%ld @ %ld\n",  maxIframe, maxIframeByteOffset);

    printf("\n*****Summary*****\n");
    printf("Number of TS bytes =%u\n", pb.numBytes);
    printf("Number of TS packets =%u\n", pb.numTSPackets);
    printf("Number of PAT =%hu (0x%x) =%u\n", pb.patPID, pb.patPID, pb.numPAT);
    printf("Number of PMT =%hu (0x%x) =%u\n", pb.pmtPID[0], pb.pmtPID[0], pb.numPMT);
    printf("Number of VID =%hu (0x%x) =%u\n", pb.videoPID[0], pb.videoPID[0], pb.numVideo);
    printf("Number of AUD =%hu (0x%x) =%u\n", pb.audioPID[0], pb.audioPID[0], pb.numAudio);
    printf("Number of NULL=%hu (0x%x) =%u\n", pb.nullPID, pb.nullPID, pb.numNull);
    printf("Number of UNK =%hu (0x%x) =%u\n", pb.unknownPID, pb.unknownPID, pb.numUnknown);

    printf("\n****Video PID****\n");
    printf("Number of I-FR=%u\n", pb.numIframe);
    printf("Number of GOP=%u\n", pb.numGOP);
    printf("Number of PIC=%u\n", pb.numPIC);
    printf("Number of PES=%u\n", pb.numPES);
    printf("Number of SEQ=%u\n", pb.numSeq);

#define VERIFY_TRICK_FILE

#ifdef VERIFY_TRICK_FILE
    fclose(pb.fpTrick);

    if((pb.fpTrick = fopen(trickName,"r")) == NULL)
    {
        perror("failure on MPEG-2 TS trick file open");
        SYSLOG_ASSERT(0, strerror(errno));
    }

    printf("\n\nRE-READING TRICK FILE HERE\n");
    fread(&pb.numIframe, sizeof(pb.numIframe), 1, pb.fpTrick);

    printf("FOUND %d I-frames\n", pb.numIframe);

    for(i=0;i<pb.numIframe;i++)
    {
        fread(&pb.iframePtrs[i], sizeof(iFramePointer), 1, pb.fpTrick);
    }


    printf("\nTrick File Verify:\n");
    for(i=0;i<pb.numIframe;i++)
    {
        if(pb.iframePtrs[i].iframeLength > maxIframe)
        {
            maxIframe=pb.iframePtrs[i].iframeLength;
            maxIframeByteOffset=pb.iframePtrs[i].iframeOffset;
        }

        printf("%d) I-frame ptr at %lld byte offset, iframeLength=%d, gopLength=%lf, gopPkts=%.1lf\n",
               i, pb.iframePtrs[i].iframeOffset, pb.iframePtrs[i].iframeLength,
               (double)(((double)pb.iframePtrs[i].iframeLength)/(double)MPEG2_PACKET_SIZE),
               (double)(((double)pb.iframePtrs[i].gopLength)/(double)MPEG2_PACKET_SIZE)
              );
    }

    printf("\n%d I-frames, %d hdrs with PID=%d\n",
           pb.numIframe, pb.matchCnt, pb.videoPID[0]);
    printf("Maximum I-frame=%ld @ %ld\n",  maxIframe, maxIframeByteOffset);
#endif

    fclose(pb.fp);
    fclose(pb.fpTrick);

    return OK;
}


int parsePAT(FILE *fp, playbackContext *pbc, streamContext *strc)
{
    int i, j, numUniquePIDs=0, numMappedPIDs=0, mpegPackets=0, byteCnt, bytesRead;
    int uniquePID=TRUE, pidsFound=FALSE, pmtsFound=0;
    unsigned int PID, CC, FLAGS;
    int payloadStart=0, pointer_field;
    unsigned short uniquePIDList[256];
    unsigned char ScrambleCtl, AdaptFieldCtl;
    unsigned char AdaptLength, AdaptFlags;
    int discIndicator, randAccIndicator, pesPrio;
    unsigned char packetBuffer[256];
    int sync = 0, patFound=FALSE;
    unsigned short sectionLen, programLen, esLen, streamType, elemPID;
    patHeader patSect;
    pmtHeader pmtSect;
    long curOffset;

    // By definition
    pbc->nullPID=NULL_PID;
    pbc->patPID=PAT_PID;

    for(i=0;i<MAX_PROGRAMS;i++)
    {
        pbc->pmtPID[i]=UNKNOWN_PID;
        pbc->videoPID[i]=UNKNOWN_PID;
        pbc->audioPID[i]=UNKNOWN_PID;
    }

    for(i=0;i<256;i++)
        uniquePIDList[i]=0xFFFF;

    curOffset=ftell(fp);

    if(curOffset > 0)
    {
        printf("curOffset=%ld, will rewind\n", curOffset);
        if(fseek(fp, 0, SEEK_SET) < 0)
        {
            //perror("bitstream fseek");
            printf("\nbitstream fseek failed\n");
            SYSLOG_ASSERT(0, strerror(errno));
        }
    }

    do  // Scan file for PSI PIDs and first program (or only) video and audio PIDs
    {

        bytesRead=0;
        do
        {
            bytesRead+=fread(&packetBuffer[bytesRead], sizeof(uint8_t), MPEG2_PACKET_SIZE, fp);
        } while((bytesRead < MPEG2_PACKET_SIZE) && !feof(fp));

        mpegPackets++;

        byteCnt = mpegPackets * MPEG2_PACKET_SIZE;

        if ((sync % (MPEG2_PACKET_SIZE*100)) == 0)
        {
            printf(".");
            fflush(stdout);
        }

        sync++;

        if(packetBuffer[0] != TS_SYNC_BYTE)
        {
            return ERROR;
        }
        else // Parse PID and adaptation fields
        {
            FLAGS=packetBuffer[1] & 0xE0;
            PID=((packetBuffer[1] & 0x1F)<<8) | (packetBuffer[2]);

            if(FLAGS & 0x40) payloadStart=TRUE;

            ScrambleCtl=((packetBuffer[3] & 0xC0)>>6);
            AdaptFieldCtl=((packetBuffer[3] & 0x30)>>4);
            CC=packetBuffer[3] & 0x0F;
            AdaptLength=(packetBuffer[4]);
            AdaptFlags=(packetBuffer[5]);
            discIndicator=((AdaptFlags & 0x80) > 0);
            randAccIndicator=((AdaptFlags & 0x40) > 0);
            pesPrio=(AdaptFlags & 0x20);

            uniquePID=TRUE;
            for(i=0;i<numUniquePIDs;i++)
            {
                if(PID == uniquePIDList[i])
                {
                    uniquePID=FALSE;
                    break;
                }
            }

            if(uniquePID)
            {
                uniquePIDList[numUniquePIDs]=PID;
                numUniquePIDs++;
                printf("\nPID SEARCH: parsed UNIQUE PID=%d (0x%x), total=%d\n", PID, PID, numUniquePIDs);
                //dumpMPEG2Packet(packetBuffer);
            }


            // Loop through PIDs until a PAT is seen
            if((PID == PAT_PID) && (!patFound))
            {
                printf("PAT found\n");
                patFound=TRUE;

                //dumpMPEG2Packet(packetBuffer);

                if(payloadStart)
                {
                    printf("PAT payload start flag set\n");
                }
                printf("Adaptation field ctl=0x%X\n", AdaptFieldCtl);
                               
                if(AdaptFieldCtl == 1)
                {     
                    // See section 2.4.3.3 of 13818-1, find payload_unit_start_indicator for PSI data

                    pointer_field=packetBuffer[4];

                    printf("PAT pointer_field=0x%x\n", pointer_field);

                    // Use structure assignment to bitfield defined header
                    patSect = *((patHeader *)&packetBuffer[pointer_field+5]);


                    // Bitwise operator approach should be replaced with above structure
                    //
                    printf("PAT table_id=0x%x\n", packetBuffer[pointer_field+5]);
                    printf("section syntax-0-res =0x%x\n", (0xF0 & packetBuffer[pointer_field+6]));

                    sectionLen=0;
                    sectionLen|=((packetBuffer[pointer_field+6]<<8) & 0x0F00) | packetBuffer[pointer_field+7];
                    sectionLen=sectionLen & 0x0FFF;

                    // transport stream ID = 8,9
                    pbc->tripletInfo.transport_stream_id=*((unsigned short *)&packetBuffer[pointer_field+8]);

                    // reserved, version_number, current_next_indicator = 10

                    printf("PAT section_length=%d (0x%x)\n", sectionLen, sectionLen);
                    printf("PAT section_number=%d\n", packetBuffer[pointer_field+11]);
                    printf("PAT last_section_number=%d\n", packetBuffer[pointer_field+12]);

                    for(i=0;i<(sectionLen-9);i+=4) // minus 5 for 8,9,10,11,12 and 4 for CRC
                    {
                        pbc->program_number[pbc->numPrograms]=(packetBuffer[pointer_field+i+13]<<8) | (packetBuffer[pointer_field+i+14]);
                        printf("PAT program_number=%d\n", pbc->program_number[pbc->numPrograms]);

                        if(pbc->program_number == 0)
                        {
                            printf("!!!!!!!!!!!!network_PID being ignored!!!!!!!!!!!!\n");
                        }
                        else
                        {
                            pbc->pmtPID[pbc->numPrograms] = (((packetBuffer[pointer_field+i+15]) & 0x1F)<<8) | (packetBuffer[pointer_field+i+16]);
                            printf("PAT PMT PID=0x%03x for program %d\n", pbc->pmtPID[pbc->numPrograms], pbc->numPrograms);
                            pbc->numPrograms++;
                        }
                    }

                    strc->numPrograms=pbc->numPrograms;
                    printf("PAT CRC=0x%lx\n", *((uint32_t *)&packetBuffer[pointer_field+i+13]));
                    numMappedPIDs++;

                }
                else
                {
                    printf("Adaptation field lenght=%d\n", AdaptLength);
                }

            }

            else if(!patFound)
            {
                // keep looking for PAT and don't look for any other PIDs
                continue;
            }

            // Now that we know the PMT-PID, get the rest of the PIDs from it
            else
            {
                for(i=0;i<pbc->numPrograms;i++)
                {
                    if((PID == pbc->pmtPID[i]))
                    {
    
                        printf("\nPMT found for program %d\n", pbc->program_number[i]);
                        strc->program[i].pmtPID=PID;
                        pmtsFound++;
        
                        //dumpMPEG2Packet(packetBuffer);
        
                        if(payloadStart)
                        {
                            printf("PMT payload start flag set\n");
                        }
                        printf("Adaptation field ctl=0x%X\n", AdaptFieldCtl);
        
                        if(AdaptFieldCtl == 1)
                        {     
                            // See section 2.4.3.3 of 13818-1, find payload_unit_start_indicator for PSI data
    
                            pointer_field=packetBuffer[4];
        
                            printf("PMT pointer_field=0x%x\n", pointer_field);
        
                            printf("PMT table_id=0x%x\n", packetBuffer[pointer_field+5]);
        
                            // Use structure assignment to bitfield defined header
                            pmtSect = *((pmtHeader *)&packetBuffer[pointer_field+5]);
        
            
                            // Bitwise operator approach should be replaced with above structure
                            //
                            printf("section syntax-0-res =0x%x\n", (0xF0 & packetBuffer[pointer_field+6]));
        
                            sectionLen=0;
                            sectionLen|=((packetBuffer[pointer_field+6]<<8) & 0x0F00) | packetBuffer[pointer_field+7];
                            sectionLen=sectionLen & 0x0FFF;
        
                            printf("PMT section_length=%d (0x%x)\n", sectionLen, sectionLen);
                            strc->program[i].programNum=(packetBuffer[pointer_field+8]<<8) | (packetBuffer[pointer_field+9]);
                            printf("PMT program_number=%d\n", strc->program[i].programNum);
        
                            // reserved, version_number, current_next_indicator = 10
        
                            printf("PMT section_number=%d\n", packetBuffer[pointer_field+11]);
                            printf("PMT last_section_number=%d\n", packetBuffer[pointer_field+12]);
        
                            pbc->pcrPID[i] = (((packetBuffer[pointer_field+13]) & 0x1F)<<8) | (packetBuffer[pointer_field+14]);
                            strc->program[i].pcrPID=pbc->pcrPID[i];
        
                            programLen=0;
                            programLen|=((packetBuffer[pointer_field+15]<<8) & 0x0F00) | packetBuffer[pointer_field+16];
                            programLen=programLen & 0x0FFF;
        
                            printf("PMT program_length=%d (0x%x)\n", programLen, programLen);
                            //
                            //
                            strc->program[i].numPIDS=0;
        
                            for(j=0;j<(sectionLen-4-programLen-9);) // CRC=4, preceeding=9, programLen if any
                            {
                                streamType=(packetBuffer[programLen+pointer_field+17+j]);
                                elemPID=(((packetBuffer[programLen+pointer_field+18+j]<<8) & 0x1F00) | packetBuffer[programLen+pointer_field+19+j]);
                                strc->program[i].pid[strc->program[i].numPIDS].PID=elemPID;
                                strc->program[i].pid[strc->program[i].numPIDS].iso_stream_type=streamType;
                                numUniquePIDs++;
                                printf("PMT stream_type=%d (0x%02X)\n", streamType, streamType);
                                printf("PMT elementary_PID=%d (0x%02X)\n", elemPID, elemPID);
                                esLen=(((packetBuffer[programLen+pointer_field+20+j]<<8) & 0x0F00) | packetBuffer[programLen+pointer_field+21+j]);
                                printf("PMT ES_info_length=%d\n", esLen);
                                j=j+5+esLen;
   
                                if(streamType == 0x02) // Video stream
                                {
                                    printf("======== Setting videoPID\n");
                                    pbc->videoPID[i]=elemPID;
                                }
                                //else if((streamType == 0x03) ||   // 11172 Audio
                                //        (streamType == 0x04) ||   // 13818-3 Audio
                                //        (streamType == 0x11) ||   // 14496-3 Audio
                                //        (streamType >= 0x15 && streamType <= 0x7F) ||   // AC-3 Audio in 13818-1 Reserved section
                                //        (streamType == 0x05))     // AC-3 Audio in private section
                                else
                                {
                                    printf("======== Setting audioPID\n");
                                    pbc->audioPID[i]=elemPID;
                                }
        
                                strc->program[i].numPIDS++;
                            }
    
                            printf("PMT CRC=0x%lx\n", *((uint32_t *)&packetBuffer[programLen+pointer_field+i+17]));
                            numMappedPIDs++;
        
                        }
                        else
                        {
                            printf("Adaptation field lenght=%d\n", AdaptLength);
                        }

                    } // end if PID match found

                } // end for in number of programs

            } // end else if looking for PMT PIDs

        } // end good packet with sync found

        if(patFound && (pbc->numPrograms == pmtsFound))
            pidsFound=TRUE;

    } while (!feof(fp) && !pidsFound); // end while not all PIDs found and not end of file

    return pbc->numPrograms;
}


int getTrickIdx(playbackContext *pb, FILE *fp, FILE *fpTrick, iFramePointer *iframeList, uint32_t numPkts, uint16_t searchPID, int printHeader, int printHex, int printGOPIdx, int printPICIdx)
{
    // variables used to get bit rate from content file
    uint32_t mpegPackets = 0;
    unsigned char mpegPkt[256];
    unsigned int PID, CC, FLAGS, i;
    size_t bytesRead;
    unsigned char ScrambleCtl, AdaptFieldCtl;
    unsigned char AdaptLength, AdaptFlags;
    int discIndicator, randAccIndicator, pesPrio, pcrIndicator;
    //unsigned long long optPCR, optOPCR;
    unsigned int seqNo;
    unsigned char frameType, hour, minute, second, frame;
    unsigned long long byteCnt;
    uint32_t prevIframe=0;
    uint32_t frameByteCnt=0, otherByteCnt=0;
    uint32_t diffIframe=0;
    uint32_t iframeCnt=0, pcrCnt=0;
    int getLength=FALSE;
    uint32_t horizSize, vertSize, aspectRatio, frameRate, bitRate;
    unsigned long long pcrTimestamp;
    long curOffset;

    int sync = 0;
    pb->numPAT=0;
    pb->numPMT=0;
    pb->numVideo=0;
    pb->numAudio=0;
    pb->numNull=0;
    pb->numUnknown=0;
    pb->numTSPackets=0;
    pb->numBytes=0;

    curOffset=ftell(fp);

    if(curOffset > 0)
    {
        printf("curOffset=%ld, will rewind\n", curOffset);
        if(fseek(fp, 0, SEEK_SET) < 0)
        {
            //perror("bitstream fseek");
            printf("\nbitstream fseek failed\n");
            SYSLOG_ASSERT(0, strerror(errno));
        }
    }

    //printf("\nScanning content file for GOP offsets\n");
    do
    {
 
        bytesRead=0;
        do
        {
            bytesRead+=fread(&mpegPkt[bytesRead], sizeof(uint8_t), MPEG2_PACKET_SIZE, fp);
        } while((bytesRead < MPEG2_PACKET_SIZE) && !feof(fp));

        mpegPackets++;
        pb->numTSPackets++;
        pb->numBytes+=bytesRead;

        byteCnt = mpegPackets * MPEG2_PACKET_SIZE;

        if ((sync % (MPEG2_PACKET_SIZE*100) == 0) && !printHex && !printGOPIdx && !printPICIdx && !printHeader)
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
            SYSLOG_ASSERT(0, strerror(errno));
        }
        else
        {

            if(printHeader && 0)
                printf("S");

            // Set offest to sync - WORKS OK for VP MUX STREAMS
            iframeList[pb->numIframe].syncOffset=byteCnt;

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


            if(printHeader && 0)
            {
                printf("\nMPEG Pkt=%lu, PID=%04X(%04d), CC=%02X, AFCtl=%01X, AFLength=%d, AFlags=%02X, discIndicator=%d\n",
                       mpegPackets, PID, PID, CC, AdaptFieldCtl, AdaptLength, AdaptFlags, discIndicator);
            }

            // Assuming that sptsParsePMT was called first, this will keep stats on PIDs seen
            if(PID == pb->patPID) pb->numPAT++;
            else if(PID == pb->pmtPID[0]) pb->numPMT++;
            else if(PID == pb->videoPID[0]) pb->numVideo++;
            else if(PID == pb->audioPID[0]) pb->numAudio++;
            else if(PID == pb->nullPID) pb->numNull++;
            else pb->numUnknown++;
            
        }

        if(PID == searchPID) // MUST be a Video PID
        {
            pb->matchCnt++;

            pcrIndicator=((AdaptFlags & 0x10) > 0);
            pcrTimestamp=0x0000000000000000;
            pcrTimestamp|= ((unsigned long long)mpegPkt[6])<<40;
            pcrTimestamp|= ((unsigned long long)mpegPkt[7])<<32;
            pcrTimestamp|= ((unsigned long long)mpegPkt[8])<<24;
            pcrTimestamp|= ((unsigned long long)mpegPkt[9])<<16;
            pcrTimestamp|= ((unsigned long long)mpegPkt[10])<<8;
            pcrTimestamp|= ((unsigned long long)mpegPkt[11]);

            // Assume that PCR PID is the video PID
            if(pcrIndicator) pcrCnt++;

            if(printHeader)
                printf("V");

            if(printHeader && 0)
                printf("[PCR]");

            // For video a SEQUENCE contains:
            //                 GOP contains:
            //                     PICTURES contains:
            //                         SLICES

            if(printHeader && 0)
            {
                printf("\nMPEG Pkt=%lu, PID=%04X(%04d), CC=%02X, AFCtl=%01X, AFLength=%d, AFlags=%02X, discIndicator=%d, pcrIndicator=%d, pcrCnt=%lu, pcrTS=%lld:\n",
                       mpegPackets, PID, PID, CC, AdaptFieldCtl, AdaptLength, AdaptFlags, discIndicator, pcrIndicator, pcrCnt, pcrTimestamp);
            }

            // Detect Sequence header for MPEG-2
            // Will be: 0x00 0x00 0x01 0xB3
            for(i=0; i<=180; i++)
            {
                if((mpegPkt[i] == 0x00) &&
                   (mpegPkt[i+1] == 0x00) &&
                   (mpegPkt[i+2] == 0x01) &&
                   (mpegPkt[i+3] == 0xB3))
                {

                    if(printHeader)
                    {
                        printf("\nMPEG Pkt=%lu, i=%d, PID=%04X(%04d), CC=%02X, AFCtl=%01X, AFLength=%d, AFlags=%02X, discIndicator=%d, pcrIndicator=%d, pcrCnt=%lu, pcrTS=%lld:\n",
                               mpegPackets, i, PID, PID, CC, AdaptFieldCtl, AdaptLength, AdaptFlags, discIndicator, pcrIndicator, pcrCnt, pcrTimestamp);
                    }

                    pb->numSeq++;

                    // Set on MPEG packet boundaries
                    iframeList[pb->numIframe].seqOffset=byteCnt;
                    iframeList[pb->numIframe].gopOffset=byteCnt;  
                    //iframeList[pb->numIframe].seqOffset=(byteCnt+i);
                    //iframeList[pb->numIframe].gopOffset=(byteCnt+i);  // in case GOP missing, set it to SEQ offset

                    // 12 bits = byte 4 7:0, byte 5 7:4
                    horizSize = (mpegPkt[i+4]<<4) & 0x00000FF0;
                    horizSize |= ((mpegPkt[i+5]>>4) & 0x0F);

                    // 12 bits = byte 5 3:0, byte 6 7:0
                    vertSize = (((mpegPkt[i+5])&0x0F)<<8) & 0x00000F00;
                    vertSize |= ((mpegPkt[i+6]) & 0x000000FF);

                    // 4 bits = byte 7 7:4
                    aspectRatio = (((mpegPkt[i+7])>>4) & 0x0F) & 0x0000000F;

                    // 4 bits = byte 7 3:0
                    frameRate = (((mpegPkt[i+7])) & 0x0F);

                    // 18 bits = byte 8 7:0, byte 9 7:0, byte 10 7:6
                    bitRate = (((mpegPkt[i+8])<<10) & 0x00003C00);
                    bitRate |= (((mpegPkt[i+9])<<2) & 0x000003FC);
                    bitRate |= ((mpegPkt[i+10])>>6) ;

                    if(printHeader)
                    {
                        printf("\nSEQUENCE MPEG Pkt=%lu, PID=%04X(%04d), Offset=%lld, Length=%d, horizSize=%lu, vertSize=%lu, aspectRatio=%lu, frameRate=%lu, bitRate=%lu:\n",
                               mpegPackets, PID, PID, iframeList[pb->numIframe].seqOffset, iframeList[pb->numIframe].seqLength,
                               horizSize, vertSize, aspectRatio, frameRate, bitRate);

                        printf("        Bytes 4 to 10 = 0x%02x 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", mpegPkt[i+4], mpegPkt[i+5], mpegPkt[i+6], mpegPkt[i+7], mpegPkt[i+8], mpegPkt[i+9], mpegPkt[i+10]);

                        printf("        horizSize=0x%lx, %ld\n", horizSize, horizSize);
                        printf("        vertSize=0x%lx, %ld\n", vertSize, vertSize);
                        printf("        aspectRatio=0x%lx, %ld\n", aspectRatio, aspectRatio);
                        printf("        frameRate=0x%lx, %ld\n", frameRate, frameRate);
                        printf("        bitRate=0x%lx, %ld\n", bitRate, bitRate);

                    }

                }
                // only one SEQ header per MPEG packet assumed
                //break;
            }


            // Detect GOP header for MPEG-2 video
            // Will be: 0x00 0x00 0x01 0xB8
            for(i=0; i<=180; i++)
            {
                if((mpegPkt[i] == 0x00) &&
                   (mpegPkt[i+1] == 0x00) &&
                   (mpegPkt[i+2] == 0x01) &&
                   (mpegPkt[i+3] == 0xB8))
                {
                    pb->numGOP++;

                    iframeList[pb->numIframe].gopOffset=byteCnt;
                    //iframeList[pb->numIframe].gopOffset=(byteCnt+i);

                    if(printHeader)
                    {
                        printf("\n        GOP MPEG Pkt=%ld, PID=%04X(%04d), Offset=%llu:\n",
                               mpegPackets, PID, PID, iframeList[pb->numIframe].gopOffset);
                    }

                    // 5 bits = byte 4 6:2 = hour
                    hour = ( ((mpegPkt[i+4])>>2) & 0x3F );

                    // 6 bits = byte 4 1:0, byte 5 7:4 = minute
                    minute = ( (((mpegPkt[i+4])<<4) & 0x30) | (((mpegPkt[i+5])>>4) & 0x0F) );

                    // 6 bits = byte 5 2:0, byte 6 7:5 = second
                    second = ( (((mpegPkt[i+5])<<3) & 0x38) | (((mpegPkt[i+6])>>5) & 0x07) );

                    // 6 bits = byte 6 4:0, byte 7:7 = frame
                    frame = ( ((((mpegPkt[i+6])<<1) & 0x3E) | ((mpegPkt[i+7])>>7)) & 0x01);

                    if(printGOPIdx)
                    {
                        printf("GOP: packet=%ld, ID=0x%02X, hr=%d, m=%d, s=%d, f=%d, byte=%llu\n",
                               mpegPackets, mpegPkt[i+3], hour, minute, second, frame, (byteCnt+i));
                    }
                }

                // only one GOP header per MPEG packet assumed
                //break;
            }

            // Detect Picture header for MPEG-2 video
            // Will be: 0x00 0x00 0x01 0x00
            //
            // Parse frame type and increment appropriate counter
            //
            for(i=0; i<=180; i++)
            {
                if((mpegPkt[i] == 0x00) &&
                   (mpegPkt[i+1] == 0x00) &&
                   (mpegPkt[i+2] == 0x01) &&
                   (mpegPkt[i+3] == 0x00))
                {
                    pb->numPIC++;

                    // 10 bits = byte 4 7:0, byte 5 7:6 = seqNo
                    seqNo = ( (((mpegPkt[i+4])<<10) & 0x03FF) | (((mpegPkt[i+5])>>6) & 0x3) );

                    // 3 bits = byte 5 5:3 = frameType
                    frameType = ( ((mpegPkt[i+5]) & 0x38)>>3 );

                    if(frameType == IFRAME)
                    {
                        // Set offest to IFRAME - for VP RAW STREAMS
                        iframeList[pb->numIframe].iframeOffset=byteCnt;
                        //iframeList[pb->numIframe].iframeOffset=byteCnt+i;

                        if(getLength == FALSE)
                        {
                            otherByteCnt=0;
                            frameByteCnt=0;
                            getLength=TRUE;
                        }

                        if(printHeader)
                        {
                            printf("\n                 IFRAME MPEG Pkt=%ld, i=%d, PID=%04X(%04d), Offset=%lld, Seq-Offset=%lld, Offset-from-seq=%lld:\n",
                                   mpegPackets, i, PID, PID, iframeList[pb->numIframe].iframeOffset,
                                   iframeList[pb->numIframe].seqOffset,
                                   ((iframeList[pb->numIframe].iframeOffset)-(iframeList[pb->numIframe].seqOffset)));
                        }

                        pb->numIframe++;
                        iframeCnt=pb->numPIC;
                    }

                    if(frameType == IFRAME)
                    {
                        diffIframe = pb->numPIC - prevIframe;
                        if(printPICIdx)
                        {
                            printf("I-FRAME(%d) pkt=%ld, seqNo=%d, byte=%llu\n",
                                   pb->numPIC, mpegPackets, seqNo, (byteCnt+i));
                        }
                        if(printHeader)
                            printf("\nI\n");
                        prevIframe=pb->numPIC;
                    }
                    else if(frameType == PFRAME)
                    {
                        if(printPICIdx)
                        {
                        printf("P-FRAME(%d) pkt=%ld, seqNo=%d, byte=%llu\n",
                               pb->numPIC, mpegPackets, seqNo, (byteCnt+i));
                        }
                        if(getLength)
                        {
                            iframeList[pb->numIframe-1].iframeLength=frameByteCnt;
                            iframeList[pb->numIframe-1].trickLength=otherByteCnt;

                            // Set Trick play parameters for either I-frame or GOP offsets and lengths
                            iframeList[pb->numIframe-1].trickOffset=iframeList[pb->numIframe-1].seqOffset;
                            iframeList[pb->numIframe-1].gopLength=
                            (iframeList[pb->numIframe-1].iframeOffset-iframeList[pb->numIframe-1].gopOffset)+iframeList[pb->numIframe-1].trickLength;
                            iframeList[pb->numIframe-1].seqLength=
                            (iframeList[pb->numIframe-1].iframeOffset-iframeList[pb->numIframe-1].seqOffset)+iframeList[pb->numIframe-1].trickLength;

                            if(printHeader)
                            {
                                printf("\nSEQ @ %lld, GOP @ %lld, I-frame @ %lld, trick @ %lld: SEQlen=%d, GOPlen=%d, IFlen=%d, TrickLen=%d\n", 
                                       iframeList[pb->numIframe-1].seqOffset,
                                       iframeList[pb->numIframe-1].gopOffset,
                                       iframeList[pb->numIframe-1].iframeOffset,
                                       iframeList[pb->numIframe-1].trickOffset,
                                       iframeList[pb->numIframe-1].seqLength,
                                       iframeList[pb->numIframe-1].gopLength,
                                       iframeList[pb->numIframe-1].iframeLength,
                                       iframeList[pb->numIframe-1].trickLength);
                                printf("I-frame length=%d, Total=%d\n", iframeList[pb->numIframe-1].iframeLength, iframeList[pb->numIframe-1].trickLength);
                            }

                            // Set offset and length to use in playback here
                            iframeList[pb->numIframe-1].offset=iframeList[pb->numIframe-1].iframeOffset;
                            iframeList[pb->numIframe-1].length=iframeList[pb->numIframe-1].iframeLength;
                            //iframeList[pb->numIframe-1].offset=iframeList[pb->numIframe-1].seqOffset;
                            //iframeList[pb->numIframe-1].length=iframeList[pb->numIframe-1].seqLength;

                            getLength=FALSE;
                        }
                        if(printHeader)
                            printf("\nP\n");

                    }
                    else if(frameType == BFRAME)
                    {
                        if(printPICIdx)
                        {
                            printf("B-FRAME(%d) pkt=%ld, seqNo=%d, byte=%llu\n",
                                   pb->numPIC, mpegPackets, seqNo, (byteCnt+i));
                        }
                        if(getLength)
                        {
                            iframeList[pb->numIframe-1].iframeLength=frameByteCnt;
                            iframeList[pb->numIframe-1].trickLength=otherByteCnt;

                            // Set Trick play parameters for either I-frame or GOP offsets and lengths
                            iframeList[pb->numIframe-1].trickOffset=iframeList[pb->numIframe-1].seqOffset;
                            iframeList[pb->numIframe-1].gopLength=
                            (iframeList[pb->numIframe-1].iframeOffset-iframeList[pb->numIframe-1].gopOffset)+iframeList[pb->numIframe-1].trickLength;
                            iframeList[pb->numIframe-1].seqLength=
                            (iframeList[pb->numIframe-1].iframeOffset-iframeList[pb->numIframe-1].seqOffset)+iframeList[pb->numIframe-1].trickLength;

                            if(printHeader)
                            {
                                printf("\nSEQ @ %lld, GOP @ %lld, I-frame @ %lld, trick @ %lld: SEQlen=%d, GOPlen=%d, IFlen=%d, TrickLen=%d\n", 
                                       iframeList[pb->numIframe-1].seqOffset,
                                       iframeList[pb->numIframe-1].gopOffset,
                                       iframeList[pb->numIframe-1].iframeOffset,
                                       iframeList[pb->numIframe-1].trickOffset,
                                       iframeList[pb->numIframe-1].seqLength,
                                       iframeList[pb->numIframe-1].gopLength,
                                       iframeList[pb->numIframe-1].iframeLength,
                                       iframeList[pb->numIframe-1].trickLength);
                                printf("I-frame length=%d, Total=%d\n", iframeList[pb->numIframe-1].iframeLength, iframeList[pb->numIframe-1].trickLength);
                            }

                            // Set offset and length to use in playback here
                            iframeList[pb->numIframe-1].offset=iframeList[pb->numIframe-1].iframeOffset;
                            iframeList[pb->numIframe-1].length=iframeList[pb->numIframe-1].iframeLength;
                            //iframeList[pb->numIframe-1].offset=iframeList[pb->numIframe-1].seqOffset;
                            //iframeList[pb->numIframe-1].length=iframeList[pb->numIframe-1].seqLength;

                            getLength=FALSE;
                        }
                        if(printHeader)
                            printf("\nB\n");

                    }
                    else if(printPICIdx)
                    {
                        printf("FRAME(%d) pkt=%ld, seqNo=%d, byte=%llu\n",
                               pb->numPIC, mpegPackets, seqNo, (byteCnt+i));

                        if(getLength)
                        {
                            iframeList[pb->numIframe-1].iframeLength=frameByteCnt;
                            iframeList[pb->numIframe-1].trickLength=otherByteCnt;

                            // Set Trick play parameters for either I-frame or GOP offsets and lengths
                            iframeList[pb->numIframe-1].trickOffset=iframeList[pb->numIframe-1].seqOffset;
                            iframeList[pb->numIframe-1].gopLength=
                            (iframeList[pb->numIframe-1].iframeOffset-iframeList[pb->numIframe-1].gopOffset)+iframeList[pb->numIframe-1].trickLength;
                            iframeList[pb->numIframe-1].seqLength=
                            (iframeList[pb->numIframe-1].iframeOffset-iframeList[pb->numIframe-1].seqOffset)+iframeList[pb->numIframe-1].trickLength;

                            if(printHeader)
                            {
                                printf("SEQ @ %lld, GOP @ %lld, I-frame @ %lld, trick @ %lld: SEQlen=%d, GOPlen=%d, IFlen=%d, TrickLen=%d\n", 
                                       iframeList[pb->numIframe-1].seqOffset,
                                       iframeList[pb->numIframe-1].gopOffset,
                                       iframeList[pb->numIframe-1].iframeOffset,
                                       iframeList[pb->numIframe-1].trickOffset,
                                       iframeList[pb->numIframe-1].seqLength,
                                       iframeList[pb->numIframe-1].gopLength,
                                       iframeList[pb->numIframe-1].iframeLength,
                                       iframeList[pb->numIframe-1].trickLength);
                                printf("\nI-frame length=%d, Total=%d\n", iframeList[pb->numIframe-1].iframeLength, iframeList[pb->numIframe-1].trickLength);
                            }

                            // Set offset and length to use in playback here
                            iframeList[pb->numIframe-1].offset=iframeList[pb->numIframe-1].iframeOffset;
                            iframeList[pb->numIframe-1].length=iframeList[pb->numIframe-1].iframeLength;
                            //iframeList[pb->numIframe-1].offset=iframeList[pb->numIframe-1].seqOffset;
                            //iframeList[pb->numIframe-1].length=iframeList[pb->numIframe-1].seqLength;

                            getLength=FALSE;
                        }
                        if(printHeader)
                            printf("\n?\n");

                    }

                    // only one PIC header per MPEG packet assumed
                    //break;

                }
            }

            // Detect PES header for MPEG-2 video
            // Will be: 0x00 0x00 0x01 0xE0-0xEF and 16-bit packet length
            for(i=0; i<=180; i++)
            {
                if((mpegPkt[i] == 0x00) &&
                   (mpegPkt[i+1] == 0x00) &&
                   (mpegPkt[i+2] == 0x01) &&
                   ((mpegPkt[i+3] >= 0xE0) && (mpegPkt[i+3] <= 0xEF)))
                {
                    pb->numPES++;

                    if(printHex)
                    {
                        printf("\n        PES Hdr: ID=0x%02X, length=%d\n",
                               mpegPkt[i+3], (uint16_t)(*(&mpegPkt[i+4])));
                    }
                }

            }


            if(printHex)
            {
                //dumpMPEG2Packet(mpegPkt);
            }

            if(getLength == TRUE)
            {
                frameByteCnt+=MPEG2_PACKET_SIZE;
                otherByteCnt+=MPEG2_PACKET_SIZE;
            }

        }
        else if(printHeader)
        {
            if(getLength == TRUE)
                otherByteCnt+=MPEG2_PACKET_SIZE;
            printf("M");
        }

    } while(!feof(fp)); // while read

    //rewind file
    rewind(fp);
    fflush(fp);

    // Save off trick file built up here using trick file pointer if not NULL
    if(fpTrick != NULL)
    {
        printf("SAVING with trick file save pointer supplied\n");
        fwrite(&pb->numIframe, sizeof(pb->numIframe), 1, fpTrick);
        for(i=0;i<pb->numIframe;i++)
        {
            fwrite(&iframeList[i], sizeof(iFramePointer), 1, fpTrick);
        }
    }
    else
    {
        printf("No trick file save pointer supplied\n");
    }

    return pb->numIframe;
}


