#include <stdio.h>
#include <stdlib.h>

#include "dumpdata.h"
#include "common.h"

//#define HAVE_DBG_PINS

static int active_device = -1;

//  from pg 45 table 8 of scsi 2 spec, based on msg, c/d, i/o
static char *scsi_phases[] = { "DATA OUT", "DATA IN", "COMMAND", "STATUS", "*1", "*2", "MESSAGE OUT", "MESSAGE IN"};

// direct access commands

struct scsi_cmd
{
    char *name;
    int code;
};

#define INVERT(a) ((a) ? 0 : 1)

static struct scsi_cmd scsi_commands [] = {
    {"CHANGE DEFINITION", 0x40}, 
    {"COMPARE", 0x39}, 
    {"COPY", 0x18}, 
    {"COPY AND VERIFY", 0x3A}, 
    {"FORMAT UNIT", 0x04}, 
    {"INQUIRY", 0x12}, 
    {"LOCK-UNLOCK CACHE", 0x36}, 
    {"LOG SELECT", 0x4C}, 
    {"LOG SENSE", 0x4D}, 
    {"MODE SELECT(6)", 0x15}, 
    {"MODE SELECT(10)", 0x55}, 
    {"MODE SENSE(6)", 0x1A}, 
    {"MODE SENSE(10)", 0x5A}, 
    {"PRE-FETCH", 0x34}, 
    {"PREVENT-ALLOW MEDIUM REMOVAL", 0x1E}, 
    {"READ(6)", 0x08}, 
    {"READ(10)", 0x28}, 
    {"READ BUFFER", 0x3C}, 
    {"READ CAPACITY", 0x25}, 
    {"READ DEFECT DATA", 0x37}, 
    {"READ LONG", 0x3E}, 
    {"REASSIGN BLOCKS", 0x07}, 
    {"RECEIVE DIAGNOSTIC RESULTS", 0x1C}, 
    {"RELEASE", 0x17}, 
    {"REQUEST SENSE", 0x03}, 
    {"RESERVE", 0x16}, 
    {"REZERO UNIT", 0x01}, 
    {"SEARCH DATA EQUAL", 0x31}, 
    {"SEARCH DATA HIGH", 0x30}, 
    {"SEARCH DATA LOW", 0x32}, 
    {"SEEK(6)", 0x0B}, 
    {"SEEK(10)", 0x2B}, 
    {"SEND DIAGNOSTIC", 0x1D}, 
    {"SET LIMITS", 0x33}, 
    {"START STOP UNIT", 0x1B}, 
    {"SYNCHRONIZE CACHE", 0x35}, 
    {"TEST UNIT READY", 0x00}, 
    {"VERIFY", 0x2F}, 
    {"WRITE(6)", 0x0A}, 
    {"WRITE(10)", 0x2A}, 
    {"WRITE AND VERIFY", 0x2E}, 
    {"WRITE BUFFER", 0x3B}, 
    {"WRITE LONG", 0x3F}, 
    {"WRITE SAME", 0x41},
    {"Read block limits", 0x05},
    {"Space", 0x11},
    {"Write filemark", 0x10},
    {"Mode Select", 0x15},
};
#define SCSI_COMMAND_LEN (sizeof (scsi_commands) / sizeof (scsi_commands[0]))

static char *scsi_command_name (int cmd)
{
    int i;
    if (cmd & 0x80)
        return "Identify";

    for (i = 0; i < SCSI_COMMAND_LEN; i++)
	if (scsi_commands[i].code == cmd)
	    return scsi_commands[i].name;
    return "Unknown";
}

