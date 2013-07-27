#ifndef _PARSE_H_
#define _PARSE_H_

// 13818-1 PCR definitions
#define SYSTEM_CLOCK_FREQUENCY_MIN                  26999190
#define SYSTEM_CLOCK_FREQUENCY                      27000000
#define SYSTEM_CLOCK_FREQUENCY_MAX                  27000810

// 13818-1 definitions
#define CRC_32_SIZE                                    4

// 13818-1 table IDs
#define SI_DATA_PAT 			                    0x00
#define SI_DATA_CAT 			                    0x01
#define SI_DATA_PMT 			                    0x02
#define SI_DATA_TSDT 			                    0x03

// 13818-1 Table 2-5, Adaptation field control values
#define ADAPT_FIELD_CTL_RESERVED                    0x0
#define ADAPT_FIELD_CTL_PAYLOAD_ONLY                0x1
#define ADAPT_FIELD_CTL_NO_PAYLOAD                  0x2
#define ADAPT_FIELD_CTL_PLUS_PAYLOAD                0x3

// 13818-1 Stream types from Table 2-29
#define ISO_STREAM_TYPE_MPEG2_VIDEO                 0x02
#define ISO_STREAM_TYPE_MPEG2_DSMCC                 0x14

// 13818-2 Program stream start codes
#define PICTURE_START_CODE                          0x00
#define SLICE_START_CODE_LOWER_BOUND                0x01
#define SLICE_START_CODE_UPPER_BOUND                0xAF
#define RESERVED_START_CODE_1                       0xB0
#define RESERVED_START_CODE_2                       0xB1
#define USER_DATA_START_CODE                        0xB2
#define SEQUENCE_HEADER_START_CODE                  0xB3
#define SEQUENCE_ERROR_START_CODE                   0xB4
#define EXTENSION_START_CODE                        0xB5
#define RESERVED_START_CODE_3                       0xB6
#define SEQUENCE_END_START_CODE                     0xB7
#define GOP_START_CODE                              0xB8

// Non-documented stream_id's found on
// http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html
#define PROGRAM_END_STREAM_ID                       0xB9
#define PACK_HEADER_STREAM_ID                       0xBA
#define SYSTEM_HEADER_STREAM_ID                     0xBB

// 13818-1 Program stream_id assignments from Table 2-18
#define PROGRAM_STREAM_MAP                          0xBC
#define PRIVATE_STREAM_1                            0xBD
#define PADDING_STREAM                              0xBE
#define PRIVATE_STREAM_2                            0xBF
#define AUDIO_STREAM_LOWER_BOUND                    0xC0 // C0 - DF
#define AUDIO_STREAM_UPPER_BOUND                    0xDF // C0 - DF
#define VIDEO_STREAM_LOWER_BOUND                    0xE0 // E0 - EF
#define VIDEO_STREAM_UPPER_BOUND                    0xEF // E0 - EF
#define ECM_STREAM                                  0xF0
#define EMM_STREAM                                  0xF1
#define DSMCC_STREAM                                0xF2
#define ISO_13522_STREAM                            0xF3
#define ITU_H222_TYPE_A                             0xF4
#define ITU_H222_TYPE_B                             0xF5
#define ITU_H222_TYPE_C                             0xF6
#define ITU_H222_TYPE_D                             0xF7
#define ITU_H222_TYPE_E                             0xF8
#define ANCILLARY_STREAM                            0xF9
#define ISO_14496_SL_PACKETIZED_STREAM              0xFA
#define ISO_14496_SL_FLEXMUX_STREAM                 0xFB
#define RESERVED_DATA_STREAM_LOWER_BOUND            0xFC
#define RESERVED_DATA_STREAM_UPPER_BOUND            0xFE
#define PROGRAM_STREAM_DIRECTORY                    0xFF


