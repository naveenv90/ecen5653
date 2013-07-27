#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <wchar.h>

#include "ts.h"
#include "parse.h"
#include "timeapi.h"

/**
 * dumpMPEG2Packet will print one 188 byte transport stream packet in hex.
 *
 * @param packetBuffer is a pointer to the TS packet.
 */
void dumpMPEG2Packet(unsigned char *packetBuffer)
{
    int i;
    unsigned char FLAGS, CC;
    unsigned short PID;
    tsPacketHeader tsp;

    FLAGS=packetBuffer[1] & 0xE0;
    PID=((packetBuffer[1] & 0x1F)<<8) | (packetBuffer[2]);
    CC=packetBuffer[3] & 0x0F;

    printf("\nMPEG Packet:\n");
    printf("PID=0x%02x\n", PID);
    printf("FLAGS=0x%02x\n", FLAGS);
    printf("CC=0x%02x\n", CC);

    tsp.overlay.b0=packetBuffer[0];
    tsp.overlay.w1=ntohs(*((unsigned short *)&packetBuffer[1]));
    tsp.overlay.b2=packetBuffer[3];

    printf("tsp.PID=0x%02x\n", tsp.spec.PID);
    printf("FLAGS, tsp.transport_priority=%u\n", tsp.spec.transport_priority);
    printf("FLAGS, tsp.payload_unit_start_indicator=%u\n", tsp.spec.payload_unit_start_indicator);
    printf("FLAGS, tsp.transport_error_indicator=%u\n", tsp.spec.transport_error_indicator);
    printf("tsp.continuity_counter=%u (0x%02x)\n", tsp.spec.continuity_counter, tsp.spec.continuity_counter);
    printf("tsp.adaptation_field_control=%u\n", tsp.spec.adaptation_field_control);
    printf("tsp.transport_scrambling_control=%u\n", tsp.spec.transport_scrambling_control);

    for(i=0; i<184; i+=8)
    {
        printf("0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
               packetBuffer[i], packetBuffer[i+1], packetBuffer[i+2], packetBuffer[i+3],
               packetBuffer[i+4], packetBuffer[i+5], packetBuffer[i+6], packetBuffer[i+7]);
    }
    printf("0x%02X 0x%02X 0x%02X 0x%02X\n",
           packetBuffer[184], packetBuffer[185], packetBuffer[186], packetBuffer[187]);
}


void printTitleString(unsigned char *strPtr)
{
    int i, j, k;
    unsigned char number_strings=strPtr[0];
    unsigned char number_segments, compression_type, mode, number_bytes;
    int cnt=0;

    //printf("number_strings=%u\n", number_strings);

    for(i=0;i<number_strings;i++)
    {
        //language_code=strPtr[cnt+1]; // 3 bytes
        number_segments=strPtr[cnt+4];
        //printf("number_segments=%u\n", number_segments);

        for(j=0;j<number_segments;j++)
        {
            compression_type=strPtr[cnt+5];
            mode=strPtr[cnt+6];
            number_bytes=strPtr[cnt+7];
            //printf("compression_type=%u\n", compression_type);
            //printf("mode=%u\n", mode);
            //printf("number_bytes=%u\n", number_bytes);

            if(compression_type == 0x00)
            {
                for(k=0;k<number_bytes;k++)
                {
                    printf("%c", strPtr[k+cnt+8]);
                }
                printf("\n"); 
            }
            else
            {
                printf("COMPRESSED TITLE DATA\n");
                return;
            }

            cnt=cnt+number_bytes+3;
        }

        cnt++;
    }
}


