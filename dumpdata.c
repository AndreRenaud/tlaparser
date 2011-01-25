#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#define __USE_GNU
#define _GNU_SOURCE
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>

#include "dumpdata.h"
#include "common.h"

#define bit(d,b) ((d & (1 << b)) ? 1 : 0)
#define falling_edge(b) (bit(diff, b) && !bit(val, b))
#define rising_edge(b) (bit(diff, b) && bit(val, b))

static int dump_single_capture (capture *c, capture *prev_cap)
{
    int i;

    if (prev_cap && capture_time (c) < capture_time (prev_cap))
	printf ("ARGH WE WENT BACKWARDS\n");

    printf ("%llx: ", capture_time (c));
    for (i = 0; i < CAPTURE_DATA_BYTES; i++)
    {
         printf ("%2.2x ", c->data[i]);
    }
    printf ("\n");

    return 0;
}

uint64_t capture_time (capture *c)
{
    uint64_t t, b;
    t = ntohl (c->time_top) & 0xffff; // not sure what the story is here, packed data isn't all time stamp
    //t = 0;
    b = ntohl (c->time_bottom);

    // timings are in 1/8th of a micro-second, so change it into micro seconds
    return (t << 32 | b) / 8;
}

void dump_channel_list (list_t *channels)
{
    list_t *n;
    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	printf ("%s->%s Probe %d, index %d %s\n", c->probe_name, c->name, c->probe, c->index, c->inverted ? "Inverted" : "");
    }
}

void dump_capture (bulk_capture *b, char *name, list_t *channels)
{
   int i;
   capture *c, *prev = NULL;

   printf ("Capture '%s'\n", name);
   dump_channel_list (channels);

   c = b->data;

   //printf ("Capture Group: %p\n", b);
   for (i = 0; i < b->length / sizeof (capture); i++)
   {
      dump_single_capture (c, prev);
      prev = c;
      c++;
   }
}

static const char *index_to_name (int index)
{
#if CAPTURE_DATA_BYTES == 18
    const char *probe_index[] = {"E0", "A3", "A2", "D3", /* 0 - 3 */
				 "E3", "E2", "E1", "D2", /* 4 - 7 */
				 "A1", "A0", "D1", "D0", /* 8 - 11 */
				 "C3", "C2", "C1", "C0", /* 12 - 15 */
    };
#elif CAPTURE_DATA_BYTES == 14
    const char *probe_index[] = {"D2", "A1", "A0", "D3", /* 0 - 3 */
				 "A2", "A3", "D1", "D0", /* 4 - 7 */
				 "C3", "C2", "C1", "C0", /* 8 - 11 */
    };
#else
#error "Don't know the probe layout for this analyser"
#endif

    if (index < 0 || index >= (sizeof (probe_index) / sizeof (probe_index[0])))
        return NULL;

    return probe_index[index];
}

void dump_changing_channels (bulk_capture *bc, char *name, list_t *channels)
{
    uint8_t changes[CAPTURE_DATA_BYTES];
    int b;
    int i;
    capture *c, *prev = NULL;

    for (b = 0; b < CAPTURE_DATA_BYTES; b++)
	changes[b] = 0;

    c = bc->data;

    //printf ("Capture Group: %p\n", b);
    for (i = 0; i < bc->length / sizeof (capture); i++)
    {
        if (prev)
        {
            for (b = 0; b < CAPTURE_DATA_BYTES; b++)
            {
                changes[b] |= c->data[b] ^ prev->data[b];
            }
        }

        prev = c;
        c++;
    }

    printf ("Changed bits on '%s'\n", name);
    for (b = 0; b < CAPTURE_DATA_BYTES; b++)
        printf (" %s  ", index_to_name (b) ?  : "UK");
    printf ("\n");
    for (b = 0; b < CAPTURE_DATA_BYTES; b++)
	printf ("0x%2.2x ", changes[b]);
    printf ("\n");

}

int capture_bit (capture *cap, channel_info *c)
{
    int retval;
    if (!c || c->probe < 0 || c->probe >= CAPTURE_DATA_BYTES || c->index < 0 || c->index >= 8)
    {
	printf ("Invalid probe: %d index: %d\n",
		c ? c->probe : -1 , c ? c->index : -1);
	abort ();
    }
    retval = (cap->data[c->probe] & (1 << c->index));
    if (c->inverted)
	retval = !retval;
    return retval ? 1 : 0;
}

unsigned int capture_data(capture *cap, channel_info *c[], int len)
{
    unsigned data = 0x0, i;

    for(i = 0; i < len; i++) {
	int bit = capture_bit(cap, c[i]);

	if(bit)
	    data |= (1 << i);
    }

    return data;
}

