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

static int get_read_data(capture *c, struct unformatted_info *info)
{
    int i;
    int result = 0;

    for (i = 0; i < 9; i++)
        result |= capture_bit(c, info->read[i]) ? 0 : 1 << i;
    return result;
}

static int get_write_data(capture *c, struct unformatted_info *info)
{
    int i;
    int result = 0;

    for (i = 0; i < 9; i++)
        result |= capture_bit(c, info->write[i]) ? 0 : 1 << i;
    return result;
}

static void save_buffer(int index, uint8_t *data, int len, int skip, int offset)
{
    FILE *fp;
    char name[20];
    int i;
    sprintf(name, "output-%d.bin", index);
    fp = fopen(name, "wb");
    if (!fp) {
        fprintf(stderr, "Unable to open '%s'\n", name);
        return;
    }
    for (i = offset * skip; i < len; i+= skip)
        fwrite(&data[i], 1, 1, fp);
    fclose(fp);
}

static void parse_unformatted_cap(capture *c, list_t *channels)
{
    static capture *prev = NULL;
    static struct unformatted_info info = {.init = 0};
    int change;
    static uint16_t read_data[10240];
    static uint16_t write_data[10240];
    static int read_data_pos = 0;
    static int write_data_pos = 0;
    int read_index[] = {21, 20, 19, 18, 14, 15, 16, 17, 23};
    int write_index[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
    int i, moving;
    static int output = 0;

    if (info.init == 0) {
        char name[20];
        info.fwd = capture_channel_details("IO_DEV[13]_FWD", channels);
        info.rev = capture_channel_details("IO_DEV[11]_REV", channels);
        info.wsel = capture_channel_details("IO_DEV[32]_WSEL", channels);
        info.rds = capture_channel_details("IO_DEV[22]_RDS", channels);
        info.wds = capture_channel_details("IO_DEV[33]_WDS", channels);

        for (i = 0; i < 9; i++) {
            sprintf(name, "IO_DEV[%2.2d]", read_index[i]);
            info.read[i] = capture_channel_details(name, channels);
        }
        for (i = 0; i < 9; i++) {
            sprintf(name, "IO_DEV[%2.2d]", write_index[i]);
            info.write[i] = capture_channel_details(name, channels);
        }

        info.init = 1;
    }

    if (!prev)
        goto out;

    moving = (!capture_bit(c, info.fwd)) || (!capture_bit(c, info.rev));
    if (moving) {
        if (capture_bit_transition(c, prev, info.rds, TRANSITION_rising_edge))
            read_data[read_data_pos++] = get_read_data(c, &info);
        if (capture_bit_transition(c, prev, info.wds, TRANSITION_rising_edge))
            write_data[write_data_pos++] = get_write_data(c, &info);
    }

    // If we've just stopped a fwd/reverse, then dump any outstanding buffers
    if (capture_bit_transition(c, prev, info.fwd, TRANSITION_rising_edge) ||
        capture_bit_transition(c, prev, info.rev, TRANSITION_rising_edge)) {
        if (read_data_pos) {
            printf("read data: %d\n", output);
            save_buffer(output++, (void *)read_data, read_data_pos * 2, 2, 0);
            display_data_buffer((unsigned char *)read_data,
                    read_data_pos * 2, DISP_FLAG_full_data | DISP_FLAG_invert | DISP_FLAG_ebcdic);
            read_data_pos = 0;
        }
        if (write_data_pos) {
            printf("write data: %d\n", output);
            save_buffer(output++, (void *)write_data, write_data_pos * 2, 4, 40);
            display_data_buffer((unsigned char *)write_data,
                    write_data_pos * 2, DISP_FLAG_full_data | DISP_FLAG_invert | DISP_FLAG_ebcdic);
            write_data_pos = 0;
        }
    }


    if (capture_bit_change(c, prev, info.wsel))
        time_log(c, "Writing %s\n", capture_bit(c, info.wsel) ?
                                    "stop" : "start");

    if (capture_bit_change(c, prev, info.fwd))
        time_log(c, "FWD Change: %d\n", capture_bit_change(c, prev, info.fwd));

    if (capture_bit_change(c, prev, info.rev))
        time_log(c, "REV Change: %d\n", capture_bit_change(c, prev, info.rev));

out:
    prev = c;
}

void parse_half_formatted(bulk_capture *b, char *filename, list_t *channels)
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