static void decode_scsi_command (int phase, unsigned char *buf, int last_phase_command)
{
    switch (phase)
    {
	case 1: // DATA IN
	{
	    switch (last_phase_command) // work out what hte last command is, since this is its response data
	    {
		case 0x12: // inquiry response data
		    printf ("\tInquiry response data\n");
		    break;

                case 0x03: // request sense - page 123, 8.2.14, Table 65
                {
                    unsigned int v, len;
                    if (!(buf[0] & 0x80)) 
                        printf ("\tRequest Sense not valid\n");
                    printf ("\tError code: 0x%x\n", buf[0] & 0x7f);
                    printf ("\tFlags:%s%s%s Key: 0x%x\n", 
                            buf[2] & 0x80 ? " Filemark" : "",
                            buf[2] & 0x40 ? " EOM" : "",
                            buf[2] & 0x20 ? " ILI" : "",
                            buf[2] & 0x0f);
#if 0
                    v = buf[3] << 24 | buf[4] << 16 | buf[5] << 8 | buf[6];
                    printf ("\tInformation: 0x%x\n", v);

                    len = buf[7];
                    printf ("\tAdditional Len: %d (%d total)\n", len, len + 7);
                    printf ("\tASC: 0x%x\n", buf[12]);
                    printf ("\tASCQ: 0x%x\n", buf[13]);
                    printf ("\tSense Key %s\n", (buf[15] & 0x80) ? "valid" : "invalid");
                    v = (buf[15] & 0x7f) << 16 | buf[16] << 8 | buf[17];
                    printf ("\tSense-Key:0x%x\n", v);

#endif
                    break;
                }

	    }
	    break;
	}

	case 2: // COMMAND
	{
	    int cmd = buf[0];
	    printf ("\t%s [%d 0x%x]\n", scsi_command_name (cmd), cmd, cmd);
	    switch (cmd)
	    {
		case 0x08: // read
                case 0xa: // write
                {
                    unsigned int len;
		    //printf (" address=0x%2.2x%2.2x len=0x%x\n", 
                            //buf[2], buf[3], buf[4]);
                    len = buf[2] << 16 | buf[3] << 8 | buf[4];
                    printf (" length=0x%x\n", len);
		    break;
                }

                case 0x11: // space
                {
                    unsigned int code = buf[1];
                    int len = buf[2] << 16 | buf[3] << 8 | buf[4];

                    if (len & (1 << 23))
                        len |= 0xff000000; // 2s complement


                    printf ("\tcode=%d length=%d\n", code, len);
                    break;
                }


		default:
		    break;
	    }
	    break;
	}

	case 6: // message out
        case 7: // message in
	{
	    int cmd = buf[0];
	    if (cmd & 0x80)
		printf ("\tIdentify\n");
	    else {
                switch (cmd) { // Table 10, page 56 of X3T9.2/375R rev 10L
                    case 0x0: printf ("\tCommand complete\n"); break;
                    case 0x02: printf ("\tSave data pointer\n"); break;
                    case 0x03: printf ("\tRestore pointers\n"); break;
                    case 0x04: printf ("\tDisconnect\n"); break;
                    default: printf ("\tUnknown command: %d\n", cmd);
                }
            }
	    break;
	}

	default:
	    break;
    }
}


static int parity (int val)
{
    int par = 0;
    int i;

    for (i = 0; i < 8; i++)
	par = par ^ (val & (1 << i) ? 0 : 1);

    return par;
}



static int get_data (capture *c, list_t *channels)
{
    int retval = 0;
    char name[10];
    int i;
    int bit;
    int par;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "d<%d>", i);
	bit = capture_bit_name (c, name, channels) ? 0 : 1; // invert the logic
	retval |= bit << i;
    }

    par = capture_bit_name (c, "dp", channels);

    if (par != parity (retval))
	time_log (c, "parity error: 0x%x (par = %d, not %d)\n", retval, par, parity (retval));

    return retval;
}

struct pin_assignments
{
    int init;

    channel_info *nbsy;
    channel_info *nack;
    channel_info *nreq;
    channel_info *nsel;
    channel_info *nrst;
    channel_info *nio;
#ifdef HAVE_DBG_PINS
    channel_info *dbg[4];
#endif
};

static int ffs (unsigned int val)
{
    int i;
    for (i = 31; i >= 0; i--)
        if (val & (1 << i))
            return i;
    return -1;
}

static void add_output_data (unsigned char ch)
{
    static FILE *fp = NULL;
    if (!fp) 
        if ((fp = fopen ("output.dat", "wb")) == NULL) {
            perror ("Unable to open output.dat");
            return;
        }
    fwrite (&ch, 1, 1, fp);
}