static void simplify_probe_name (const char *probe_name, char *result)
{
    const char *p;
    char *r;

    for (p = probe_name, r = result; *p; p++)
    {
	if (strchr ("<>_ .[]()", *p)) /* Which characters in the name do we ignore */
	    continue;
	*r = *p;
	r++;
    }
    *r = '\0';
}

/* Try and convert the name we're using, with the name that was entered into the scope
 * Be a bit generous, look for substrings, ignore case etc...
 * Should possibly try and drop characters like '<', '>', '_'
 */
channel_info *capture_channel_details (char *channel_name_full, list_t *channels)
{
    list_t *n;
    char channel_name[100];
    char probe_name[100];
    channel_info *retval = NULL;
    int len;

    //strcpy(channel_name, channel_name_full);
    simplify_probe_name (channel_name_full, channel_name);

    /* Do they exactly match */
    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	simplify_probe_name (c->name, probe_name);
	//printf ("Comparing '%s' to '%s'\n", probe_name, channel_name);

	if (strcasecmp (probe_name, channel_name) == 0) {
            retval = c;
            goto out;
        }
    }

    /* Is it a suffix? */
    len = strlen(channel_name);
    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
        int this_len;

	simplify_probe_name (c->name, probe_name);

        this_len = strlen(probe_name);
        if (this_len < len)
            continue;
	//printf ("Suffix Comparing '%s' to '%s'\n", probe_name, channel_name);
        if (strcasecmp(&probe_name[this_len - len], channel_name) == 0) {
            retval = c;
            goto out;
        }
    }


    /* Do they partially match */
    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	simplify_probe_name (c->name, probe_name);
	//printf ("Comparing '%s' to '%s'\n", probe_name, channel_name);

	if (strcasestr (probe_name, channel_name)) {
            retval = c;
            goto out;
        }
	if (strcasestr (channel_name, probe_name)) {
            retval = c;
            goto out;
        }
    }

    printf ("Unknown channel: %s\n", channel_name);
    return NULL;
out:
#if 0
    if (strcasecmp(retval->name, channel_name_full) != 0)
        printf("Using name '%s' in place of '%s\n",
            retval->name, channel_name_full);
#endif
    return retval;

}


int capture_bit_name (capture *cap, char *channel_name, list_t *channels)
{
    channel_info *chan;

    chan = capture_channel_details (channel_name, channels);
    if (chan)
	return capture_bit (cap, chan);
    return -1;
}


int capture_bit_change (capture *cur, capture *prev, channel_info *chan)
{
    int n, p;

    if (!chan) {
        fprintf(stderr, "No channels\n");
        return -1;
    }
    p = capture_bit (prev, chan);
    n = capture_bit (cur, chan);
    if (n && !p)
        return TRANSITION_low_to_high;
    if (!n && p)
        return TRANSITION_high_to_low;
    return TRANSITION_none;
}


int capture_bit_transition (capture *cur, capture *prev, channel_info *chan, int want_dir)
{
    int dir = capture_bit_change (cur, prev, chan);

    return dir == want_dir;
#if 0
    int n, p;
    if (!chan)
	return -1;
    p = capture_bit (prev, chan);
    if (p < 0)
	return -1;
    n = capture_bit (cur, chan);
    if (n < 0)
	return -1;

    if (n && !p && dir == TRANSITION_low_to_high)
	return 1;
    if (!n && p && dir == TRANSITION_high_to_low)
	return 1;
#endif
    return 0;
}

int capture_bit_transition_name (capture *cur, capture *prev, char *name, list_t *channels, int dir)
{
    channel_info *chan;
    chan = capture_channel_details (name, channels);

    return capture_bit_transition (cur, prev, chan, dir);
}

static int name_to_index (const char *probe_name)
{
#warning "Using vaguelly guessed table to convert names to probe indicies"
    int i;

#define NPROBES (CAPTURE_DATA_BYTES - 2) // 2 of them are clocks, which we don't care about

    for (i = 0; i < NPROBES; i++)
    {
        const char *probe = index_to_name (i);
	if (probe && strncmp (probe, probe_name, 2) == 0)
	    return i;
    }
    fprintf (stderr, "Could not determine index for probe '%s'\n", probe_name);
    return 0;
}

