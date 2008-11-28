#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

static int nor_addr (capture *c, list_t *channels)
{
    char name[20];
    int bit;
    int retval = 0;
    int i;

    for (i = 0; i < 16; i++) {
        sprintf (name, "ma%d", i);
        bit = capture_bit_name (c, name, channels) ? 1 : 0;
        retval |= bit << i;
    }

    return retval;
}

static int nor_data (capture *c, list_t *channels)
{
    char name[20];
    int bit;
    int retval = 0;
    int i;

    for (i = 0; i < 16; i++) {
        sprintf (name, "md%d", i);
        bit = capture_bit_name (c, name, channels) ? 1 : 0;
        retval |= bit << i;
    }

    return retval;
}

struct pin_assignments
{
    int init;
    channel_info *cs;
    channel_info *read;
    channel_info *write;
};

static void parse_nor_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};

    if (pa.init == -1) {
        pa.init = 1;
        pa.cs = capture_channel_details (c, "cs-nor-n-1", channels);
        pa.read = capture_channel_details (c, "nrd", channels);
        pa.write = capture_channel_details (c, "nwr", channels);
    }

    if (!prev)
        return;

    if (capture_bit (c, pa.cs) == 0) {
        if (capture_bit_transition (c, prev, pa.read, TRANSITION_low_to_high)) {
            unsigned int data = nor_data (c, channels);
            time_log (c, "Read: 0x%x (0x%x) Addr=0x%x\n", 
                    data, data ^ 0xffff, nor_addr (c, channels));
        }
        if (capture_bit_transition (c, prev, pa.write, TRANSITION_low_to_high)) {
            unsigned int data = nor_data (c, channels);
            time_log (c, "Write: 0x%x (0x%x) Addr=0x%x\n", 
                    data, data ^ 0xffff, nor_addr (c, channels));
        }
    }

       
}

void parse_nor (bulk_capture *b, char *filename, list_t *channels)
{

    int i;
    capture *c, *prev = NULL;

    printf ("nor analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
        parse_nor_cap (c, prev, channels);
        prev = c;
        c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));

}

