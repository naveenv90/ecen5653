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

int findAllPIDs(FILE *fp, unsigned int packetLimit);
int parsePAT(FILE *fp, playbackContext *pbc, streamContext *strc);
void printPlayback(playbackContext *pbPtr, int pbCnt);
int findAllTiming(FILE *fp, unsigned int packetLimit, playbackContext *pbPtr);

playbackContext pb;

int main (int argc, char *argv[])
{
    int numPIDs;
    streamContext sc;

    if(argc < 2)
    {
        printf("Usage: %s <transport stream file>\n", argv[0]);
        //SYSLOG_ASSERT(0, strerror(errno));
    }
 
    if((pb.fp = fopen(argv[1],"r+b")) == NULL)
    {
        perror("failure on MPEG-2 TS file open");
        SYSLOG_ASSERT(0, strerror(errno));
    }
    else
    {
        printf("opened %s\n", argv[1]);
    }


    numPIDs=findAllPIDs(pb.fp, 10000);
    //numPIDs=findAllPIDs(pb.fp, 0xFFFFFFFF);
    
    // Get detailed PID info
    parsePAT(pb.fp, &pb, &sc);

    findAllTiming(pb.fp, 10000, &pb);
    //findAllTiming(pb.fp, 0xFFFFFFFF, &pb);

    printf("numPIDs=%d\n", numPIDs);

    printf("\n\nSUMMARY OF INFO:\n");
    printPlayback(&pb, 1);

    fclose(pb.fp);

    return OK;
};


int findAllPIDs(FILE *fp, unsigned int packetLimit)
{

    int i, payloadStart, numUniquePIDs=0, mpegPackets=0, byteCnt, bytesRead, uniquePID=TRUE;
    unsigned int PID, CC, FLAGS;
    unsigned short uniquePIDList[256];
    unsigned char ScrambleCtl, AdaptFieldCtl;
    unsigned char AdaptLength, AdaptFlags;
    int discIndicator, randAccIndicator, pesPrio;
    unsigned char packetBuffer[256];
    int sync = 0;
    long curOffset;

    for(i=0;i<256;i++)
        uniquePIDList[i]=UNKNOWN_PID;

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

    do  // Scan file for PIDs
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
                printf("\nPID SEARCH: parsed UNIQUE PID=%d (0x%x), total=%d @ MPEG packet=%d\n", PID, PID, numUniquePIDs, mpegPackets);

                // Uncomment and add PID checks to dump packets as needed for debug
#if 0
                if(PID == NULL_PID)
                {
                    printf("NULL PID Packet Dump:\n");
                    dumpMPEG2Packet(packetBuffer);
                }
#endif

            }


        } // end good packet with sync found


    } while (!feof(fp) && (mpegPackets < packetLimit)); // end while not all PIDs found and not end of file

    printf("%d MPEG packets parsed\n", mpegPackets);

    return numUniquePIDs;
};


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
    
                        printf("\n The value of PID is %d and pbc->pmtPID is %d",PID,pbc->pmtPID[i]);
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
                                    printf("\n The value of pbc->videoPID is %d",pbc->videoPID[0]);
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
};


void printPlayback(playbackContext *pbPtr, int pbCnt)
{
    int i, j;

    printf("\nPlayback count=%d:\n", pbCnt);

    for(i=0;i<pbCnt;i++)
    {
        printf("STREAM %d\n", i+1);
        printf("devID=%d, port=%d\n", pbPtr[i].devID, pbPtr[i].port);
        printf("streamID=%d, streamName=%s, transportBitRate=%d\n", pbPtr[i].streamID, pbPtr[i].streamName, pbPtr[i].transportBitRate);
        printf("TRIPLET INFO: annex=%d, freq=%u, mode=%d, port=%hu, progNum=%hu, tsid=%hu\n",
               pbPtr[i].tripletInfo.annex, pbPtr[i].tripletInfo.center_channel_freq,
               pbPtr[i].tripletInfo.mode, pbPtr[i].tripletInfo.port, pbPtr[i].tripletInfo.programNum, pbPtr[i].tripletInfo.transport_stream_id);
        printf("numPrograms=%d\n", pbPtr[i].numPrograms);

        for(j=0;j<pbPtr[i].numPrograms;j++)
        {
            printf("PROGRAM %d\n", pbPtr[i].program_number[j]);
            printf("\tvideoPID[%d]=%d\n", j, pbPtr[i].videoPID[j]);
            printf("\taudioPID[%d]=%d\n", j, pbPtr[i].audioPID[j]);
            printf("\tpmtPID[%d]=%d\n", j, pbPtr[i].pmtPID[j]);
            printf("\tpcrPID[%d]=%d\n", j, pbPtr[i].pcrPID[j]);
        }
        printf("\n");
    }
};