// ATSC A/65C Table IDs
#define SI_DATA_SYSTEM_TIME_TABLE 			0xCD
#define SI_DATA_MASTER_GUIDE_TABLE 			0xC7
#define SI_DATA_TERRESTRIAL_VIRTUAL_CHANNEL_TABLE 	0xC8
#define SI_DATA_CABLE_VIRTUAL_CHANNEL_TABLE 		0xC9
#define SI_DATA_RATING_REGION_TABLE 			0xCA
#define SI_DATA_EIT 					0xCB
#define SI_DATA_BIT_STREAM_SYNTAX_TEXT_TABLE		0xCC
#define SI_DATA_DIRECTED_CHANNEL_CHANGE_TABLE		0xD3

#define MAX_TITLE_TEXT_LENGTH                           32
#define MULTI_STRING_STRUCT_LENGTH                      (8+MAX_TITLE_TEXT_LENGTH)
#define RRT_REGION_TEXT_LENGTH                          9 // letter rating

#define NUM_TEST_CHANNELS 3

// These structures are packed so that they are consistent with specs and
// defined for x86 Little-endian format.  Note that MPEG-2 Transport Streams
// are network byte order (Big-endian).  This means that any 16-bit or 32-bit
// word should be initialized using ntohs or ntohl when reading in transport data
// for parsing.  When multi-byte data is to be sent out in a transport stream, htons
// or htonl should be called.
//
// Each structure is defined in terms of the spec, but also includes a union overlay
// for byte, word, dword access to simplify byte-ordering adjustment.
//
// In some cases bit-fields are defined in an order differing from the spec within a
// byte/word/dword because bitwise ordering issues were noted - these have all been well
// commented.  The order does match the spec at the level of bytes/words/dwords.
//
// Print functions for all of the structs have been defined in parse.c along with
// dumpMPEG2Packet which provides a hex byte by byte dump that can be useful for
// verification.
//
// 


#pragma pack(1)
// HEADER: Basic TS header up to Adaptation
typedef union
{
    struct
    {
        unsigned sync_byte :8; 

        // reverse order of spec
        unsigned PID :13;
        unsigned transport_priority :1;
        unsigned payload_unit_start_indicator :1;
        unsigned transport_error_indicator :1;

        // reverse order of spec
        unsigned continuity_counter :4;
        unsigned adaptation_field_control:2;
        unsigned transport_scrambling_control :2;

    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned char b2;
    } overlay;

    // adaptation field data if any
} tsPacketHeader;


// SI Data headers up to descriptor sections based upon 13818-1

// PAT: Program Allocation Table
typedef union
{
    unsigned table_id :8; 

    // reverse order of spec
    unsigned section_length :12;
    unsigned :2;
    unsigned zero_literal :1;
    unsigned section_syntax_indicator :1;

    unsigned transport_stream_id :16;

    // reverse order of spec
    unsigned current_next_indicator :1;
    unsigned version_number :5;
    unsigned :2;

    unsigned section_number :8;
    unsigned last_section_number :8;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
    } overlay;


    // descriptors
    // CRC_32
} patHeader;


// CAT: Conditional Access Table
typedef union
{
    struct
    {
        unsigned table_id :8; 
    
        unsigned section_syntax_indicator :1;
        unsigned zero_literal :1;
        unsigned res1 :2;
        unsigned section_length :12;

        unsigned res2 :16;

        // reverse order of spec
        unsigned current_next_indicator :1;
        unsigned version_number :5;
        unsigned res3 :2;

        unsigned section_number :8;
        unsigned last_section_number :8;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
    } overlay;

    // descriptors
    // CRC_32
} catHeader;


// PMT: Program Map Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        unsigned section_syntax_indicator :1;
        unsigned zero_literal :1;
        unsigned res1 :2;
        unsigned section_length :12;

        unsigned program_number :16;

        // reverse order of spec
        unsigned current_next_indicator :1;
        unsigned version_number :5;
        unsigned res2 :2;

        unsigned section_number :8;
        unsigned last_section_number :8;

        // reverse order of spec
        unsigned PCR_PID :13;
        unsigned res3 :3;

        unsigned res4 :4;
        unsigned program_info_length :12;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
        unsigned short w6;
        unsigned short w7;
    } overlay;

    // descriptors
    // elementary PID descriptors
    // CRC_32
} pmtHeader;


