#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

struct spi_format 
{
    unsigned int frame_count:9;
    unsigned int seconds:4;
    unsigned int frame_inc:3;
    unsigned int flags:8;
    unsigned int dummy:8;
};

struct pin_assignments
{
    int init;
    struct {
        channel_info *pps;
        channel_info *frm;
        channel_info *clk;
        channel_info *miso;
        uint8_t data[64];
        int data_bits;
        int last_inc;
        int frame_count;
    } camera[2];
};

static void parse_oreo_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    int i;

    if (pa.init == -1) {
        char name[20];

        pa.init = 1;

        for (i = 0; i < 2; i++) {
            sprintf(name, "c%dpps", i);
            pa.camera[i].pps = capture_channel_details(name, channels);
            sprintf(name, "c%dmiso", i);
            pa.camera[i].miso = capture_channel_details(name, channels);
            sprintf(name, "c%dfrm", i);
            pa.camera[i].frm = capture_channel_details(name, channels);
            sprintf(name, "c%dclk", i);
            pa.camera[i].clk = capture_channel_details(name, channels);

            pa.camera[i].data_bits = 0;
            pa.camera[i].last_inc = -1;
            pa.camera[i].frame_count = -1;
        }
    }

    if (!prev)
        return;

    for (i = 0; i < 2; i++) {
        if (capture_bit_transition(c, prev, pa.camera[i].pps, 
                    TRANSITION_low_to_high))
            time_log(c, "PPS %d transition\n", i);

        if (capture_bit_transition(c, prev, pa.camera[i].frm,
            TRANSITION_high_to_low)) {
            pa.camera[i].data_bits = 0;
            memset(pa.camera[i].data, 0, sizeof(pa.camera[i].data));
        }

        if (!capture_bit(c, pa.camera[i].frm)) {
            if (capture_bit_transition(c, prev, pa.camera[i].clk,
                    TRANSITION_low_to_high)) {
                int len = pa.camera[i].data_bits;
                int dat = capture_bit(c, pa.camera[i].miso);
                if (dat)
                    pa.camera[i].data[len / 8] |= 1 << (7 - (len % 8));
                pa.camera[i].data_bits++;
            }
        } else if (pa.camera[i].data_bits) {
            struct spi_format *fmt = (void *)pa.camera[i].data;
            int other_frame_count;
            if (pa.camera[i].data_bits != 32)
                time_log(c, "Failed to get enough camera bytes\n");
            //time_log(c, "Camera %d spi transfer (%d bits)\n", 
                    //i, pa.camera[i].data_bits);
            //display_data_buffer(pa.camera[i].data, 
                    //(pa.camera[i].data_bits + 7) / 8, 0);
            pa.camera[i].data_bits = 0;
            printf("Cam %d Count: %3.3d  Inc: %3.3d  Seconds: %3.3d  Flags: 0x%2.2x\n",
                i, fmt->frame_count, fmt->frame_inc, fmt->seconds, fmt->flags);
            if (pa.camera[i].last_inc != -1) {
                if ((pa.camera[i].last_inc + 1) % 8 != fmt->frame_inc)
                    time_log(c, "Cam %d incorrect frame inc\n",
                        fmt->frame_inc);
            }
            pa.camera[i].last_inc = fmt->frame_inc;
            pa.camera[i].frame_count = fmt->frame_count;
            other_frame_count = pa.camera[(i + 1) % 2].frame_count;
            if (other_frame_count != fmt->frame_count &&
                other_frame_count != fmt->frame_count + 1 &&
                other_frame_count != fmt->frame_count - 1)
                time_log(c, "Frame count mismatch (%d): %d vs %d\n",
                    i, other_frame_count, fmt->frame_count);

        }
    }

}

void parse_oreo_fpga (bulk_capture *b, char *filename, list_t *channels)
{

    int i;
    capture *c, *prev = NULL;

    printf ("Oreo FPGA analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
        parse_oreo_cap (c, prev, channels);
        prev = c;
        c++;
    }
}