void printEventInfoTable(eventInformationTable *eit, unsigned char *tablePtr, eventInfoEntry *eie, int maxEvents)
{
    int i, cnt=0;
    //int j;
    unsigned int days, hours, minutes, seconds;

    printf("Event Info Table:\n");
    printf("\ttable_id=%d (0x%0x)\n", eit->spec.table_id, eit->spec.table_id);
    printf("\tsection_syntax_indicator=%d\n", eit->spec.section_syntax_indicator);
    printf("\tprivate_indicator=%d\n", eit->spec.private_indicator);
    printf("\tsection_length=%d\n", eit->spec.section_length);
    printf("\tsource_id=%d\n", eit->spec.source_id);
    printf("\tversion_number=%d\n", eit->spec.version_number);
    printf("\tcurrent_next_indicator=%d\n", eit->spec.current_next_indicator);
    printf("\tsection_number=%d\n", eit->spec.section_number);
    printf("\tlast_section_number=%d\n", eit->spec.last_section_number);
    printf("\tprotocol_version=%d\n", eit->spec.protocol_version);
    printf("\tnum_events_in_section=%d\n", eit->spec.num_events_in_section);
    printf("Event Info Table Raw:\n");
    printf("\tb0=%u (0x%02x)\n", eit->overlay.b0, eit->overlay.b0);
    printf("\tw1=%u (0x%04x)\n", eit->overlay.w1, eit->overlay.w1);
    printf("\tw2=%u (0x%04x)\n", eit->overlay.w2, eit->overlay.w2);
    printf("\tb3=%u (0x%02x)\n", eit->overlay.b3, eit->overlay.b3);
    printf("\tb4=%u (0x%02x)\n", eit->overlay.b4, eit->overlay.b4);
    printf("\tb5=%u (0x%02x)\n", eit->overlay.b5, eit->overlay.b5);
    printf("\tb6=%u (0x%02x)\n", eit->overlay.b6, eit->overlay.b6);
    printf("\tb7=%u (0x%02x)\n", eit->overlay.b7, eit->overlay.b7);


    for(i=0;((i<eit->spec.num_events_in_section) && (i<maxEvents));i++)
    {
        eie[i].overlay.w0=ntohs(*((unsigned short *)&tablePtr[cnt+0]));
        eie[i].overlay.dw1=ntohl(*((unsigned int *)&tablePtr[cnt+2]));
        eie[i].overlay.dw2=ntohl(*((unsigned int *)&tablePtr[cnt+6]));

        days = eie[i].spec.start_time / SECONDS_IN_DAY;
        hours = (eie[i].spec.start_time - (days*SECONDS_IN_DAY)) / SECONDS_IN_HOUR;
        minutes = (eie[i].spec.start_time - (days*SECONDS_IN_DAY) - (hours*SECONDS_IN_HOUR)) / SECONDS_IN_MINUTE;
        seconds = (eie[i].spec.start_time - (days*SECONDS_IN_DAY) - (hours*SECONDS_IN_HOUR) - (minutes*SECONDS_IN_MINUTE));

        printf("\tEvent Entry:\n");
        printf("\t\tevent_id=%d (0x%0x)\n", eie[i].spec.event_id, eie[i].spec.event_id);

        printf("\t\tstart_time=%u (seconds since 00:00:00 UTC Jan 6th 1980)\n", eie[i].spec.start_time);
        printf("\t\tstart_time=%02d:%02d:%02d:%02d (seconds since 00:00:00 UTC Jan 6th 1980)\n", days, hours, minutes, seconds);

        printf("\t\tETM_location=%d\n", eie[i].spec.ETM_location);
        printf("\t\tlength_in_seconds=%d\n", eie[i].spec.length_in_seconds);
        printf("\t\ttitle_length=%d\n", eie[i].spec.title_length);
        //printf("Title=%s\n", &tablePtr[cnt+sizeof(eventInfoEntry)]);
        printf("\t\tTITLE=");printTitleString(&tablePtr[cnt+sizeof(eventInfoEntry)]);

        printf("\n\tEvent Entry Raw:\n");
        printf("\t\tw0=%u (0x%04x)\n", eie[i].overlay.w0, eie[i].overlay.w0);
        printf("\t\tdw1=%u (0x%08x)\n", eie[i].overlay.dw1, eie[i].overlay.dw1);
        printf("\t\tdw2=%u (0x%08x)\n", eie[i].overlay.dw2, eie[i].overlay.dw2);
        //for(j=0;j<eie[i].spec.title_length;j++)
        //    printf("0x%02x(%c)", tablePtr[cnt+sizeof(eventInfoEntry)+j], tablePtr[cnt+sizeof(eventInfoEntry)+j]);
        printf("\n");

        cnt=cnt+sizeof(masterGuideEntry)+eie[i].spec.title_length;
    }

}

