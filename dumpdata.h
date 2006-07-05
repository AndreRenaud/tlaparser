#ifndef DUMPDATA_H
#define DUMPDATA_H

#include <stdint.h>

#include "lists.h"

#define MAX_DATA_TRANSFER (8 * 1024)

enum // indices into capture->data[...], to the 8 data bits
{
    PROBE_clk1 = 17,
    PROBE_clk2 = 16,

    PROBE_c0 = 15,
    PROBE_c1 = 14,
    PROBE_c2 = 13,
    PROBE_c3 = 12,

    PROBE_d0 = 11,
    PROBE_d1 = 10,
    PROBE_d2 = 9,
    PROBE_d3 = 8,

    PROBE_a0 = 7,
    PROBE_a1 = 6,
    PROBE_a2 = 5,
    PROBE_a3 = 4,

    PROBE_e0 = 3,
    PROBE_e1 = 2,
    PROBE_e2 = 1,
    PROBE_e3 = 0,
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
} bulk_capture;

typedef struct
{
    char probe_name[20];
    char name[20];
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

// returns 0, or 1 depending on whether the bit from the capture corresponding to 
// 'channel_name' (from the channels list) was set
int capture_bit (capture *c, char *channel_name, list_t *channels);
// retrieves 1 bit from the capture, from probe 'probe' ( see the PROBE_ structure) index 'index'
int capture_bit_raw (capture *c, int probe, int index);

bulk_capture *build_dump (unsigned char *data, int length);
channel_info *build_channel (char *probe, char *name);

int capture_compare (list_t *data1, list_t *data2, char *file1, char *file2);

stream_info_t *build_stream_info (list_t *data, char *filename);
void compare_streams (stream_info_t *s1, stream_info_t *s2);


extern list_t *datasets;
extern list_t *channels;

#endif