// Private Header
typedef union
{
    struct
    {
        unsigned table_id :8; 

        // reverse order from spec
        unsigned private_section_length :12;
        unsigned :2;
        unsigned private_indicator :1;
        unsigned section_syntax_indicator :1;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
    } overlay;

    // descriptors
    // descriptors
    // CRC_32
} privateHeader;


// TSDT: Transport Stream Description Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        unsigned section_syntax_indicator :1;
        unsigned zero_literal :1;
        unsigned res1 :2;
        unsigned section_length :12;

        unsigned res2 :16;

        unsigned res3 :2;
        unsigned version_number :5;
        unsigned current_next_indicator :1;

        unsigned section_number :8;
        unsigned last_section_number :8;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
    } overlay;

    // descriptors
    // CRC_32
} tsdtHeader;


// SI Data headers up to descriptor sections based upon ATSC A/65C

// STT: System Time Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        // reverse order of spec
        unsigned section_length :12;
        unsigned res1:2;
        unsigned private_indicator :1;
        unsigned section_syntax_indicator:1;

        unsigned table_id_extension :16;

        // reverse order of spec
        unsigned current_next_indicator :1;
        unsigned version_number :5;
        unsigned res2:2;

        unsigned section_number :8;
        unsigned last_section_number :8;
        unsigned protocol_version :8;
        unsigned system_time :32;
        unsigned GPS_UTC_offset :8;

        // reverse order of spec
        unsigned DS_day_of_month :5;
        unsigned res3 :2;
        unsigned DS_status :1;

        unsigned DS_hour :8;

    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
        unsigned char b6;
        unsigned int dw7;
        unsigned char b8;
        unsigned char b9;
        unsigned char b10;
    } overlay;

    // descriptors
    // CRC_32
} systemTimeTable;


// MGT: Master Guide Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        // reverse order of spec
        unsigned section_length :12;
        unsigned res1 :2;
        unsigned private_indicator :1;
        unsigned section_syntax_indicator:1;

        unsigned table_id_extension :16;

        // reverse order of spec
        unsigned current_next_indicator :1;
        unsigned version_number :5;
        unsigned res2 :2;

        unsigned section_number :8;
        unsigned last_section_number :8;
        unsigned protocol_version :8;

        unsigned tables_defined :16;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
        unsigned char b6;
        unsigned short w7;
    } overlay;

    // table definition entrys (mGuideEntry)
    // reserved:4, descriptors_length:12, descriptors
    // CRC_32

} masterGuideTable;


// MGE: Master Guide Entry
typedef union
{
    struct
    {
        unsigned table_type :16;

        // reverse order of spec
        unsigned table_type_PID :13;
        unsigned res1:3;

        // reverse order of spec
        unsigned table_type_version_number :5;
        unsigned res2:3;

        unsigned number_bytes :32;

        // reverse order of spec
        unsigned table_type_descriptors_length :12;
        unsigned res3:4;
    } spec;

    struct
    {
        unsigned short w0;
        unsigned short w1;
        unsigned char b2;
        unsigned int dw3;
        unsigned short w4;
    } overlay;

    // descriptors

} masterGuideEntry;


// TVCT: Terrestrial Virtual Channel Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        // reverse order of spec
        unsigned section_syntax_indicator:1;
        unsigned private_indicator :1;
        unsigned res1 :2;
        unsigned section_length :12;

        unsigned transport_stream_id :16;

        // reverse order of spec
        unsigned res2 :2;
        unsigned version_number :5;
        unsigned current_next_indicator :1;

        unsigned section_number :8;
        unsigned last_section_number :8;
        unsigned protocol_version :8;
        unsigned num_channels_in_section :8;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
        unsigned char b6;
        unsigned char b7;
    } overlay;

    // channel definitions
    // reserved:6, additional_descriptors_length:10, descriptors
    // CRC_32

} terrVirtChannelTable;