int printMasterGuideTable(masterGuideTable *mgt, unsigned char *tablePtr, masterGuideEntry *mge, int maxGuideEntries)
{
    int i, cnt=0;

    printf("Master Guide Table:\n");
    printf("\ttable_id=%d (0x%02x)\n", mgt->spec.table_id, mgt->spec.table_id);
    printf("\tsection_syntax_indicator=%d\n", mgt->spec.section_syntax_indicator);
    printf("\tprivate_indicator=%d\n", mgt->spec.private_indicator);
    printf("\tsection_length=%d (0x%03x)\n", mgt->spec.section_length, mgt->spec.section_length);
    printf("\ttable_id_extension=%d\n", mgt->spec.table_id_extension);
    printf("\tversion_number=%d\n", mgt->spec.version_number);
    printf("\tcurrent_next_indicator=%d\n", mgt->spec.current_next_indicator);
    printf("\tsection_number=%d\n", mgt->spec.section_number);
    printf("\tlast_section_number=%d\n", mgt->spec.last_section_number);
    printf("\tprotocol_version=%d\n", mgt->spec.protocol_version);
    printf("\ttables_defined=%d (0x%04x)\n", mgt->spec.tables_defined,  mgt->spec.tables_defined);
    printf("Master Guide Table Raw:\n");
    printf("\tb0=%d (0x%02x)\n", mgt->overlay.b0, mgt->overlay.b0);
    printf("\tw1=%d (0x%04x)\n", mgt->overlay.w1, mgt->overlay.w1);
    printf("\tw2=%d (0x%04x)\n", mgt->overlay.w2, mgt->overlay.w2);
    printf("\tb3=%d (0x%02x)\n", mgt->overlay.b3, mgt->overlay.b3);
    printf("\tb4=%d (0x%02x)\n", mgt->overlay.b4, mgt->overlay.b4);
    printf("\tb5=%d (0x%02x)\n", mgt->overlay.b5, mgt->overlay.b5);
    printf("\tb6=%d (0x%02x)\n", mgt->overlay.b6, mgt->overlay.b6);
    printf("\tw7=%d (0x%04x)\n", mgt->overlay.w7, mgt->overlay.w7);

    for(i=0;((i<mgt->spec.tables_defined) && (i<maxGuideEntries));i++)
    {
        mge[i].overlay.w0=ntohs(*((unsigned short *)&tablePtr[cnt+0]));
        mge[i].overlay.w1=ntohs(*((unsigned short *)&tablePtr[cnt+2]));
        mge[i].overlay.b2=tablePtr[cnt+4];
        mge[i].overlay.dw3=ntohl(*((unsigned int *)&tablePtr[cnt+5]));
        mge[i].overlay.w4=ntohs(*((unsigned short *)&tablePtr[cnt+9]));

        printf("\tTable Entry (%d):\n", i);
        printf("\t\ttable_type=%d (0x%0x)\n", mge[i].spec.table_type, mge[i].spec.table_type);
        printf("\t\ttable_type_PID=%d (0x%0x)\n", mge[i].spec.table_type_PID, mge[i].spec.table_type_PID);
        printf("\t\ttable_type_version_number=%d (0x%0x)\n", mge[i].spec.table_type_version_number, mge[i].spec.table_type_version_number);
        printf("\t\tnumber_bytes=%d (0x%0x)\n", mge[i].spec.number_bytes, mge[i].spec.number_bytes);
        printf("\t\ttable_type_descriptors_length=%d\n", mge[i].spec.table_type_descriptors_length);
        printf("\tTable Entry Raw (%d):\n", i);
        printf("\t\tw0=%u (0x%04x)\n", mge[i].overlay.w0, mge[i].overlay.w0);
        printf("\t\tw1=%u (0x%04x)\n", mge[i].overlay.w1, mge[i].overlay.w1);
        printf("\t\tb2=%u (0x%02x)\n", mge[i].overlay.b2, mge[i].overlay.b2);
        printf("\t\tdw3=%u (0x%08x)\n", mge[i].overlay.dw3, mge[i].overlay.dw3);
        printf("\t\tw4=%u (0x%04x)\n", mge[i].overlay.w4, mge[i].overlay.w4);

        cnt=cnt+sizeof(masterGuideEntry);
    }

    if(i >= maxGuideEntries)
    {
        SYSLOG_ASSERT(0, strerror(errno));
        return -1;
    }
    else
    {
        return i;
    }
}