/* Works out where in the binary blob the channel info is found
 * Channels are ordered as specified in the file (So the order we call build_channel matters)
 * Probes are also paired, so e3 & e2 are always together, so are a1 & a0. If a1 is entirely unused,
 * it will not appear in the tla file, but we must still account for it (that is the bit below looking
 * for 3 & 1, checking that we haven't skipped a probe
 * This is all vaguely confusing
 */
channel_info *build_channel (char *probe_name, char *name, int inverted)
{
    channel_info *retval;
    // they seem to have leading & trailing $ signs on them, so chop them off if they're there
    retval = malloc (sizeof (channel_info));

    strncpy (retval->probe_name, probe_name, 20);
    retval->probe_name[19] = '\0';

    retval->inverted = inverted;

    if (name[0] == '$' && name[strlen(name) - 1] == '$')
    {
	strncpy (retval->name, &name[1], strlen(name) - 2);
	retval->name[strlen(name) - 2] = '\0';
    }
    else
	strncpy (retval->name, name, 20);
    retval->name[19] = '\0';

    retval->index = atoi (&probe_name[3]);

#if 0
#warning "Using vaguelly magic packed probe assignment - almost definitely wrong"
    static int probe = -1; // so that we ++ to 0
    static char last_probe[10] = "\0";

    if (strncasecmp (probe_name, last_probe, 2) != 0)
    {
	if (probe != -1 && last_probe[0] != probe_name[0] && probe_name[1] != '3' && probe_name[1] != '1') // we've dropped half of a probe pair
	    probe++;
	strcpy (last_probe, probe_name);

	probe++;

    }
    retval->probe = probe;
#endif

#if 0
#warning "Using guessed packing method" // this is wrong
/* Probes are packed a3 a2 c3 c2 d3 d2 e3 e2 a1 a0 c1 c0 d1 d0 e1 e0 ?? */
    retval->probe = (retval->probe_name[0] - 'A') * 2;
    int probe_index = atoi (&retval->probe_name[1]);
    if (probe_index <= 1)
	retval->probe += 8;
    retval->probe += 1 - (probe_index % 2);
#endif

    retval->probe = name_to_index (retval->probe_name);


#if 0
#warning "Overriding channel probes for SCSI & 61K"
    if (strncmp (retval->probe_name, "A2", 2) == 0)
	retval->probe = 2;
    else if (strncmp (retval->probe_name, "A1", 2) == 0)
	retval->probe = 8;
    else if (strncmp (retval->probe_name, "A0", 2) == 0)
	retval->probe = 9;
#endif

#if 0
#warning "Overriding channel probes for 8250"
    if (strncmp (retval->probe_name, "A3", 2) == 0)
	retval->probe = 1;
    else if (strncmp (retval->probe_name, "A2", 2) == 0)
	retval->probe = 2;
    else if (strncmp (retval->probe_name, "D2", 2) == 0)
	retval->probe = 7;
    else if (strncmp (retval->probe_name, "D3", 2) == 0)
	retval->probe = 3;
#endif


#if 0
    printf ("Added channel %s %s - (%d, %d) %s\n",
	    retval->probe_name, retval->name, retval->probe, retval->index, retval->inverted ? "Inverted" : "");
#endif

    return retval;
}

bulk_capture *build_dump (void *data, int length)
{
   bulk_capture *retval;
   capture *c;
   int i;
   int nrecords = length / sizeof (capture);

   if (length % sizeof (capture)) {
       fprintf (stderr, "Data length isn't a whole multiple of capture size: %d %% %d = %d\n",
       length, sizeof (capture), length % sizeof (capture));
       return NULL;
   }

   retval = malloc (sizeof (bulk_capture));
   retval->data = malloc (length);

   //data += sizeof (capture * 16); // skip the header crap

    /* Attempt to convert the endian-ness of them? */
    for (i = 0; i < nrecords; i++)
    {
	int v;
	c = (capture *)(((char *)data) + i * sizeof (capture));
	for (v = 0; v < CAPTURE_DATA_BYTES / 4; v++)
	{
	    uint32_t *val;
	    val = (uint32_t *)&c->data[v];
	    *val = htonl (*val);
	}

	memcpy (&retval->data[i], c, sizeof (capture));
    }

   //memcpy (retval->data, data, length);

   retval->length = length;

   //printf ("build_dump: %p, %d -> %p, %d\n", data, length, retval->data, retval->length);

   return retval;
 }



