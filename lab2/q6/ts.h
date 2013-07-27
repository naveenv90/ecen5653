#ifndef _TS_H_
#define _TS_H_

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define OK 1
#define ERROR -1
#define TRUE 1
#define FALSE 0

#define MPEG2_PACKET_SIZE	(188)
#define TS_SYNC_BYTE        	(0x47)
#define BITS_PER_BYTE		(8)
#define PSIP_PID	        (0x1FFB)

// Emergency Alert Message in the clear PID
#define EAM_CLEAR_PID       	(0x1FFB)

// Emergency Alert Message for Out-Of-Band use on the extended channel
#define EAM_OOB_PID         	(0x1FFC)

#define NULL_PID	        (0x1FFF)
#define PAT_PID		        (0x0000)
#define CAT_PID		        (0x0001)
#define TSDT_PID	        (0x0002)
#define UNKNOWN_PID	        (0xFFFF)

// Two basic modes defined for now
enum qamModes {qam64=64, qam256=256};
enum annexTypes {annexA=0, annexB=1, annexC=2};

// Typical QAM256 Annex B modulation max bit rate
#define MAX_QAM256_BITRATE	(38810717)


// The basic scheme for managing transport stream hardware is:
//     SYSTEM (0)
//         DEVICES (0-3) - Limit on number of VP PCI cards in one system
//             PORTS (0-7) - The maximum Q8RF configuration
//                 STREAMS (0-10) - 10 SDTV 3.8 Mbps streams is maximum
//                     PROGRAMS (0-15) - Limit=?
//                         PIDS (0-15) - Limit=2^13 (8192), but usually far less
//
// So, the maximum number of SDTV streams would be 10 x 8 x 4 = 320 SDTV streams
//
#define MAX_DEVS                4
#define MAX_PORTS               8
#define MAX_STREAMS             10 // At most 10 SDTV streams on QAM256
#define MAX_PROGRAMS            16
#define MAX_PIDS                16
#define MAX_DEV_PATH            512
#define MAX_PATH_NAME           128
#define MAX_CONFIG_LINE         512
#define ID_NOT_SET              0xFFFF

#define QAM_256_ANNEX_B_MAX_BITRATE 38810717
#define ONE_MEGAHERTZ 1000000
#define ONE_KILOHERTZ 1000
#define ANNEX_B_BASE_FREQ_IN_MHZ 141
#define ANNEX_B_BW_PER_CHANNEL_IN_MHZ 6
#define ANNEX_B_MAX_CHANNEL 158 // This maximum is based upon Appendix A Channel Allocation in SCTE Modern CATV Book

#define NPT_NOW             0x80000000
#define NPT_END             0x7FFFFFFF
#define NPT_START           0x00000000

#define NEXT_GOP 1
#define PREV_GOP -1
#define CURR_GOP 0

#define TRICK_BUFF_SIZE 65424 // 348 188 byte packets, just less than 64K

#define MAX_SI_TABLE_ENTRIES 16

// For MPEG-2 frame parsing
#define IFRAME              (1)
#define PFRAME              (2)
#define BFRAME              (3)
#define DFRAME              (4)
#define MAX_IFRAMES         (65536)

typedef struct pidCtxt
{
  unsigned short PID;
  unsigned char iso_stream_type;
} pidContext;

typedef struct pgrmCtxt
{
    unsigned short programNum;
    unsigned short numPIDS;
    unsigned short pmtPID;
    unsigned short pcrPID;
    pidContext pid[MAX_PIDS];
} programContext;

typedef struct strCtxt
{
    unsigned short streamID;

    // The bitrate should be the maximum for VBR transport streams that is
    // set when the stream is configured with the QAM and the stream will
    // either be NULL padded to match this rate by the QAM or prior to this in
    // the original source or by the SW-mux, so in essence, this is the CBR
    // for this stream when NULL pad is included.
    // 
    // This bitrate is used when stream resources are set up with the QAM
    // transport driver.
    // 
    unsigned int transportBitRate;

    unsigned short numPrograms;
    char sourcePath[MAX_DEV_PATH];
    char sourcePlaylistPath[MAX_DEV_PATH];

    programContext program[MAX_PROGRAMS];
} streamContext;