void printTVCTable(terrVirtChannelTable *tvct, unsigned char *tablePtr, terrVirtChannelEntry *tvce, int maxVCs)
{
    int i, j, cnt=0;

    printf("TVC Table:\n");
    printf("\ttable_id=%d (0x%0x)\n", tvct->spec.table_id, tvct->spec.table_id);
    printf("\tsection_syntax_indicator=%d\n", tvct->spec.section_syntax_indicator);
    printf("\tprivate_indicator=%d\n", tvct->spec.private_indicator);
    printf("\tsection_length=%d\n", tvct->spec.section_length);
    printf("\ttransport_stream_id=%d\n", tvct->spec.transport_stream_id);
    printf("\tversion_number=%d\n", tvct->spec.version_number);
    printf("\tcurrent_next_indicator=%d\n", tvct->spec.current_next_indicator);
    printf("\tsection_number=%d\n", tvct->spec.section_number);
    printf("\tlast_section_number=%d\n", tvct->spec.last_section_number);
    printf("\tprotocol_version=%d\n", tvct->spec.protocol_version);
    printf("\tnum_channels_in_section=%d\n", tvct->spec.num_channels_in_section);

    for(i=0;((i<tvct->spec.num_channels_in_section) && (i<maxVCs));i++)
    {
        for(j=0;j<14;j+=2)
        {
            tvce[i].overlay.w0[j/2]=ntohs(*((unsigned short *)&tablePtr[cnt+j]));
        }

        tvce[i].overlay.dw14=ntohl(*((unsigned int *)&tablePtr[cnt+0]));
        tvce[i].overlay.dw15=ntohl(*((unsigned int *)&tablePtr[cnt+4]));
        tvce[i].overlay.w16=ntohs(*((unsigned short *)&tablePtr[cnt+8]));
        tvce[i].overlay.w17=ntohs(*((unsigned short *)&tablePtr[cnt+10]));
        tvce[i].overlay.w18=ntohs(*((unsigned short *)&tablePtr[cnt+12]));
        tvce[i].overlay.w19=ntohs(*((unsigned short *)&tablePtr[cnt+14]));
        tvce[i].overlay.w20=ntohs(*((unsigned short *)&tablePtr[cnt+16]));

        printf("\tChannel Entry:\n");
        for(j=0;j<7;j++)
        {
            printf("\t\tshort_name=%c\n", (char)(tvce[i].spec.short_name[cnt+j]));
            //printf("\t\tshort_name=");putwchar(tvce[i].spec.short_name[cnt+j]);printf("\n");
        }

        printf("\t\tmajor_channel_number=%d\n", tvce[i].spec.major_channel_number);
        printf("\t\tminor_channel_number=%d\n", tvce[i].spec.minor_channel_number);
        printf("\t\tmodulation_mode=%d (0x%0x)\n", tvce[i].spec.modulation_mode, tvce[i].spec.modulation_mode);
        printf("\t\tcarrier_frequency=%d (0x%0x)\n", tvce[i].spec.carrier_frequency, tvce[i].spec.carrier_frequency);
        printf("\t\tchannel_TSID=%d (0x%0x)\n", tvce[i].spec.channel_TSID, tvce[i].spec.channel_TSID);
        printf("\t\tprogram_number=%d (0x%0x)\n", tvce[i].spec.program_number, tvce[i].spec.program_number);
        printf("\t\taccess_controlled=%d\n", tvce[i].spec.access_controlled);
        printf("\t\thidden=%d\n", tvce[i].spec.hidden);
        printf("\t\thide_guide=%d\n", tvce[i].spec.hide_guide);
        printf("\t\tservice_type=%d\n", tvce[i].spec.service_type);
        printf("\t\tsource_id=%d\n", tvce[i].spec.source_id);
        printf("\t\tdescriptors_length=%d\n", tvce[i].spec.descriptors_length);

        printf("\tChannel Entry Raw:\n");
        for(j=0;j<7;j++)
        {
            printf("\t\tw0=%u (0x%04x)\n", tvce[i].overlay.w0[j], tvce[cnt+i].overlay.w0[cnt+j]);
        }

        printf("\t\tdw14=%u (0x%08x)\n", tvce[i].overlay.dw14, tvce[i].overlay.dw14);
        printf("\t\tdw15=%u (0x%08x)\n", tvce[i].overlay.dw15, tvce[i].overlay.dw15);
        printf("\t\tw16=%u (0x%04x)\n", tvce[i].overlay.w16, tvce[i].overlay.w16);
        printf("\t\tw17=%u (0x%04x)\n", tvce[i].overlay.w17, tvce[i].overlay.w17);
        printf("\t\tw18=%u (0x%04x)\n", tvce[i].overlay.w18, tvce[i].overlay.w18);
        printf("\t\tw19=%u (0x%04x)\n", tvce[i].overlay.w19, tvce[i].overlay.w19);
        printf("\t\tw20=%u (0x%04x)\n", tvce[i].overlay.w20, tvce[i].overlay.w20);

        cnt=cnt+sizeof(terrVirtChannelEntry);
    }

}


