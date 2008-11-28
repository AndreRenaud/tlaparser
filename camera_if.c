#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

static unsigned int get_data (capture *c, list_t *channels)
{
    unsigned int data = 0;
    char name[10];
    int i;

    for (i = 2; i < 10; i++) {
        sprintf (name, "d%d", i);
        if (capture_bit_name (c, name, channels))
            data |= 1 << (i - 2);
    }

    return data;
}

struct pin_assignments
{
    int init;
    channel_info *href;
    channel_info *vref;
    channel_info *pclk;
};

static void display_buffer(unsigned char *buffer, int len)
{
    static int i = 0;
    char filename[20];
    FILE *fp;

    display_data_buffer (buffer, len, 0);

    sprintf (filename, "file-%d.jpg", i);
    if ((fp = fopen (filename, "wb")) != NULL) {
        fwrite (buffer, 1, len, fp);
        fclose (fp);
    } else {
        perror ("Cannot open file");
    }
    i++;

}

static void parse_camera_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static int capture_len = 0;
    static unsigned char data[10 * 1024 * 1024];
    static int bytes_per_href = 0;
    static int href_per_vref = 0;
    static int bytes_outside_href = 0;

    if (pa.init == -1) {
        pa.init = 1;
        pa.href = capture_channel_details (c, "href", channels);
        pa.vref = capture_channel_details (c, "vref", channels);
        pa.pclk = capture_channel_details (c, "pclk", channels);
    }

    if (!c && !prev) {
        if (capture_len)
            display_buffer (data, capture_len);
        if (href_per_vref)
            time_log (c, "Href/Vref: %d\n", href_per_vref);
    }
    if (!prev)
        return;

    //time_log (c, "capture\n");
    
    if (!capture_bit(c, pa.vref)) {
        if (href_per_vref) {
            time_log (c, "Href/Vref: %d\n", href_per_vref);
            href_per_vref = 0;
        }
        if (capture_len) {
            display_buffer (data, capture_len);
            capture_len = 0;
        }
    } else {
        if (capture_bit_transition (c, prev, pa.href, TRANSITION_low_to_high))
            href_per_vref++;

        if (capture_bit (c, pa.href)) {
            if (capture_bit_transition (c, prev, pa.pclk, TRANSITION_low_to_high)) {
                data[capture_len] = get_data(c, channels);
                capture_len++;
                bytes_per_href++;
            }
            if (bytes_outside_href) {
                time_log (c, "Bytes outside href: %d\n", bytes_outside_href);
                bytes_outside_href = 0;
            }
        } else {
            bytes_outside_href++;
            if (bytes_per_href) {
                time_log (c, "Href bytes: %d\n", bytes_per_href);
                bytes_per_href = 0;
            }
        }
    }
}

void parse_camera (bulk_capture *b, char *filename, list_t *channels)
{
    int i;
    capture *c, *prev = NULL;

    printf ("Camera analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
        parse_camera_cap (c, prev, channels);
        prev = c;
        c++;
    }

    parse_camera_cap (NULL, NULL, channels);

    printf ("Parsed %d captures\n", b->length / sizeof (capture));

}