static void parse_scsi_cap (capture *c, list_t *channels, int last_cap)
{
    static struct pin_assignments pa = {-1};
    static capture *prev = NULL;
    static int last_phase = -1;
    static unsigned char buffer[1024 * 1024];
    static int buffer_len = 0;
    static int current_device = -1;
    static int last_phase_command = -1;
#if 0
    static int last_good_ack = 0;
    static int outstanding_nreq = 0;
    static uint64_t last_nreq = 0;
    int i;
#endif

    if (pa.init == -1 && c)
    {
	pa.init = 1;
	pa.nbsy = capture_channel_details (c, "nbsy", channels);
	pa.nack = capture_channel_details (c, "nack", channels);
	pa.nreq = capture_channel_details (c, "nreq", channels);
	pa.nsel = capture_channel_details (c, "nsel", channels);
	pa.nrst = capture_channel_details (c, "nrst", channels);
	pa.nio = capture_channel_details (c, "nio", channels);

#ifdef HAVE_DBG_PINS
	for (i = 0; i < 4; i++)
	{
	    char dbgbuffer[10];
	    sprintf (dbgbuffer, "dbg%d", i);
	    pa.dbg[i] = capture_channel_details (c, dbgbuffer, channels);
	}
#endif
    }

    if (!prev)
	goto out;

    // rising edge of nbsy, with nsel high => bus free
    if (capture_bit_transition (c, prev, pa.nbsy, TRANSITION_low_to_high) && 
        capture_bit (c, pa.nsel)) {
        if (current_device != -1) {
            time_log (c, "Bus free\n");
            current_device = -1;
        }
    }

    // nSel going low while busy is low impliese the end of arbitration
    if (option_set("verbose"))
        if (!capture_bit(c, pa.nbsy) &&
                capture_bit_transition(c, prev, pa.nsel, TRANSITION_high_to_low)) {
            int arb = get_data(c, channels) ^ 0xff;
            time_log (c, "Arbitrator: 0x%x\n", arb);
        }

    if (capture_bit_transition(c, prev, pa.nbsy, TRANSITION_low_to_high) &&
        !capture_bit(c, pa.nsel)) {
    	int ch = get_data (c, channels);
        //time_log (c, "B Selection: 0x%x\n", ch);
	if (ch != current_device) {
            int dev1, dev2;

            current_device = ch;
            dev1 = ffs(ch);
            dev2 = ffs(ch & ~(1 << dev1));
	    time_log (c, "Selected device: 0x%2.2x (dev1: %d dev2: %d)\n", 
                    ch, dev1, dev2);
            if (dev1 < 0 || dev2 < 0)
                time_log (c, "Invalid device selection: 0x%2.2x\n", ch);
            else if (ch & ~(1 << dev1 | 1 << dev2))
                time_log (c, "More than 2 devices set in 0x%2.2x\n", ch);
        }
    }

#ifdef HAVE_DBG_PINS
    for (i = 0; i < 4; i++)
    {
	if (capture_bit (c, pa.dbg[i]) != capture_bit (prev, pa.dbg[i]))
	    time_log (c, "DBG %d change: %d\n", i, capture_bit (c, pa.dbg[i]));
    }
#endif

    if (capture_bit (c, pa.nrst) != capture_bit (prev, pa.nrst))
	time_log (c, "Bus reset %s\n", capture_bit (c, pa.nrst) ? "end" : "start");

    if (active_device != -1 && current_device != active_device)
        goto out;

#if 0
    if (!capture_bit (c, pa.nreq) && capture_bit_transition (c, prev, pa.nack, TRANSITION_high_to_low))
    {
	outstanding_nreq = 1;
	last_nreq = capture_time (c);
    }

    if (capture_bit_transition (c, prev, pa.nreq, TRANSITION_high_to_low))
	outstanding_nreq = 0;

    if (outstanding_nreq && (capture_time (c) - last_nreq) > 1000 && buffer_len > 0)
    {
	time_log (c, "Timeout, dumping buffer\n");
	display_data_buffer (buffer, buffer_len, 0);
	buffer_len = 0;
	outstanding_nreq = 0;
    }
#endif

    // bsy is low, and nack goes from high to low
    if (!capture_bit (c, pa.nbsy)) {
        int nio = capture_bit (c, pa.nio);
        int got_data;
        if (nio)
            got_data = capture_bit_transition(c, prev, pa.nack,
                    TRANSITION_high_to_low);
        else
            got_data = capture_bit_transition(c, prev, pa.nreq, 
                    TRANSITION_high_to_low);
        if (got_data) {
            int phase = 0;
            int ch;


            phase |= capture_bit_name (c, "nMSG", channels) << 2;
            phase |= capture_bit_name (c, "nCD", channels) << 1;
            phase |= capture_bit_name (c, "nIO", channels) << 0;
            phase = (~phase) & 0x7; // invert, since signals are negative logic

            if (phase != last_phase && last_phase != -1) {
                time_log (c, "Phase: %s (%d)\n", scsi_phases[last_phase], last_phase);
                decode_scsi_command (last_phase, buffer, last_phase_command);
                display_data_buffer (buffer, buffer_len, 0);
                buffer_len = 0;
            }
            ch = get_data (c, channels);
            if (phase == 2 && buffer_len == 0) // COMMAND phase, first byte, so record the command
                last_phase_command = ch;

            if (buffer_len == 0)
                time_log (c, "Data start\n");
            if (phase == 1 && last_phase_command == 8) // read command, data_in
                add_output_data (ch);
            buffer[buffer_len] = ch;
            buffer_len++;
            if (buffer_len > sizeof (buffer))
            {
                fprintf (stderr, "ARGH BUFFER OVERUN\n");
                abort ();
            }
            last_phase = phase;
        }
    }
   
    // bsy went high, end of transaction 
    if (last_phase != -1 && 
        (capture_bit_transition (c, prev, pa.nbsy, TRANSITION_low_to_high) || 
         last_cap))
    {
	time_log (c, "Phase: %s (%d) (nbsy)\n", scsi_phases[last_phase], last_phase);
	decode_scsi_command (last_phase, buffer, last_phase_command);
	display_data_buffer (buffer, buffer_len, 0);
	buffer_len = 0;
	last_phase = -1;
    }

out:
    prev = c;
}

void parse_scsi (bulk_capture *b, char *filename, list_t *channels)
{
    int i;
    capture *c;
    char buffer[100];

    printf ("SCSI analysis of file: '%s' (%d samples)\n", filename,
            b->length / sizeof (capture));

    if (option_val ("device", buffer, 10))
	active_device = strtoul (buffer, NULL, 0);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_scsi_cap (c, channels, i == (b->length / sizeof (capture)) - 1);
	c++;
    }
}