typedef struct portCtxt
{
    unsigned short portID;
    unsigned short numActiveStreams;

    // A port can transport numerous programs, each uniquely identified for
    // client tuners by the triplet, which includes QAM mode/annex, center
    // channel frequency, and a program number.
    // A stream may be multi-program, so a port can transport many program
    // triplets.
    enum qamModes mode;
    enum annexTypes annex;
    // e.g. 177000000 (177 MhZ) is channel 7 in 6 MhZ spaced QAM-256 mode
    unsigned int center_channel_freq;

    streamContext stream[MAX_STREAMS];
} portContext;

typedef struct devCtxt
{
    unsigned short devID;
    unsigned short numPorts;
    char devPath[MAX_DEV_PATH];
    char boardName[MAX_DEV_PATH];
    portContext port[MAX_PORTS];
} devContext;

typedef struct sysCtxt
{
    unsigned short numDevs;
    char configPath[MAX_DEV_PATH];
    devContext devs[MAX_DEVS];
} sysContext;


// SYSLOG_ASSERT to use for integration debug and extreme cases where no
// error recovery is possible
// 
// Note that this should be called like - SYSLOG_ASSERT(0, strerror(errno))
// in callers program context so that the correct error string is printed.
//
// WARNING: Calls to perror may modify errno and result in misleading error
// strings being printed by this macro. There's really no reason to call
// perror in addition since the macro prints out the equivalent.
//
#ifdef NDEBUG
#define SYSLOG_ASSERT(tst, errstr)
#else
#define SYSLOG_ASSERT(tst, errstr) if(!tst){ syslog(LOG_CRIT, "ASSERTION for PID=%d, @ %s in %s:%d, error=%s - PROGRAM ABORT",          \
                                                    getpid(), __TIME__, __FILE__, __LINE__, errstr);                                    \
                                             fprintf(stdout, "\nASSERTION for PID=%d, @ %s in %s:%d, error=%s - PROGRAM ABORT\n",       \
                                                     getpid(), __TIME__, __FILE__, __LINE__, errstr);                                   \
                                             fprintf(stderr, "\nASSERTION for PID=%d, @ %s in %s:%d, error=%s - PROGRAM ABORT\n",       \
                                                     getpid(), __TIME__, __FILE__, __LINE__, errstr); abort(); };
#endif


// period is in milliseconds and packets are assumed to be standard
// MPEG2 transport stream packets
#define BITRATE_OF(numPackets, period) \
	(unsigned int)(((double)(numPackets*MPEG2_PACKET_SIZE*BITS_PER_BYTE))/ \
        (((double)period)/((double)MILLISECS_IN_SECS)))


// Macro to get the adaptation field indicator from a TS packet
#define GET_MPEG2TS_ADAPT(m) ((m[3] >> 4) & 0x3)
#define GET_MPEG2TS_ADAPTLENGTH(m) (m[4])

// Macro to get the adaptation field from a TS packet
#define GET_MPEG2TS_ADAPTFIELD(m) (m[5])
#define GET_MPEG2TS_PID(m) (((m[1] & 0x1f) << 8) | (m[2] & 0xff))

// Macro to get a PCR from a TS packet that is know to contain a PCR.
#define GET_MPEG2TS_PCR(m, p)    \
do {                                                    \
        uint32_t        iTmp;                           \
        p = (int64_t )((m[6]<<24)+(m[7]<<16)+(m[8]<<8)+m[9]);   \
        p <<= 1;                                        \
        if (m[10] & 0x80) p |= (int64_t )1;     \
        p *= (int64_t )300;                     \
        iTmp = ((m[10] & 1) << 8) + m[11]; \
        p += (int64_t )iTmp;            \
} while(0);

typedef struct iFramePtr
{
    // Offset and length to use for trick play
    unsigned long long offset;
    unsigned int length;

    // Sync info
    unsigned long long syncOffset;

    // Seq pointers and lengths
    unsigned long long seqOffset;
    unsigned int seqLength;

    // GOP pointers and lengths
    unsigned long long gopOffset;
    unsigned int gopLength;

    // I-frame pointers and lengths
    unsigned long long iframeOffset;
    unsigned int iframeLength;

    // Trick play offset and length
    unsigned long long trickOffset;
    unsigned int trickLength;

} iFramePointer;


enum streamTypes {basicLoop=0, vodPlay=1, basicLoopSecondary=2, playOnce=3};

