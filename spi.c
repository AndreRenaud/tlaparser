#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

static const char *sst_cmd_to_name (int cmd)
{
    switch (cmd) {
        case 0x3: return "read";
        case 0xb: return "high-speed read";
        case 0x20: return "sector erase";
        case 0x52: return "block erase";
        case 0x60: return "chip-erase";
        case 0x02: return "byte program";
        case 0xaf: return "auto address increment";
        case 0x5: return "read status";
        case 0x50: return "enable write status register";
        case 0x01: return "write status register";
        case 0x6: return "write enable";
        case 0x04: return "write disable";
        case 0x90: return "read id";
        case 0xab: return "read id";
        default: return "unknown";
    }
}

// which commands will be followed by an address
static int sst_cmd_want_addr (int cmd)
{
    switch (cmd) {
        case 0x3:
        case 0xb:
        case 0x20:
        case 0x52:
        case 0x02:
        case 0xaf:
            return 1;
        default:
            return 0;
    }
}

static void decode_frame (unsigned char *in_data, unsigned char *out_data, int len)
{
    int i;
    unsigned char cmd;
    const char *cmd_string;
    int addr = -1;
    int pos = 0;

    if (len <= 0)
        return;

    cmd = out_data[0];
    cmd_string = sst_cmd_to_name (cmd);
    printf ("cmd: %s [len = %d]\n", cmd_string, len);
    pos++;

    if (sst_cmd_want_addr (cmd)) {
        if (len < 4)
            printf ("got command that wants an address, but no address supplied (%d len)\n", len);
        addr = out_data[1] << 16;
        addr |= out_data[2] << 8;
        addr |= out_data[3];
        pos += 3;
        printf ("addr: 0x%x\n", addr);
    }

    display_dual_data_buffer (&in_data[pos], len - pos, &out_data[pos], len - pos);
}

struct pin_assignments
{
    int init;
    channel_info *mosi;
    channel_info *miso;
    channel_info *sfrm;
    channel_info *sclk;
};

static void parse_spi_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static int in_frame = 0;
    static int bits;
    static unsigned char in_data[1024];
    static unsigned char out_data[1024];
    static int frame_count = 0;
    static int frame_start;

    if (pa.init == -1) {
        pa.init = 1;
        pa.mosi = capture_channel_details (c, "mosi", channels);
        pa.miso = capture_channel_details (c, "miso", channels);
        pa.sfrm = capture_channel_details (c, "sfrm", channels);
        pa.sclk = capture_channel_details (c, "sclk", channels);
    }

    if (!prev)
        return;

    if (capture_bit_transition (c, prev, pa.sfrm, TRANSITION_high_to_low)) {
        time_log (c, "Start of frame %d\n", frame_count++);
        in_frame = 1;
        bits = 0;
        memset (out_data, 0, sizeof (out_data));
        memset (in_data, 0, sizeof (in_data));
        frame_start = capture_time (c);
    }

    if (capture_bit_transition (c, prev, pa.sfrm, TRANSITION_low_to_high)) {
        if (bits % 8)
            time_log (c, "Finished a frame, but the number if bits isn't divisible by 8: %d", bits);
        decode_frame (in_data, out_data, bits / 8);
        time_log (c, "End of frame [%dus]\n", 
                (capture_time (c) - frame_start) / 1000);
        in_frame = 0;
    }

    if (capture_bit_transition (c, prev, pa.sclk, TRANSITION_low_to_high)) {
        if (in_frame) {
            int this_bit = 7 - (bits % 8);
            out_data[bits / 8] |= capture_bit (c, pa.mosi) << this_bit;
            in_data[bits / 8] |= capture_bit (c, pa.miso) << this_bit;

            bits++;
        } else
            time_log (c, "clk transition with no SPI frame\n");
    }

        
}

void parse_spi (bulk_capture *b, char *filename, list_t *channels)
{

    int i;
    capture *c, *prev = NULL;

    printf ("SPI analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
        parse_spi_cap (c, prev, channels);
        prev = c;
        c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));

}