void printCVCTable(cableVirtChannelTable *cvct, unsigned char *tablePtr, cableVirtChannelEntry *cvce, int maxVCs)
{
    int i, j, cnt=0;

    printf("CVC Table:\n");
    printf("\ttable_id=%d (0x%0x)\n", cvct->spec.table_id, cvct->spec.table_id);
    printf("\tsection_syntax_indicator=%d\n", cvct->spec.section_syntax_indicator);
    printf("\tprivate_indicator=%d\n", cvct->spec.private_indicator);
    printf("\tsection_length=%d\n", cvct->spec.section_length);
    printf("\ttransport_stream_id=%d\n", cvct->spec.transport_stream_id);
    printf("\tversion_number=%d\n", cvct->spec.version_number);
    printf("\tcurrent_next_indicator=%d\n", cvct->spec.current_next_indicator);
    printf("\tsection_number=%d\n", cvct->spec.section_number);
    printf("\tlast_section_number=%d\n", cvct->spec.last_section_number);
    printf("\tprotocol_version=%d\n", cvct->spec.protocol_version);
    printf("\tnum_channels_in_section=%d\n", cvct->spec.num_channels_in_section);

    for(i=0;((i<cvct->spec.num_channels_in_section) && (i<maxVCs));i++)
    {
        for(j=0;j<14;j+=2)
        {
            cvce[i].overlay.w0[j/2]=ntohs(*((unsigned short *)&tablePtr[cnt+j]));
        }

        cvce[i].overlay.dw14=ntohl(*((unsigned int *)&tablePtr[cnt+0]));
        cvce[i].overlay.dw15=ntohl(*((unsigned int *)&tablePtr[cnt+4]));
        cvce[i].overlay.w16=ntohs(*((unsigned short *)&tablePtr[cnt+8]));
        cvce[i].overlay.w17=ntohs(*((unsigned short *)&tablePtr[cnt+10]));
        cvce[i].overlay.w18=ntohs(*((unsigned short *)&tablePtr[cnt+12]));
        cvce[i].overlay.w19=ntohs(*((unsigned short *)&tablePtr[cnt+14]));
        cvce[i].overlay.w20=ntohs(*((unsigned short *)&tablePtr[cnt+16]));


        printf("\tChannel Entry:\n");
        for(j=0;j<7;j++)
        {
            printf("\t\tshort_name=%c\n", (char)(cvce[i].spec.short_name[cnt+j]));
            //printf("\t\tshort_name=");putwchar(cvce[i].spec.short_name[cnt+j]);printf("\n");
        }

        printf("\t\tmajor_channel_number=%d\n", cvce[i].spec.major_channel_number);
        printf("\t\tminor_channel_number=%d\n", cvce[i].spec.minor_channel_number);
        printf("\t\tmodulation_mode=%d (0x%0x)\n", cvce[i].spec.modulation_mode, cvce[i].spec.modulation_mode);
        printf("\t\tcarrier_frequency=%d (0x%0x)\n", cvce[i].spec.carrier_frequency, cvce[i].spec.carrier_frequency);
        printf("\t\tchannel_TSID=%d (0x%0x)\n", cvce[i].spec.channel_TSID, cvce[i].spec.channel_TSID);
        printf("\t\tprogram_number=%d (0x%0x)\n", cvce[i].spec.program_number, cvce[i].spec.program_number);
        printf("\t\tETM_location=%d\n", cvce[i].spec.ETM_location);
        printf("\t\taccess_controlled=%d\n", cvce[i].spec.access_controlled);
        printf("\t\thidden=%d\n", cvce[i].spec.hidden);
        printf("\t\tpath_select=%d\n", cvce[i].spec.path_select);
        printf("\t\tout_of_band=%d\n", cvce[i].spec.out_of_band);
        printf("\t\thide_guide=%d\n", cvce[i].spec.hide_guide);
        printf("\t\tservice_type=%d\n", cvce[i].spec.service_type);
        printf("\t\tsource_id=%d\n", cvce[i].spec.source_id);
        printf("\t\tdescriptors_length=%d\n", cvce[i].spec.descriptors_length);

        printf("\tChannel Entry Raw:\n");
        for(j=0;j<7;j++)
        {
            printf("\t\tw0=%u (0x%04x)\n", cvce[i].overlay.w0[cnt+j], cvce[i].overlay.w0[cnt+j]);
        }

        printf("\t\tdw14=%u (0x%08x)\n", cvce[i].overlay.dw14, cvce[i].overlay.dw14);
        printf("\t\tdw15=%u (0x%08x)\n", cvce[i].overlay.dw15, cvce[i].overlay.dw15);
        printf("\t\tw16=%u (0x%04x)\n", cvce[i].overlay.w16, cvce[i].overlay.w16);
        printf("\t\tw17=%u (0x%04x)\n", cvce[i].overlay.w17, cvce[i].overlay.w17);
        printf("\t\tw18=%u (0x%04x)\n", cvce[i].overlay.w18, cvce[i].overlay.w18);
        printf("\t\tw19=%u (0x%04x)\n", cvce[i].overlay.w19, cvce[i].overlay.w19);
        printf("\t\tw20=%u (0x%04x)\n", cvce[i].overlay.w20, cvce[i].overlay.w20);

        cnt=cnt+sizeof(cableVirtChannelEntry);
    }

}


