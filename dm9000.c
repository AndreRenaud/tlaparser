#include <stdio.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"

static int dm9000_data (capture *c, list_t *channels)
{
    char name[20];
    int retval = 0;
    int i;
    int max = 16;

    if (option_set ("8-bit"))
        max = 8;

    for (i = 0; i < max; i++) {
        sprintf (name, "md%d", i);
        if (capture_bit_name (c, name, channels))
            retval |= 1 << i;
    }

    return retval;
}

static const char *register_name (int addr)
{
    switch (addr) {
        case 0x00: return "NCR";
        case 0x01: return "NSR";
        case 0x02: return "TCR";
        case 0x03: return "TSR1";
        case 0x04: return "TSR2";
        case 0x05: return "RCR";
        case 0x06: return "RSR";
        case 0x07: return "ROCR";
        case 0x08: return "BPTR";
        case 0x09: return "FCTR";
        case 0x0a: return "FCR";
        case 0x0b: return "EPCR";
        case 0x0c: return "EPAR";
        case 0x0d: return "EPDRL";
        case 0x0e: return "EPDRH";
        case 0x0f: return "WCR";

        case 0x1e: return "GPCR";
        case 0x1f: return "GPR";

        case 0x28: return "VIDL";
        case 0x29: return "VIDH";
        case 0x2a: return "PIDL";
        case 0x2b: return "PIDH";

        case 0xf0: return "MRCMDx";
        case 0xf2: return "MRCMD";
        case 0xf4: return "MRRL";
        case 0xf5: return "MRRH";
        case 0xf6: return "MWCMDX";
        case 0xf8: return "MWCMD";
        case 0xfa: return "MWRL";
        case 0xfb: return "MWRH";
        case 0xfc: return "TXPLL";
        case 0xfd: return "TXPLH";
        case 0xfe: return "ISR";
        case 0xff: return "IMR";
        default: return "UNKNOWN";
    }
}

struct pin_assignments
{
    int init;
    channel_info *cs;
    channel_info *read;
    channel_info *write;
    channel_info *cmd;
};

static FILE *open_packet (void)
{
    static int id = 0;
    char buffer[200];
    sprintf (buffer, "packet-%d.dat", id++);
    return fopen (buffer, "wb");
}

static void add_packet_data (int data, FILE *fp)
{
    char ch = data;

    if (!fp)
        return;

#warning "Not 16-bit"
    fwrite (&ch, 1, 1, fp);
}

static void parse_dm9000_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static int last_io_write = -1;
    static FILE *packet = NULL;

    if (pa.init == -1) {
        pa.init = 1;
        pa.cs = capture_channel_details (c, "cs4-n", channels);
        pa.read = capture_channel_details (c, "oe-n", channels);
        pa.write = capture_channel_details (c, "rw-n", channels);
        pa.cmd = capture_channel_details (c, "a19", channels);
    }

    if (!prev)
        return;

    if (capture_bit (c, pa.cs) == 0) {
        /* Read */
        if (capture_bit_transition (c, prev, pa.read, TRANSITION_low_to_high)) {
            unsigned int data = dm9000_data (c, channels);
            int cmd = capture_bit (c, pa.cmd);
            if (last_io_write == -1)
                time_log (c, "%s Read: 0x%x\n",
                        cmd ? "Data" : "IO", data);
            else
                time_log (c, "Register read: 0x%x (%s) = 0x%x\n", 
                        last_io_write, register_name (last_io_write) ,data);

            last_io_write = -1;
        }
        /* Write */
        if (capture_bit_transition (c, prev, pa.write, TRANSITION_low_to_high)) {
            unsigned int data = dm9000_data (c, channels);
            int cmd = capture_bit (c, pa.cmd);

            if (!cmd && last_io_write != -1)
                time_log (c, "Argh - !cmd after !cmd: 0x%x %s\n", 
                        last_io_write, register_name (last_io_write));

            if (last_io_write != -1) {
                if (packet) { fclose (packet); packet = NULL; }
                time_log (c, "Register write: 0x%x (%s) = 0x%x\n", 
                        last_io_write, 
                        register_name (last_io_write), data);
                if (last_io_write == 0xf8) {// mwcmd
                    time_log (c, "Starting new packet\n");
                    packet = open_packet ();
                    add_packet_data (data, packet);
                }
            } else if (cmd) {
                time_log (c, "Data Write: 0x%x\n", data);
                add_packet_data (data, packet);
            }

            if (cmd)
                last_io_write = -1;
            else
                last_io_write = data;
        }
    }
}

void parse_dm9000 (bulk_capture *b, char *filename, list_t *channels)
{

    int i;
    capture *c, *prev = NULL;

    printf ("dm9000 analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
        parse_dm9000_cap (c, prev, channels);
        prev = c;
        c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));

}