// TVCE: Terrestrial Virtual Channel Entry
typedef union
{
    struct
    {
        unsigned short short_name[7];

        unsigned res1 :4;
        unsigned major_channel_number:10;
        unsigned minor_channel_number:10;
        unsigned modulation_mode :8;

        unsigned carrier_frequency :32;
        unsigned channel_TSID :16;
        unsigned program_number :16;

        unsigned ETM_location:2;
        unsigned access_controlled :1;
        unsigned hidden :1;
        unsigned res2 :2;
        unsigned hide_guide :1;
        unsigned res3 :3;
        unsigned service_type :6;

        unsigned source_id :16;

        unsigned res4 :6;
        unsigned descriptors_length :10;
    } spec;

    struct
    {
        unsigned short w0[7];
        unsigned int dw14;
        unsigned int dw15;
        unsigned short w16;
        unsigned short w17;
        unsigned short w18;
        unsigned short w19;
        unsigned short w20;
    } overlay;

    // descriptors
    // reserved:6, additional_descriptors_length:10, descriptors
    // CRC_32
} terrVirtChannelEntry;


// CVCT: Cable Virtual Channel Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        // reverse order of spec
        unsigned section_length :12;
        unsigned res1 :2;
        unsigned private_indicator :1;
        unsigned section_syntax_indicator:1;

        unsigned transport_stream_id :16;

        // reverse order of spec
        unsigned current_next_indicator :1;
        unsigned version_number :5;
        unsigned res2 :2;

        unsigned section_number :8;
        unsigned last_section_number :8;
        unsigned protocol_version :8;
        unsigned num_channels_in_section :8;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
        unsigned char b6;
        unsigned char b7;
    } overlay;

    // channel definitions
    // reserved:6, additional_descriptors_length:10, descriptors
    // CRC_32
} cableVirtChannelTable;


// CVCE: Cable Virtual Channel Entry
typedef union
{
    struct 
    {
        unsigned short short_name[7];

        // reverse order of spec
        unsigned int modulation_mode :8;
        unsigned int minor_channel_number:10;
        unsigned int major_channel_number:10;
        unsigned int res1:4;    

        unsigned int carrier_frequency;
        unsigned short channel_TSID;
        unsigned short program_number;

        // reverse order of spec
        unsigned short service_type :6;
        unsigned short res2:3;
        unsigned short hide_guide :1;
        unsigned short out_of_band :1;
        unsigned short path_select :1;
        unsigned short hidden :1;
        unsigned short access_controlled :1;
        unsigned short ETM_location:2;
        
        unsigned short source_id;

        // reverse order of spec
        unsigned short descriptors_length :10;
        unsigned short res3:6;
    } spec;

    struct
    {
        unsigned short w0[7];
        unsigned int dw14;
        unsigned int dw15;
        unsigned short w16;
        unsigned short w17;
        unsigned short w18;
        unsigned short w19;
        unsigned short w20;
    } overlay;

    // descriptors
    // reserved:6, additional_descriptors_length:10, descriptors
    // CRC_32
} cableVirtChannelEntry;


// RRT: Region Rating Table
typedef union
{
    struct
    {
        unsigned char table_id :8; 

        // reverse order of spec
        unsigned short section_length :12;
        unsigned short res1 :2;
        unsigned short private_indicator :1;
        unsigned short section_syntax_indicator:1;

        unsigned short res2;
        unsigned char rating_region;

        // reverse order of spec
        unsigned char current_next_indicator :1;
        unsigned char version_number :5;
        unsigned char res3 :2;

        unsigned char section_number;
        unsigned char last_section_number;
        unsigned char protocol_version;
        unsigned char rating_region_name_length;
        unsigned char rating_region_name_text[RRT_REGION_TEXT_LENGTH];  // minimum
        unsigned char dimensions_defined;

        // Rating Region Entries

    } spec;

    struct
    {
        unsigned char b0; // table id
        unsigned short w1;
        unsigned short w2; // reserved -- this should be a byte according to the spec, but had to make a word based on Sencore
        unsigned char b3; // rating region code
        unsigned char b4; // reserved+version_number+current_next_ind
        unsigned char b5; // section number
        unsigned char b6; // last section number
        unsigned char b7; // protocol version
        unsigned char b8; // rating region name length
        unsigned char str10[RRT_REGION_TEXT_LENGTH];
        unsigned char b11; // dimensions defined
    } overlay;

    // reserved:6, additional_descriptors_length:10, descriptors
    // CRC_32
} regionRatingTable;


