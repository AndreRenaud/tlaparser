#ifndef DUMPDATA_H
#define DUMPDATA_H

#include <stdint.h>

#include "lists.h"

#define MAX_DATA_TRANSFER (8 * 1024)
#define MAX_DATA_LEN (100 * 1024 * 1024)

enum
{
    TRANSITION_low_to_high,
    TRANSITION_high_to_low,

    TRANSITION_falling_edge = TRANSITION_high_to_low,
    TRANSITION_rising_edge = TRANSITION_low_to_high,
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
channel_info *capture_channel_details (capture *cap, char *channel_name, list_t *channels);
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


/* Returns the time of the sample in nano seconds */
uint64_t capture_time (capture *c);

bulk_capture *build_dump (void *data, int length);
channel_info *build_channel (char *probe, char *name, int inverted);

int time_log (capture *c, char *msg, ...);

void display_data_buffer (unsigned char *buffer, int len, int ebcdic);

extern capture *first_capture;

bulk_capture *tla_parse_file (char *filename);

#endif

