#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

struct unformatted_info
{
    int init;
    channel_info *fwd;
    channel_info *rev;
    channel_info *wsel;
    channel_info *rds;
    channel_info *wds;

    channel_info *read[9];
    channel_info *write[9];
};

static unsigned int get_read_data(capture *c, struct unformatted_info *info)
{
    int i;
    unsigned int result = 0;

    for (i = 0; i < 9; i++)
        result |= capture_bit(c, info->read[i]) ? 0 : 1 << i;
    return result;
}

static unsigned int get_write_data(capture *c, struct unformatted_info *info)
{
    int i;
    unsigned int result = 0;

    for (i = 0; i < 9; i++)
        result |= capture_bit(c, info->write[i]) ? 0 : 1 << i;
    return result;
}

static void parse_unformatted_cap(capture *c, list_t *channels)
{
    static capture *prev = NULL;
    static struct unformatted_info info = {.init = 0};
    static int in_write = 0;
    int change;
    static uint16_t read_data[10240];
    static uint16_t write_data[10240];
    static int read_data_pos = 0;
    static int write_data_pos = 0;
    int read_index[] = {11, 10, 9, 8, 7, 6, 5, 4, 13};
    int write_index[] = {30, 31, 32, 33, 0, 1, 2, 3, 29};
    int i;

    if (info.init == 0) {
        char name[20];
        info.fwd = capture_channel_details("FWD", channels);
        info.rev = capture_channel_details("REV", channels);
        info.wsel = capture_channel_details("WSEL", channels);
        info.rds = capture_channel_details("RDS", channels);
        info.wds = capture_channel_details("WDS", channels);

        for (i = 0; i < 9; i++) {
            sprintf(name, "RD%c", i < 8 ? '0' + i : 'P');
            info.read[i] = capture_channel_details(name, channels);
        }
        for (i = 0; i < 9; i++) {
            sprintf(name, "WD%c", i < 8 ? '0' + i : 'P');
            info.write[i] = capture_channel_details(name, channels);
        }

        info.init = 1;
    }

    if (!prev)
        goto out;

    change = capture_bit_change(c, prev, info.wsel);
    if (change) {
        if (change == TRANSITION_high_to_low)
            in_write = 1;
        else if (change == TRANSITION_low_to_high) {
            if (read_data_pos && write_data_pos) {
                if (memcmp(read_data, write_data,
                            min(read_data_pos, write_data_pos) * 2)) {
                    time_log(c, "Data mismatch in read/write data up to %d\n",
                        min(read_data_pos, write_data_pos));
                }
            }
            if (read_data_pos) {
                time_log(c, "Read Data:\n");
                display_data_buffer((unsigned char *)read_data,
                        read_data_pos * 2, DISP_FLAG_full_data);
                read_data_pos = 0;
            }
            if (write_data_pos) {
                time_log(c, "Write Data:\n");
                display_data_buffer((unsigned char *)write_data,
                        write_data_pos * 2, DISP_FLAG_full_data);
                write_data_pos = 0;
            }
            in_write = 0;
        }
        time_log(c, "WSel change: %d %s\n", change, in_write ? "Start" : "Stop");
    }

    if (capture_bit(c, info.wsel))
        if (capture_bit_transition(c, prev, info.wds, TRANSITION_rising_edge))
            write_data[write_data_pos++] = get_write_data(c, &info);
    if (capture_bit_transition(c, prev, info.rds, TRANSITION_rising_edge))
        read_data[read_data_pos++] = get_read_data(c, &info);

    if (capture_bit_change(c, prev, info.fwd))
        time_log(c, "FWD Change: %d\n", capture_bit_change(c, prev, info.fwd));

    if (capture_bit_change(c, prev, info.rev))
        time_log(c, "REV Change: %d\n", capture_bit_change(c, prev, info.rev));

out:
    prev = c;
}

void parse_unformatted(bulk_capture *b, char *filename, list_t *channels)
{
    int i;
    capture *c;

    printf("Unformatted analysis of '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof(capture); i++) {
        parse_unformatted_cap(c, channels);
        c++;
    }
}
