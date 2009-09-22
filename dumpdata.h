#ifndef DUMPDATA_H
#define DUMPDATA_H

#include <stdint.h>

#include "lists.h"

enum
{
    TRANSITION_none,
    TRANSITION_low_to_high = 1,
    TRANSITION_high_to_low = -1,

    TRANSITION_falling_edge = TRANSITION_high_to_low,
    TRANSITION_rising_edge = TRANSITION_low_to_high,
};

enum {
    DISP_FLAG_ebcdic = 1 << 0,
    DISP_FLAG_ascii = 1 << 1,
    DISP_FLAG_none = 1 << 2,
    DISP_FLAG_default = 0,
    DISP_FLAG_both = DISP_FLAG_ebcdic | DISP_FLAG_ascii,
};


//#define CAPTURE_DATA_BYTES 14 // for the TLA 714
#define CAPTURE_DATA_BYTES 18 // for the TLA 5204

struct capture
{
   uint8_t data[CAPTURE_DATA_BYTES];
   uint32_t time_top;
   uint32_t time_bottom;
} __attribute__((packed));

typedef struct capture capture;

typedef struct
{
   int length;
   capture *data;
} bulk_capture;

typedef struct
{
    char probe_name[20];
    char name[20];
    int probe;
    int index;
    int inverted;
} channel_info;

void dump_capture (bulk_capture *cap, char *name, list_t *channels);
void dump_channel_list (list_t *channels);
void dump_changing_channels (bulk_capture *cap, char *name, list_t *channels);

/* Returns the channel with name 'channel_name'
 * Useful to speed things up so we don't continually do the same searches
 */
channel_info *capture_channel_details (char *channel_name, list_t *channels);
// returns 0, or 1 depending on whether the bit from the capture corresponding to
// 'channel_name' (from the channels list) was set
int capture_bit_name (capture *cap, char *channel_name, list_t *channels);
// retrieves 1 bit from the capture, from probe 'probe' ( see the PROBE_ structure) index 'index'
int capture_bit (capture *cap, channel_info *c);

// retrieves len bits from the capture, using the array of probes
unsigned int capture_data(capture *cap, channel_info *c[], int len);

// returns true/false if a given bit performs a transition between two captures (dir is TRANSITION_...)
int capture_bit_transition (capture *cur, capture *prev, channel_info *chan, int dir);
// same as above, but specify the channel by name
int capture_bit_transition_name (capture *cur, capture *prev, char *name, list_t *channels, int dir);
// returns direction of transition between two captures (TRANSITION_...)
int capture_bit_change (capture *cur, capture *prev, channel_info *chan);


/* Returns the time of the sample in micro seconds */
uint64_t capture_time (capture *c);

bulk_capture *build_dump (void *data, int length);
channel_info *build_channel (char *probe, char *name, int inverted);

int time_log (capture *c, char *msg, ...);

void display_data_buffer (unsigned char *buffer, int len, int flags);
void display_dual_data_buffer (unsigned char *buff1, int len1, unsigned char *buff2, int len2);

extern capture *first_capture;

bulk_capture *tla_parse_file (char *filename);

#endif

