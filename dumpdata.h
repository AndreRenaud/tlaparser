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


#define CAPTURE_DATA_BYTES 18

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

typedef struct
{
   list_t *data;
   char filename[100];
   bulk_capture *current;

   capture *current_cap;

   uint32_t prev;
   uint32_t val;
   uint32_t diff;
   uint32_t byte;

   uint32_t type;

   uint32_t command_count;
   uint32_t data_count;

   uint32_t finished;

   uint8_t buffer[MAX_DATA_TRANSFER];
   uint32_t buff_size;

} stream_info_t;

int dump_capture (bulk_capture *c);
void dump_capture_list (list_t *capture, char *name, list_t *channels);
void dump_channel_list (list_t *channels);

/* Returns the channel with name 'channel_name'
 * Useful to speed things up so we don't continually do the same searches
 */
channel_info *capture_channel_details (capture *cap, char *channel_name, list_t *channels);
// returns 0, or 1 depending on whether the bit from the capture corresponding to 
// 'channel_name' (from the channels list) was set
int capture_bit_name (capture *cap, char *channel_name, list_t *channels);
// retrieves 1 bit from the capture, from probe 'probe' ( see the PROBE_ structure) index 'index'
int capture_bit (capture *cap, channel_info *c);

// returns true/false if a given bit performs a transition between two captures (dir is TRANSITION_...)
int capture_bit_transition (capture *cur, capture *prev, channel_info *chan, int dir);
// same as above, but specify the channel by name
int capture_bit_transition_name (capture *cur, capture *prev, char *name, list_t *channels, int dir);


/* Returns the time of the sample in nano seconds */
uint64_t capture_time (capture *c);

bulk_capture *build_dump (unsigned char *data, int length);
channel_info *build_channel (char *probe, char *name, int inverted);

int capture_compare (list_t *data1, list_t *data2, char *file1, char *file2);

stream_info_t *build_stream_info (list_t *data, char *filename);
void compare_streams (stream_info_t *s1, stream_info_t *s2);

int time_log (capture *c, char *msg, ...);

#endif