void printSysTimeTable(systemTimeTable *stt)
{
    unsigned int days, hours, minutes, seconds;

    days = stt->spec.system_time / SECONDS_IN_DAY;
    hours = (stt->spec.system_time - (days*SECONDS_IN_DAY)) / SECONDS_IN_HOUR;
    minutes = (stt->spec.system_time - (days*SECONDS_IN_DAY) - (hours*SECONDS_IN_HOUR)) / SECONDS_IN_MINUTE;
    seconds = (stt->spec.system_time - (days*SECONDS_IN_DAY) - (hours*SECONDS_IN_HOUR) - (minutes*SECONDS_IN_MINUTE));
    printf("System Time Table:\n");
    printf("\ttable_id=%u (0x%0x)\n", stt->spec.table_id, stt->spec.table_id);
    printf("\tsection_syntax_indicator=%u\n", stt->spec.section_syntax_indicator);
    printf("\tprivate_indicator=%u\n", stt->spec.private_indicator);
    printf("\tsection_length=%u\n", stt->spec.section_length);
    printf("\ttable_id_extension=%u\n", stt->spec.table_id_extension);
    printf("\tversion_number=%u\n", stt->spec.version_number);
    printf("\tcurrent_next_indicator=%u\n", stt->spec.current_next_indicator);
    printf("\tsection_number=%u\n", stt->spec.section_number);
    printf("\tlast_section_number=%u\n", stt->spec.last_section_number);
    printf("\tprotocol_version=%u\n", stt->spec.protocol_version);
    printf("\tsystem_time=%u (seconds since 00:00:00 UTC Jan 6th 1980)\n", stt->spec.system_time);
    printf("\tsystem_time=%d days and %02d:%02d:%02d (seconds since 00:00:00 UTC Jan 6th 1980)\n", days, hours, minutes, seconds);
    printDateFromATSCEpoch(stt->spec.system_time);
    printf("\tGPS_UTC_offset=%u (between GPS and UTC time)\n", stt->spec.GPS_UTC_offset);
    printf("\tDS_Status=0x%0x\n", stt->spec.DS_status);
    printf("\tDS_day_of_month=0x%0x\n", stt->spec.DS_day_of_month);
    printf("\tDS_hour=0x%0x\n", stt->spec.DS_hour);
    printf("System Time Table Raw:\n");
    printf("\tb0=%u (0x%02x)\n", stt->overlay.b0, stt->overlay.b0);
    printf("\tw1=%u (0x%04x)\n", stt->overlay.w1, stt->overlay.w1);
    printf("\tw2=%u (0x%04x)\n", stt->overlay.w2, stt->overlay.w2);
    printf("\tb3=%u (0x%02x)\n", stt->overlay.b3, stt->overlay.b3);
    printf("\tb4=%u (0x%02x)\n", stt->overlay.b4, stt->overlay.b4);
    printf("\tb5=%u (0x%02x)\n", stt->overlay.b5, stt->overlay.b5);
    printf("\tb6=%u (0x%02x)\n", stt->overlay.b6, stt->overlay.b6);
    printf("\tdw7=%u (0x%08x)\n", stt->overlay.dw7, stt->overlay.dw7);
    printf("\tb8=%u (0x%02x)\n", stt->overlay.b8, stt->overlay.b8);
    printf("\tb9=%u (0x%02x)\n", stt->overlay.b9, stt->overlay.b9);
    printf("\tb10=%u (0x%02x)\n", stt->overlay.b10, stt->overlay.b10);
    //printf("\tw9=%u (0x%04x)\n", stt->overlay.w9, stt->overlay.w9);
}