int findAllTiming(FILE *fp, unsigned int packetLimit, playbackContext *pbPtr)
{
    int payloadStart, numPCRs=0, byteCnt, bytesRead, i;
    unsigned int PID, CC, FLAGS;
    unsigned char ScrambleCtl, AdaptFieldCtl, pesFlags, pesHdrLen;
    unsigned char bitField1, bitField2, bitField3, bitField4, bitField5;
    unsigned char AdaptLength, AdaptFlags;
    int discIndicator, randAccIndicator, pesPrio, pcrFlag, opcrFlag;
    int splicing_point_flag, transport_private_data_flag, adaptation_field_extenstion_flag;
    unsigned char packetBuffer[256];
    int sync = 0;
    long curOffset;
    unsigned long long curPCR=0, prevPCR=0, curPTS=0, curDTS=0;
    unsigned short curPCRRem=0, prevPCRRem=0;
    unsigned long long mpegPackets=0;
    double PCRlast=0.0, PCRnow=0.0, timeNow=0.0, timeLast=0.0;
    unsigned long long lastPCRPacket=0, nowPCRPacket=0;
    double PTSnow=0.0, ptsTime=0.0, DTSnow=0.0, dtsTime=0.0, transportRate=0.0;

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

    do  // Scan file for PCRs
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
            pesPrio=(AdaptFlags & 0x20) > 0;
            pcrFlag=(AdaptFlags & 0x10) > 0;
            opcrFlag=(AdaptFlags & 0x08) > 0;
            splicing_point_flag=(AdaptFlags & 0x04) > 0;
            transport_private_data_flag=(AdaptFlags & 0x02) > 0;
            adaptation_field_extenstion_flag=(AdaptFlags & 0x01) > 0;
            
            if(PID == 49)
            {
               printf("\n The value of pbr->VIDEOPID[0] is %d",pbPtr->videoPID[0]);
               printf("\n VIDEO PTS FOUND");
            }
            if((PID == pbPtr->pcrPID[0]) &&
               ((AdaptFieldCtl==ADAPT_FIELD_CTL_NO_PAYLOAD) ||
                (AdaptFieldCtl==ADAPT_FIELD_CTL_PLUS_PAYLOAD)) && pcrFlag)
            {
                printf("\nMPEG packet %llu, AdaptFieldCtl=0x%x, adaptFieldLen=%d, adaptFlag=0x%x, pcrFlag=%d, opcrFlag=%d, discFlag=%d PID=%d \n",
                       mpegPackets, AdaptFieldCtl, AdaptLength, AdaptFlags, pcrFlag, opcrFlag, discIndicator,PID);
                numPCRs++;

                // 33-bit PCR, 6 bits reserved, 9-bit PCR-remainder
                //
                curPCR=((((unsigned long long)packetBuffer[6])<<25) |
                        (((unsigned long long)packetBuffer[7])<<17) | 
                        (((unsigned long long)packetBuffer[8])<<9)  | 
                        (((unsigned long long)packetBuffer[9])<<1)  | ((packetBuffer[10]&0x80) > 0));

                curPCRRem=(((packetBuffer[10]&0x01)<<8)&0x1000) | packetBuffer[11];

                nowPCRPacket=mpegPackets;

                printf("PCR-33-bit=%llu (0x%llx), PCR-9-bit-rem=%d (0x%x), PCR Bytes=0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", 
                       curPCR, curPCR, curPCRRem, curPCRRem,
                       packetBuffer[6], packetBuffer[7], packetBuffer[8],
                       packetBuffer[9], packetBuffer[10], packetBuffer[11]);

                printf("PCR (%d) PID Packet Dump:", numPCRs);
                //dumpMPEG2Packet(packetBuffer);

                PCRlast = (((double)prevPCR) * 300.0) + ((double)prevPCRRem);
                PCRnow = (((double)curPCR) * 300.0) + ((double)curPCRRem);

                timeNow=((double)PCRnow)/((double)SYSTEM_CLOCK_FREQUENCY);
                timeLast=((double)PCRlast)/((double)SYSTEM_CLOCK_FREQUENCY);

                printf("\nRETIME: currentPCR=%f, timeNow=%f @ packet %llu, prevPCR=%f timeLast=%f @ packet %llu\n", 
                       PCRnow, timeNow, nowPCRPacket, PCRlast, timeLast, lastPCRPacket);

                transportRate=(((double)(nowPCRPacket - lastPCRPacket)*MPEG2_PACKET_SIZE*BITS_PER_BYTE) *
                               ((double)SYSTEM_CLOCK_FREQUENCY))/(PCRnow-PCRlast);

                printf("RETIME: transportRate=%f\n", transportRate);

                prevPCR=curPCR;
                prevPCRRem=curPCRRem;
                lastPCRPacket=nowPCRPacket;

            }

            if((PID == pbPtr->videoPID[0]) /*&&
               ((AdaptFieldCtl==ADAPT_FIELD_CTL_PAYLOAD_ONLY) ||
                (AdaptFieldCtl==ADAPT_FIELD_CTL_PLUS_PAYLOAD))*/)
            {

                printf("\n Detect PES header for MPEG-2 video");
                // Will be: 0x00 0x00 0x01 StremID
                for(i=0; i<=180; i++)
                {
                    if((packetBuffer[i] == 0x00) &&
                       (packetBuffer[i+1] == 0x00) &&
                       (packetBuffer[i+2] == 0x01))
                    {
                        // VIDEO_STREAM_LOWER_BOUND E0 - EF
                        // VIDEO_STREAM_UPPER_BOUND E0 - EF
                        if((packetBuffer[i+3] != PROGRAM_STREAM_MAP) &&
                           (packetBuffer[i+3] != PADDING_STREAM) &&
                           (packetBuffer[i+3] != PRIVATE_STREAM_2) &&
                           (packetBuffer[i+3] != ECM_STREAM) &&
                           (packetBuffer[i+3] != EMM_STREAM) &&
                           (packetBuffer[i+3] != PROGRAM_STREAM_DIRECTORY) &&
                           (packetBuffer[i+3] != DSMCC_STREAM) &&
                           (packetBuffer[i+3] != ITU_H222_TYPE_E) &&
                           (((packetBuffer[i+3] >= VIDEO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= VIDEO_STREAM_UPPER_BOUND)) ||
                            ((packetBuffer[i+3] >= AUDIO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= AUDIO_STREAM_UPPER_BOUND))
                           )
                          )
                        {
                            // Parse out PTS/DTS timing info

                            // bytes 4,5 = PES_packet_length
                            // bytes 6 = PES_scrambling_control ... original/copy

                            // bytes 7 = PES flags
                            pesFlags = (packetBuffer[i+7]>>6) & 0x03;

                            // bytes 8 = PES_header_data_length
                            pesHdrLen = packetBuffer[i+8];

                            // Time-stamp data
                            if(pesFlags == 0x2)
                            {
                                if((packetBuffer[i+3] >= VIDEO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= VIDEO_STREAM_UPPER_BOUND))
                                {
                                    printf("\nVIDEO PTS present\n");
                                }
                                else if((packetBuffer[i+3] >= AUDIO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= AUDIO_STREAM_UPPER_BOUND))
                                {
                                    printf("\nAUDIO PTS present\n");
                                }
                                else
                                {
                                    printf("UNKNOWN PTS present\n");
                                }

                                // 33-bit PTS/DTS bitfields
                                //
                                bitField1=(packetBuffer[i+9]<<4)&0xE0;
                                bitField2=(packetBuffer[i+10]);
                                bitField3=(packetBuffer[i+11])&0xFE;
                                bitField4=(packetBuffer[i+12]);
                                bitField5=(packetBuffer[i+13])&0xFE;

                                curPTS=((((unsigned long long)bitField1)<<30) |    // upper 3 bits
                                        (((unsigned long long)bitField2)<<22) |    // upper 8 bits
                                        (((unsigned long long)bitField3)<<15) |    // lower 7 bits
                                        (((unsigned long long)bitField4)<<7)  |    // uppper 8 bits
                                        (((unsigned long long)bitField5)));        // lower 7 bits

                                printf("curPTS=%llu (0x%llx), PTS bitFields = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
                                       curPTS, curPTS,
                                       bitField1, bitField2, bitField3, bitField4, bitField5);

                                PTSnow=(((double)curPTS) * 300.0);
                                ptsTime=((double)PTSnow)/((double)SYSTEM_CLOCK_FREQUENCY);

                                printf("PTSnow=%f, ptsTime=%f\n", PTSnow, ptsTime);

                            }
                            else if(pesFlags == 0x3)
                            {
                                if((packetBuffer[i+3] >= VIDEO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= VIDEO_STREAM_UPPER_BOUND))
                                {
                                    printf("\nVIDEO PTS & DTS present\n");
                                }
                                else if((packetBuffer[i+3] >= AUDIO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= AUDIO_STREAM_UPPER_BOUND))
                                {
                                    printf("\nAUDIO PTS & DTS present\n");
                                }
                                else
                                {
                                    printf("UNKNOWN PTS & DTS present\n");
                                }

                                // 33-bit PTS/DTS bitfields
                                //
                                bitField1=(packetBuffer[i+9]<<4)&0xE0;
                                bitField2=(packetBuffer[i+10]);
                                bitField3=(packetBuffer[i+11])&0xFE;
                                bitField4=(packetBuffer[i+12]);
                                bitField5=(packetBuffer[i+13])&0xFE;

                                curPTS=((((unsigned long long)bitField1)<<30) |    // upper 3 bits
                                        (((unsigned long long)bitField2)<<22) |    // upper 8 bits
                                        (((unsigned long long)bitField3)<<15) |    // lower 7 bits
                                        (((unsigned long long)bitField4)<<7)  |    // uppper 8 bits
                                        (((unsigned long long)bitField5)));        // lower 7 bits

                                bitField1=(packetBuffer[i+14]<<4)&0xE0;
                                bitField2=(packetBuffer[i+15]);
                                bitField3=(packetBuffer[i+16])&0xFE;
                                bitField4=(packetBuffer[i+17]);
                                bitField5=(packetBuffer[i+18])&0xFE;

                                curDTS=((((unsigned long long)bitField1)<<30) |    // upper 3 bits
                                        (((unsigned long long)bitField2)<<22) |    // upper 8 bits
                                        (((unsigned long long)bitField3)<<15) |    // lower 7 bits
                                        (((unsigned long long)bitField4)<<7)  |    // uppper 8 bits
                                        (((unsigned long long)bitField5)));        // lower 7 bits

                                printf("curPTS=%llu (0x%llx), PTS bitFields = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
                                       curPTS, curPTS,
                                       bitField1, bitField2, bitField3, bitField4, bitField5);

                                PTSnow=(((double)curPTS) * 300.0);
                                ptsTime=((double)PTSnow)/((double)SYSTEM_CLOCK_FREQUENCY);

                                printf("curDTS=%llu (0x%llx), DTS bitFields = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
                                       curDTS, curDTS,
                                       bitField1, bitField2, bitField3, bitField4, bitField5);

                                DTSnow=(((double)curPTS) * 300.0);
                                dtsTime=((double)PTSnow)/((double)SYSTEM_CLOCK_FREQUENCY);

                                printf("PTSnow=%f, ptsTime=%f\n", PTSnow, ptsTime);
                                printf("DTSnow=%f, dtsTime=%f\n", DTSnow, dtsTime);

                            }
                            else if(pesFlags == 0)
                            {
                                if((packetBuffer[i+3] >= VIDEO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= VIDEO_STREAM_UPPER_BOUND))
                                {
                                    printf("VIDEO NO PTS or DTS present\n");
                                }
                                else if((packetBuffer[i+3] >= AUDIO_STREAM_LOWER_BOUND) && (packetBuffer[i+3] <= AUDIO_STREAM_UPPER_BOUND))
                                {
                                    printf("AUDIO NO PTS or DTS present\n");
                                }
                                else
                                {
                                    printf("UNKNOWN NO PTS or DTS present\n");
                                }
                            }
                            else
                            {
                                printf("VIDEO Bad PTS/DTS flag\n");
                            }

                        }

                    } // end if video PTS/DTS

                } // end for bytes in MPEG packet

            } // end if video PID

        } // end good packet with sync found


    } while (!feof(fp) && (mpegPackets < packetLimit)); // end while not all PIDs found and not end of file

    printf("%llu MPEG packets parsed\n", mpegPackets);

    return numPCRs;
};