static void outtime (long long t, int max, int leading_zero)
{
//     printf ("[%10.10lld] ", t);
    char out [25], *s;
    char str [22];
    int len, i;

    len = sprintf (str, leading_zero ? "%12.12lld" : "%12lld", t);
    s = out;
    *s++ = '[';
    for (i = len - max; i < len; i++)
    {
        if (i != len - max && !(i % 3))
            *s++ = str [i - 1] == ' ' ? ' ' : ',';
        *s++ = str [i];
    }
    *s++ = ']';
    *s++ = ' ';
    *s++ = '\0';
    printf ("%s", out);
}

 //         printf ("[%10.10lld] ", time_now - first_time);

capture *first_capture = NULL;
int time_log (capture *c, char *msg, ...)
{
    char buffer[1024];
    va_list ap;
    //uint64_t time_now = capture_time (c); // we want it in nano-seconds
    uint64_t time_now = capture_time (c) / 1000; // we want it in useconds
    static uint64_t last_time = -1, first_time = 0;

    va_start (ap, msg);
    vsnprintf (buffer, 1024, msg, ap);
    va_end (ap);
    buffer[1023] = '\0';

#if 0
    printf ("[%10.10lld] ", time_now);
#endif
    if (first_capture && !first_time)
    {
	first_time = capture_time(first_capture) / 1000;
        if (first_time)
            printf ("Start time = %lld\n", first_time);
    }
    if (!option_set("no-timing")) {
        outtime (time_now - first_time, 10, 1);
//         printf ("[%10.10lld] ", time_now - first_time);


        if (last_time != -1)
            outtime (time_now - last_time, 7, 0);
//             printf ("[%8.8lld] ", time_now - last_time);
        else
            printf ("[None    ] ");
    }
    last_time = time_now;

    printf ("%s", buffer);

    return 0;
}

static const unsigned char ebcdic_to_ascii[256] = {
    0x00, 0x01, 0x02, 0x03, 0x85, 0x09, 0x86, 0x7f, /* 00-0f:           */
    0x87, 0x8d, 0x8e, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* ................ */
    0x10, 0x11, 0x12, 0x13, 0x8f, 0x0a, 0x08, 0x97, /* 10-1f:           */
    0x18, 0x19, 0x9c, 0x9d, 0x1c, 0x1d, 0x1e, 0x1f, /* ................ */
    0x80, 0x81, 0x82, 0x83, 0x84, 0x92, 0x17, 0x1b, /* 20-2f:           */
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x05, 0x06, 0x07, /* ................ */
    0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, /* 30-3f:           */
    0x98, 0x99, 0x9a, 0x9b, 0x14, 0x15, 0x9e, 0x1a, /* ................ */
    0x20, 0xa0, 0xe2, 0xe4, 0xe0, 0xe1, 0xe3, 0xe5, /* 40-4f:           */
    0xe7, 0xf1, 0xa2, 0x2e, 0x3c, 0x28, 0x2b, 0x7c, /*  ...........<(+| */
    0x26, 0xe9, 0xea, 0xeb, 0xe8, 0xed, 0xee, 0xef, /* 50-5f:           */
    0xec, 0xdf, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e, /* &.........!$*);^ */
    0x2d, 0x2f, 0xc2, 0xc4, 0xc0, 0xc1, 0xc3, 0xc5, /* 60-6f:           */
    0xc7, 0xd1, 0xa6, 0x2c, 0x25, 0x5f, 0x3e, 0x3f, /* -/.........,%_>? */
    0xf8, 0xc9, 0xca, 0xcb, 0xc8, 0xcd, 0xce, 0xcf, /* 70-7f:           */
    0xcc, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22, /* .........`:#@'=" */
    0xd8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 80-8f:           */
    0x68, 0x69, 0xab, 0xbb, 0xf0, 0xfd, 0xfe, 0xb1, /* .abcdefghi...... */
    0xb0, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, /* 90-9f:           */
    0x71, 0x72, 0xaa, 0xba, 0xe6, 0xb8, 0xc6, 0xa4, /* .jklmnopqr...... */
    0xb5, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, /* a0-af:           */
    0x79, 0x7a, 0xa1, 0xbf, 0xd0, 0x5b, 0xde, 0xae, /* .~stuvwxyz...[.. */
    0xac, 0xa3, 0xa5, 0xb7, 0xa9, 0xa7, 0xb6, 0xbc, /* b0-bf:           */
    0xbd, 0xbe, 0xdd, 0xa8, 0xaf, 0x5d, 0xb4, 0xd7, /* .............].. */
    0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /* c0-cf:           */
    0x48, 0x49, 0xad, 0xf4, 0xf6, 0xf2, 0xf3, 0xf5, /* {ABCDEFGHI...... */
    0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, /* d0-df:           */
    0x51, 0x52, 0xb9, 0xfb, 0xfc, 0xf9, 0xfa, 0xff, /* }JKLMNOPQR...... */
    0x5c, 0xf7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, /* e0-ef:           */
    0x59, 0x5a, 0xb2, 0xd4, 0xd6, 0xd2, 0xd3, 0xd5, /* \.STUVWXYZ...... */
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* f0-ff:           */
    0x38, 0x39, 0xb3, 0xdb, 0xdc, 0xd9, 0xda, 0x9f  /* 0123456789...... */
};

