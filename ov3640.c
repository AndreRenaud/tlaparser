/*
 * ov3640.c
 * Ryan Mallon (May, 2009)
 *
 * TLA parser for ov3640 image sensor
 *
 * Determine how many pixels per href are being sent, and output the jpeg data
 * to a file called dump.jpg. The gpio signal is used as a trigger point for
 * the frame to be captured.
 *
 */

#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <asm/byteorder.h>

#include "dumpdata.h"
#include "common.h"

#define NR_DATA_BITS	8

//static int data_pin_mapping[NR_DATA_BITS] = {5, 4, 6, 7, 11, 10, 9, 8, 3, 2};

struct ov3640_info {
    channel_info	*vref;
    channel_info	*href;
    channel_info	*pclk;
    channel_info	*gpio;
    channel_info	*data[NR_DATA_BITS];

    int			href_count;
    int			href_block_count;
    int			total_pixels;

    FILE		*fd;
};

static struct ov3640_info info;

static void write_pixel_data(capture *c)
{
    int i;
    unsigned char pixel = 0;

    for (i = 0; i < NR_DATA_BITS; i++)
	if (capture_bit(c, info.data[NR_DATA_BITS - 1 - i]))
	    pixel |= (1 << i);

    //time_log(c, "pixel: 0x%x\n", pixel);
    fwrite(&pixel, 1, 1, info.fd);
}

static void parse_capture(capture *c, capture *prev, list_t *channels)
{
    static uint64_t prev_time;
    static int frame_started = 0;

    //time_log(c, "capture: %p\n", c);
    if (!prev)
	return;

    /* If we're inside our gpio tagged area.
     * We might have started half-way through the previous frame, so
     * wait for a vref transition first
     */
    if (capture_bit(c, info.gpio)) {
        /* When vref transitions, we start/end a frame */
	if (capture_bit_transition(c, prev, info.vref,
				   TRANSITION_low_to_high)) {
            if (info.total_pixels)
                time_log(c, "Skipping secondary jpeg\n");
            else {
                frame_started = 1;
                info.href_count = 0;
            }
        }
	if (capture_bit_transition(c, prev, info.vref,
				   TRANSITION_high_to_low)) {
            frame_started = 0;
        }

	if (capture_bit_transition(c, prev, info.href,
				   TRANSITION_low_to_high)) {
	    /* Start of jpeg data block */
            time_log(c, "Start of jpeg block\n");
	    info.href_block_count = 0;
	    prev_time = capture_time(c);
	}

	if (capture_bit_transition(c, prev, info.href,
				   TRANSITION_high_to_low)) {
	    /* End of jpeg data block */
	    time_log(c, "href %d has %d pixels, time since last: %lld\n",
                    info.href_count, info.href_block_count,
		   capture_time(c) - prev_time);
	    info.href_count++;

	}

        /* If we're inside a frame, href is high & pclk transitions
           then that is jpeg data, so squirrel it away
           */
	if (frame_started &&
            (capture_bit(c, info.href) == 1) &&
	    capture_bit_transition(c, prev, info.pclk,
				   TRANSITION_low_to_high)) {
	    write_pixel_data(c);
	    info.href_block_count++;
	    info.total_pixels++;
	}
    }
}

void parse_ov3640(bulk_capture *b, char *filename, list_t *channels)
{
    char name[10];
    capture *c, *prev = NULL;
    int i;

    info.vref = capture_channel_details("vref", channels);
    info.href = capture_channel_details("href", channels);
    info.pclk = capture_channel_details("pclk", channels);
    info.gpio = capture_channel_details("frame", channels);
    assert(info.vref  && info.href && info.pclk && info.gpio);
    for (i = 0; i < NR_DATA_BITS; i++) {
	snprintf(name, sizeof(name), "d%d", i);
	info.data[i] = capture_channel_details(name, channels);
        assert(info.data[i]);
    }

    info.href_count = 0;
    info.href_block_count = 0;

    info.fd = fopen("test.jpg", "wb");
    if (!info.fd) {
	fprintf(stderr, "Cannot open dump.jpg for writing\n");
	return;
    }

    c = b->data;
    for (i = 0; i < b->length / sizeof(capture); i++, c++) {
	parse_capture(c, prev, channels);
	prev = c;
    }

    printf("%d total pixels\n", info.total_pixels);
    fclose(info.fd);
}