typedef struct qamCtxt
{
    unsigned short devID;
    unsigned short port;

    enum qamModes mode;
    enum annexTypes annex;

    // e.g. 177000000 = 177 MhZ, which is channel 7 for QAM-256 Annex-B at 6 MhZ spacing
    unsigned int channel;

    char devStr[128];

} qamContext;

typedef struct tripCtxt  // QAM mode, frequency, and program number - IDs a program for a tuner
{
    enum qamModes mode;
    enum annexTypes annex;
    unsigned int center_channel_freq; // e.g. 177000000 (177 MhZ) is channel 7 in 6 MhZ spaced QAM-256 mode
    unsigned short programNum;

    // port to which this triplet has been mapped or port being requested for it
    unsigned short port;

    // As defined and parsed from PAT, this is the TS carrying this program, purely informational for debug
    unsigned short transport_stream_id;

} tripletContext;


typedef struct pbCtxt
{
    // baseTOD is time that playback was started
    double baseTOD;

    // Internal playback resource ID
    unsigned short streamID;
    unsigned short devID;

    // The bitrate should be the maximum for VBR transport streams that is set
    // when the stream is configured with the QAM and the stream will either be
    // NULL padded to match this rate by the QAM or prior to this in the original
    // source or by the SW-mux, so in essence, this is the CBR for this stream when
    // NULL pad is included.
    // 
    // This bitrate is used when stream resources are set up with the QAM transport driver.
    // 
    unsigned int transportBitRate;
    
    // Triplet specification of resources - includes RF port, QAM configuration, and Program info for SPTS/MPTS
    unsigned short port;
    int qamConfigIdx;
    unsigned short tsID;

    // Replace above with this triplet context
    tripletContext tripletInfo;

    int numPrograms;
    unsigned short program_number[MAX_PROGRAMS];
    int numPIDS[MAX_PROGRAMS];

    // Original stream parameters - used for trick play operations
    unsigned short matchCnt;
    unsigned short uniquePIDList[MAX_PIDS];

    // Parsed from the transport stream
    unsigned short patPID;
    unsigned short psipPID;
    unsigned short catPID;
    unsigned short tsdtPID;
    unsigned short nullPID;
    unsigned short unknownPID;

    // PIDs per program in MPTS 
    unsigned short pmtPID[MAX_PROGRAMS];
    unsigned short videoPID[MAX_PROGRAMS];
    unsigned short audioPID[MAX_PROGRAMS];
    unsigned short secAudioPID[MAX_PROGRAMS];
    unsigned short pcrPID[MAX_PROGRAMS];

    // Continuity counters for RT PID packet insertion
    unsigned char psipCC;

    // Version for PMT updates
    unsigned char pmtVersion;

    // Consider changing to unsigned short to save space since there can really
    // only be 2^13 PIDs in any given transport stream.
    // 
    unsigned int numPAT;     // # of PIDS
    unsigned int numPMT;     // # of PIDS
    unsigned int numVideo;   // # of PIDS
    unsigned int numAudio;   // # of PIDS
    unsigned int numNull;    // # of PIDS
    unsigned int numUnknown; // # of PIDS

    unsigned int numIframe;
    unsigned int numGOP;
    unsigned int numSeq;
    unsigned int numPES;
    unsigned int numPIC;
    unsigned int numTSPackets;
    unsigned int numBytes;
    iFramePointer iframePtrs[MAX_IFRAMES];

    enum streamTypes streamType; 

    // If this is set true, then stream is assumed to be pre-muxed and pre-timed.
    // This should be set for MPTOOTs MPTS bit-streams
    int RAW_STREAM;

    // Overall context
    int pbWorkerIdx;
    FILE *fp;
#ifdef LARGE_FILE_SUPPORT
    int fd; // this is used to open 2GB or larger files when large file support is defined
#endif
    FILE *fpTrick;
    fpos_t pos; // position in file for current active stream
    char streamName[MAX_PATH_NAME];
    char streamTrickFileName[MAX_PATH_NAME];
    int qamDev;
    unsigned int streamState;
    unsigned long long bytesSent;

    // Playback SM
    //
    // Consider converting these to uint8_t or more compact boolean type to 
    // save space.
    // 
    int secondaryStream;
    int startVOD;
    int newState;
    int playStream;
    int loopEnabled;
    int pauseActive;
    int restartStream;
    int terminateStream;
    int ffStream;
    int rewStream;

} playbackContext;

#endif
