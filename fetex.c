#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

struct pin_assignments
{
    int init;
    channel_info *rwgtn;
    channel_info *read[9];
    channel_info *write[9];
};

static int get_read_data(capture *c, struct pin_assignments *pa)
{
    int retval = 0;
    int i;
    for (i = 0; i < 9; i++)
        if (capture_bit(c, pa->read[i]))
            retval |= 1 << i;
    return retval;
}

static int get_write_data(capture *c, struct pin_assignments *pa)
{
    int retval = 0;
    int i;
    for (i = 0; i < 9; i++)
        if (capture_bit(c, pa->write[i]))
            retval |= 1 << i;
    return retval;
}

static void parse_fetex_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static unsigned char in_data[16384];
    static unsigned char out_data[16384];
    static int in_data_count = 0;
    static int out_data_count = 0;

    if (pa.init == -1) {
        char name[20];
        int i;

        pa.init = 1;

        pa.rwgtn = capture_channel_details("rwgtn", channels);
        for (i = 0; i < 9; i++) {
            sprintf(name, "RDN%d", i);
            pa.read[i] = capture_channel_details(name, channels);
            sprintf(name, "WDN%d", i);
            pa.write[i] = capture_channel_details(name, channels);
        }
    }

    if (!prev)
        return;

    if (get_read_data(prev, &pa) != get_read_data(c, &pa))
        out_data[out_data_count++] = get_read_data(c, &pa);
    if (get_write_data(prev, &pa) != get_write_data(c, &pa))
        in_data[in_data_count++] = get_write_data(c, &pa);

    if (capture_bit(c, pa.rwgtn)) {
        if (in_data_count) {
            time_log(c, "Input data\n");
            display_data_buffer (in_data, in_data_count, 0);
            in_data_count = 0;
        }
        if (out_data_count) {
            time_log(c, "Output data\n");
            display_data_buffer (out_data, out_data_count, 0);
            out_data_count = 0;
        }
    }
}

void parse_fetex (bulk_capture *b, char *filename, list_t *channels)
{

    int i;
    capture *c, *prev = NULL;

    printf ("Fetex analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
        parse_fetex_cap (c, prev, channels);
        prev = c;
        c++;
    }
}