static uint8_t printable_char (int data)
{
        if (data >= ' ' && data <= 126)
	            return data;
	    return '.';
}

void display_data_buffer (unsigned char *buffer, int len, int flags)
{
    int i,j;
    int linelen;
    int skipped = 0;
#if 0
    const char *columns = getenv ("COLUMNS"); /* for some reason this doesn't work */
    int col = columns ? atoi (columns) : 80; /* Try to auto-adjust for terminal width */

    col -= 8; // line prefix
    col -= 4; // inter field stuff
    linelen = col / 7;
#endif

    if (!(flags & DISP_FLAG_both))
        flags |= DISP_FLAG_ascii;

    linelen = 16;

    if ((flags & DISP_FLAG_both) == DISP_FLAG_both)
        linelen = 12;
    //linelen = 8; // just do 8 across

    if (flags & DISP_FLAG_invert)
        linelen = 8;

    printf ("Data length: 0x%x (%d)\n", len, len);

    for (i = 0; i < len; i+=linelen)
    {
	int this_len = min(linelen, len - i);
	printf (" %4.4x:", i);
	for (j = 0; j < this_len; j++)
	    printf ("%2.2x ", buffer[i + j]);

#if 0
	printf ("%*s| ", (linelen - this_len) * 3, "");
	for (j = 0; j < this_len; j++)
	    printf ("%2.2x ", buffer[i + j] ^ 0xff);
#endif

        printf ("%*s", (linelen - this_len) * 3, "");

        if (flags & DISP_FLAG_ascii) {
            printf ("'");
            for (j = 0; j < this_len; j++) {
                unsigned char ch = buffer[i + j];
                printf ("%c", printable_char (ch));
            }

            printf ("%*s'", (linelen - this_len), "");
        }

        if (flags & DISP_FLAG_invert) {
            printf("  ");
            for (j = 0; j < this_len; j++)
                printf ("%2.2x ", buffer[i + j] ^ 0xff);
            printf ("%*s", (linelen - this_len) * 3, "");
        }

        if (flags & DISP_FLAG_ebcdic) {
            printf (" '");
            for (j = 0; j < this_len; j++) {
                unsigned char ch = ebcdic_to_ascii[buffer[i + j]];
                printf ("%c", printable_char (ch));
            }

            printf ("%*s'", (linelen - this_len), "");

            if (flags & DISP_FLAG_invert) {
                printf (" '");
                for (j = 0; j < this_len; j++) {
                    unsigned char ch = ebcdic_to_ascii[buffer[i + j] ^ 0xff];
                    printf ("%c", printable_char (ch));
                }
                printf ("%*s'", (linelen - this_len), "");
            }

        }


        printf ("\n");
        if (!(flags & DISP_FLAG_full_data) && i == linelen * 4 && !skipped)
        {
            int new_i;
            new_i = max(i, (len / linelen - 4) * linelen);
            if (new_i != i) {
                printf ("   <omitting buffer display as size 0x%x too large>\n", len);
                i = new_i;
            }
            skipped = 1;
        }
    }
}

static void display_buffer_line (unsigned char *buff, int bufflen, int linelen)
{
    int j;

    for (j = 0; j < bufflen; j++)
        printf ("%2.2x ", buff[j]);

    printf ("%*s'", (linelen - bufflen) * 3, "");
    for (j = 0; j < bufflen; j++)
        printf ("%c", printable_char (buff[j]));

    printf ("%*s'", (linelen - bufflen), "");
}

void display_dual_data_buffer (unsigned char *buff1, int len1, unsigned char *buff2, int len2)
{
    int i;
    int linelen = 8;
    int maxlen = max(len1, len2);

    for (i = 0; i < maxlen; i+=linelen)
    {
	int this_len;
	printf ("  %4.4x: ", i);

	this_len = max(min(linelen, len1 - i), 0);
        display_buffer_line (&buff1[i], this_len, linelen);
        printf (" | ");

	this_len = max(min(linelen, len2 - i), 0);
        display_buffer_line (&buff2[i], this_len, linelen);

        printf ("\n");
    }
}

