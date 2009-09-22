/*
 * ssc_audio.c
 * Ryan Mallon (Dec, 2008)
 *
 * TLA parser for SSC audio on the AT91 processor.
 *
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <asm/byteorder.h>

#include "dumpdata.h"
#include "common.h"

#define BITS_PER_SAMPLE		16

struct ssc_pins {
    channel_info	*sclk;
    channel_info	*frame;
    channel_info	*din;
    channel_info	*dout;
};

struct sample {
    int			active;
    int			din_bit;
    int			dout_bit;
    unsigned short	din;
    unsigned short	dout;
};

static struct ssc_pins ssc;
static struct sample sample;

static void ssc_clear_sample(void)
{
    memset(&sample, 0, sizeof(struct sample));
}

static unsigned short bit_reverse(unsigned short v)
{
    unsigned short r = v;
    int s = sizeof(v) * CHAR_BIT - 1;
	
    for (v >>= 1; v; v >>= 1) {
	r <<= 1;
	r |= v & 1;
	s--;
    }

    r <<= s;
    return r;
}

int count = 0;
static void parse_ssc_capture(capture *c, capture *prev, list_t *channels)
{
    if (!prev)
	return;
 
    if (capture_bit_transition(c, prev, ssc.frame, TRANSITION_low_to_high)) 
	sample.active	= 1;

    if (sample.active) {
	if (capture_bit_transition(c, prev, ssc.sclk, 
				   TRANSITION_high_to_low) &&
	    sample.din_bit < BITS_PER_SAMPLE) {
	    int din_bit;
	    
	    din_bit = capture_bit(c, ssc.din);
	    sample.din |= (din_bit << sample.din_bit);
	    sample.din_bit++;
	}
	
	if (capture_bit_transition(c, prev, ssc.sclk,
				   TRANSITION_low_to_high) &&
	    sample.dout_bit < BITS_PER_SAMPLE) {
	    int dout_bit;
	
	    dout_bit = capture_bit(c, ssc.dout);	
	    sample.dout |= (dout_bit << sample.dout_bit);
	    
	    sample.dout_bit++;
	}
	    
	if (sample.dout_bit == BITS_PER_SAMPLE && 
	    sample.din_bit == BITS_PER_SAMPLE) {
	    printf("%d, %d\n", count++,
		   (short)bit_reverse(sample.dout));
	    ssc_clear_sample();
	}
    }
}

void parse_ssc_audio(bulk_capture *b, char *filename, list_t *channels)
{
    capture *c, *prev;
    int i;

    ssc.sclk	= capture_channel_details("bclk", channels);
    ssc.frame	= capture_channel_details("lrc", channels);
    ssc.din	= capture_channel_details("din", channels);
    ssc.dout	= capture_channel_details("dout", channels);

    ssc_clear_sample();
    
    c = b->data;
    for (i = 0; i < b->length / sizeof(capture); i++, c++) {
	parse_ssc_capture(c, prev, channels);
	prev = c;
    }
    
    //printf ("Parsed %d captures\n", b->length / sizeof (capture));
}