// RRE: Region Rating Entry
typedef union
{
    struct 
    {
        unsigned dimension_name_length :8;
        unsigned char dimension_name_text[20];

        unsigned res1 :3;
        unsigned graduated_scale :1;
        unsigned values_defined :4;

        // Array of ratingRegionValues
    } spec;

    struct
    {
        unsigned char b0;
        unsigned char str1[20];
        unsigned char b2;
    } overlay;

} ratingRegionEntry;


// RRV: Region Rating Value
typedef union
{
    struct 
    {
        unsigned char abbrev_rating_value_length;
        unsigned char dimension_name_text[8];  // per ATSC A/65C page 40

        unsigned char rating_value_length;
        unsigned char rating_value_text[150];  // per ATSC A/65C page 40

    } spec;

    struct
    {
        unsigned char b0;
        unsigned char str1[8];
        unsigned char b2;
        unsigned char str3[150];
    } overlay;

} ratingRegionValue;



// EIT: Event Information Table
typedef union
{
    struct
    {
        unsigned table_id :8; 

        // reverse order of spec
        unsigned section_length :12;
        unsigned res1 :2;
        unsigned private_indicator :1;
        unsigned section_syntax_indicator:1;

        unsigned source_id :16;

        // reverse order of spec
        unsigned current_next_indicator :1;
        unsigned version_number :5;
        unsigned res2 :2;

        unsigned section_number :8;
        unsigned last_section_number :8;
        unsigned protocol_version :8;
        unsigned num_events_in_section :8;
    } spec;

    struct
    {
        unsigned char b0;
        unsigned short w1;
        unsigned short w2;
        unsigned char b3;
        unsigned char b4;
        unsigned char b5;
        unsigned char b6;
        unsigned char b7;
    } overlay;

    // event definitions
    // CRC_32
} eventInformationTable;


// EIE: Event Information Entry
typedef union
{
    struct
    {
        // reverse order of spec
        unsigned event_id :14;
        unsigned res1 :2;

        unsigned start_time :32;

        // reverse order of spec
        unsigned title_length :8;
        unsigned length_in_seconds :20;
        unsigned ETM_location :2;
        unsigned res2 :2;

        //8 bytes for mult string struct fields
        unsigned char multString[MULTI_STRING_STRUCT_LENGTH];
        
        // reverse order of spec
        unsigned descriptors_length :12;
        unsigned res3 :4;
    } spec;

    struct
    {
        unsigned short w0;
        unsigned int dw1;
        unsigned int dw2;
        unsigned char a3[MULTI_STRING_STRUCT_LENGTH];
        unsigned short w4;
    } overlay;

    // title text
    // reserved:4, descriptors_length:12, descriptors
} eventInfoEntry;



// prototypes for verification functions
//
void printSysTimeTable(systemTimeTable *stt);
int printMasterGuideTable(masterGuideTable *mgt, unsigned char *tablePtr, masterGuideEntry *mge, int maxGuideEntries);
void printEventInfoTable(eventInformationTable *eit, unsigned char *tablePtr, eventInfoEntry *eie, int maxEvents);
void printTVCTable(terrVirtChannelTable *tvct, unsigned char *tablePtr, terrVirtChannelEntry *tvce, int maxVCs);
void printCVCTable(cableVirtChannelTable *cvct, unsigned char *tablePtr, cableVirtChannelEntry *cvce, int maxVCs);
void dumpMPEG2Packet(unsigned char *packetBuffer);

#endif