void printTableID(unsigned char table_id)
{
    switch(table_id)
    {
        case SI_DATA_SYSTEM_TIME_TABLE:
            printf("SI_DATA_SYSTEM_TIME_TABLE\n");
        break;

        case SI_DATA_MASTER_GUIDE_TABLE:
            printf("SI_DATA_MASTER_GUIDE_TABLE\n");
        break;

        case SI_DATA_TERRESTRIAL_VIRTUAL_CHANNEL_TABLE:
            printf("SI_DATA_TERRESTRIAL_VIRTUAL_CHANNEL_TABLE\n");
        break;

        case SI_DATA_CABLE_VIRTUAL_CHANNEL_TABLE:
            printf("SI_DATA_CABLE_VIRTUAL_CHANNEL_TABLE\n");
        break;

        case SI_DATA_RATING_REGION_TABLE:
            printf("SI_DATA_RATING_REGION_TABLE\n");
        break;

        case SI_DATA_EIT:
            printf("SI_DATA_EIT\n");
        break;

        case SI_DATA_BIT_STREAM_SYNTAX_TEXT_TABLE:
            printf("SI_DATA_BIT_STREAM_SYNTAX_TEXT_TABLE\n");
        break;

        case SI_DATA_DIRECTED_CHANNEL_CHANGE_TABLE:
            printf("SI_DATA_DIRECTED_CHANNEL_CHANGE_TABLE\n");
        break;

        default:
            printf("UNKNOWN TABLE ID\n");
    }

}
