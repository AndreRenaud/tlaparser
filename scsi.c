#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dumpdata.h"
#include "common.h"


/* options:

option    example        meaning
device    -o device=4    sets scsi id we are interested in - only transactions to/from
                         this device will be displayed

glitch    -o glitch      detect glitches in nreq/nack - see detect_glitches below

debug     -o debug       debug pins are present and should be displayed
*/


//  from pg 45 table 8 of scsi 2 spec, based on msg, c/d, i/o
static char *scsi_phases[] = { "DATA OUT", "DATA IN", "COMMAND", "STATUS", "*1", "*2", "MESSAGE OUT", "MESSAGE IN"};


typedef struct scsi_info
{
    list_t *channels;       //!< list of channels in the trace
    channel_info *nbsy;
    channel_info *nack;
    channel_info *nreq;
    channel_info *nsel;
    channel_info *nrst;
    channel_info *nio;
    channel_info *nmsg;
    channel_info *ncd;
    channel_info *natn;
    channel_info *dbg[4];
    channel_info *data [9]; // 8 data bits and parity

    capture *prev;
    int last_phase;
    capture *last_phase_capture;
    unsigned char buffer[1024 * 1024];
    int buffer_len;
    int current_devices[2];
    int last_phase_command;
    int data_state;
    int waiting_for_idle;   //!< we waiting for bus to become idle - we do this on startup
#if 0
    int last_good_ack = 0;
    int outstanding_nreq = 0;
    uint64_t last_nreq = 0;
    int i;
#endif
//     long long time_offset;
    int last_valid_phase;
    capture *time_reqed;        //!< capture record at which to record data
    int longest_req_glitch;
    long long time_unreqed;
    int longest_unreq_glitch;
    int got_glitch;
    int glitch_data;
    uint64_t last_got_data;    // time of last data
    int sync;                   //!< TRUE if we are doing synchronous data transfer
    int nreq_count;             //!< number of nreqs seen since data start
    int nack_count;             //!< number of nacks seen since data start
    capture *data_start;             //!< capture record for start of data

    /** detect glitches in the nreq and nack lines. This is used in asynchronous
    data transfer mode to ensure that nreq and nack operate in lock-step and there
    are no glitches in either */
    int detect_glitches;

    /** we have the debug pins in the trace */
    int debug_pins;

    /** show only transactions relating to this device */
    int active_device;

    /** Write a simple summary to this file - used to create test cases */
    FILE *summary;

} scsi_info;



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
    {"Erase", 0x19},
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


static const char *scsi_phase (int phase)
{
    if (phase >= 0 && phase < 8)
        return scsi_phases [phase];
    return "-";
}


static void decode_scsi_command (FILE *summary, int phase, unsigned char *buf, int buffer_len, int last_phase_command)
{
    switch (phase)
    {
	case 1: // DATA IN
	{
	    switch (last_phase_command) // work out what the last command is, since this is its response data
	    {
		case 0x12: // inquiry response data
		    printf ("\tInquiry response data\n");
		    break;

                case 0x03: // request sense - page 123, 8.2.14, Table 65
                {
                    //unsigned int v, len;
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
            int ctrl = buf[buffer_len - 1];
            if (summary) {
                int i;
                fprintf(summary, "Command: %s [",
                            scsi_command_name(cmd));
                for (i = 0; i < buffer_len; i++)
                    fprintf(summary, "%2.2x%c",
                        buf[i], i == buffer_len - 1 ? ']' : ' ');
                fprintf(summary, "\n");
            }
	    printf ("\t%s [%d 0x%x] %s\n", scsi_command_name (cmd), cmd, cmd,
                    (ctrl & 0x01) ? "linked" : "");
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
                    if (code == 0)
                        printf("\tcode=block\n");
                    else if (code == 1)
                        printf("\tcode=filemark\n");
                    else if (code == 2)
                        printf("\tcode=sequential filemark\n");
                    else if (code == 3)
                        printf("\tcode=eod\n");
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
                    case 0x0a: printf ("\tLinked command complete\n"); break;
                    default: printf ("\tUnknown command: 0x%x [%d]\n", cmd, cmd);
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



static int get_data (scsi_info *scsi, capture *c)
{
    int retval = 0;
    int i;
    int bit;
    int par;
//     channel_info *d0;

    for (i = 0; i < 8; i++)
    {
	bit = capture_bit (c, scsi->data [i]) ? 0 : 1; // invert the logic
	retval |= bit << i;
    }

    par = capture_bit (c, scsi->data [8]);

    if (par != parity (retval) && option_set("parity-check")) {
        int nio = capture_bit(c, scsi->nio);
	time_log (c, "parity error: 0x%x (par = %d, not %d nio=%d)\n",
                retval, par, parity (retval), nio);
    }

    return retval;
}

#if 0
static int ffs (unsigned int val)
{
    int i;
    for (i = 31; i >= 0; i--)
        if (val & (1 << i))
            return i;
    return -1;
}
#endif

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


enum
{
    /** states are described in terms of what we are doing. For example 'requesting' means
    that we have sent a request for data and are waiting for a response. In this state
    we have asserted nreq so we are continuing to request data, so we are in the 'requesting'
    state */
    STATE_requesting,       //!< we have asserted nreq and are awaiting an nack
    STATE_acknowledging,    //!< we have seen an nack and need to de-assert nreq
    STATE_completing,       //!< we have de-asserted nreq and are waiting de-assert of nack
    STATE_idle              //!< transaction is complete and we are idle

};


void scsi_init (scsi_info *scsi, list_t *channels)
{
    char name [20];
    int i;

    scsi->channels = channels;
    scsi->nbsy = capture_channel_details ("bsy", channels);
    scsi->nack = capture_channel_details ("ack", channels);
    scsi->nreq = capture_channel_details ("req", channels);
    scsi->nsel = capture_channel_details ("sel", channels);
    scsi->nrst = capture_channel_details ("rst", channels);
    scsi->nio = capture_channel_details ("io", channels);
    scsi->nmsg = capture_channel_details ("MSG", channels);
    scsi->ncd = capture_channel_details ("CD", channels);
    scsi->natn = capture_channel_details ("atn", channels);

    if (scsi->debug_pins) for (i = 0; i < 4; i++)
    {
        char dbgbuffer[10];
        sprintf (dbgbuffer, "dbg%d", i);
        scsi->dbg[i] = capture_channel_details (dbgbuffer, channels);
    }

    char *base = NULL;

    // automatically determine data names
    if (capture_channel_details("d<0>", channels))
        base = "";
    else if (capture_channel_details("db<0>", channels))
        base = "b";
    else
        fprintf (stderr, "Unable to determine base for data channels\n");

    for (i = 0; i < 8; i++)
    {
        sprintf (name, "d%s<%d>", base, i);
        //bit = capture_bit_name (c, name, channels) ? 1 : 0; // normal logic
        scsi->data [i] = capture_channel_details (name, channels);
    }
    sprintf(name, "d%sp", base);
    scsi->data [8] = capture_channel_details (name, channels);

    /* For positive logic, they are the reverse of single ended,
     * use this for differential & direct probing of
     * mictors on gurnard */
    if (option_set("positive")) {
        scsi->nbsy->inverted = 1;
        scsi->nack->inverted = 1;
        scsi->nreq->inverted = 1;
        scsi->nsel->inverted = 1;
        scsi->nrst->inverted = 1;
        scsi->nio->inverted = 1;
        scsi->nmsg->inverted = 1;
        scsi->ncd->inverted = 1;
        scsi->natn->inverted = 1;
        for (i = 0; i < 9; i++)
            scsi->data[i]->inverted = 1;
    }


    scsi->last_phase = -1;
    scsi->current_devices [0] = -1;
    scsi->current_devices [1] = -1;
    scsi->last_phase_command = -1;
    scsi->data_state = STATE_idle;
    scsi->waiting_for_idle = 1;
    scsi->last_valid_phase = -1;
    scsi->longest_req_glitch = 0;
    scsi->time_unreqed = -1;
    scsi->longest_unreq_glitch = 0;
    scsi->got_glitch = 0;
    scsi->last_got_data = -1;
    scsi->sync = 0;
}


static void check_phase (scsi_info *scsi, capture *c)
{
    int phase_changed = 0;
    int phase = scsi->last_phase;

    if (scsi->last_phase_capture)
    {
        long long elapsed = capture_time (c) - capture_time (scsi->last_phase_capture);
//         time_log (c, "capture change elapsed %lld\n", elapsed);
        if (elapsed > 100)
            phase_changed = 1;
    }
    if (phase_changed) {
//         time_log (c, "phase changed to %s [%d], was %s [%d], last valid %s [%d]\n",
//                   scsi_phase (phase), phase, scsi_phase (last_phase), last_phase,
//                               scsi_phase (last_valid_phase), last_valid_phase);

            /* report time when data capture started rather than current time c
        so that the user doesn't get confused */
        if (scsi->last_valid_phase == 2)
            decode_scsi_command (scsi->summary, scsi->last_valid_phase, scsi->buffer, scsi->buffer_len, scsi->last_phase_command);
        if (scsi->detect_glitches && (scsi->longest_req_glitch || scsi->longest_unreq_glitch))
            time_log (c, "longest_req_glitch = %d, longest_unreq_glitch = %d\n",
                        scsi->longest_req_glitch, scsi->longest_unreq_glitch);
        if (scsi->buffer_len)
        {
            long long elapsed = capture_time (c) - capture_time (scsi->data_start);
            double rate = scsi->buffer_len / ((double)elapsed / 1e9) / 1e6;

            if (scsi->buffer_len >= 1024)
                time_log (c, "size = %1.1lf KB, rate=%1.1lf MB/s\n",
                        scsi->buffer_len / 1024.0, rate);
            if (scsi->sync)
                time_log (c, "counts: nreq=0x%x, nack=0x%x\n",
                    scsi->nreq_count, scsi->nack_count);
            display_data_buffer (scsi->buffer, scsi->buffer_len, DISP_FLAG_both);
            // print data in
            if (scsi->summary &&
                (scsi->last_valid_phase == 1 || scsi->last_valid_phase == 0)) {
                int i;
                fprintf(scsi->summary, "%d %s of unit\n", scsi->buffer_len,
                scsi->last_valid_phase ? "out" : "in");
                for (i = 0; i < scsi->buffer_len; i++)
                    fprintf(scsi->summary, "%2.2x%s", scsi->buffer[i],
                        (i % 16) == 15 ? "\n" : " ");
                fprintf(scsi->summary, "\n");
            }
        }
        time_log (scsi->last_phase_capture, "Phase: %s (%d atn=%d)\n",
                  scsi_phases [scsi->last_phase], scsi->last_phase,
                  capture_bit_name(c, "atn", scsi->channels) ? 1 : 0);
        if (scsi->data_state != STATE_idle)
            time_log (c, "** Phase changed when data_state = %d, buffer=0x%x\n", scsi->data_state, scsi->buffer_len);
        scsi->buffer_len = 0;
        scsi->last_valid_phase = phase;
        scsi->last_phase_capture = NULL;
        scsi->longest_req_glitch = 0;
        scsi->longest_unreq_glitch = 0;
        scsi->sync = 0;
        scsi->nreq_count = scsi->nack_count = 0;
    }
}


static int handle_select (scsi_info *scsi, capture *c)
{
    capture *prev = scsi->prev;
    int i;

    // rising edge of nbsy, with nsel high => bus free
    if (capture_bit_transition (c, prev, scsi->nbsy, TRANSITION_low_to_high) &&
        capture_bit (c, scsi->nsel)) {
        if (scsi->current_devices[0] != -1 && scsi->current_devices[1] != -1) {
            time_log (c, "Bus free\n");
            scsi->current_devices[0] = -1;
            scsi->current_devices[1] = -1;
        }
        scsi->last_phase = -1;
        scsi->last_phase_capture = NULL;
        if (scsi->data_state != STATE_idle)
            time_log (c, "** Bus free when data_state = %d, buffer=0x%x\n", scsi->data_state, scsi->buffer_len);
        scsi->data_state = STATE_idle;
        }

    // nSel going low while busy is low impliese the end of arbitration
    if (option_set("verbose"))
        if (!capture_bit(c, scsi->nbsy) &&
                capture_bit_transition(c, prev, scsi->nsel, TRANSITION_high_to_low)) {
            int arb = get_data(scsi, c) ^ 0xff;
            time_log (c, "Arbitrator: 0x%x\n", arb);
        }

    if (capture_bit_transition(c, prev, scsi->nbsy, TRANSITION_low_to_high) &&
        !capture_bit(c, scsi->nsel)) {
        int ch = get_data (scsi, c);
        int dev1, dev2;
        dev1 = ffs(ch) -1;
        dev2 = ffs(ch & ~(1 << dev1)) -1;
        //time_log (c, "B Selection: 0x%x\n", ch);
        if (dev1 != scsi->current_devices[0] || dev2 != scsi->current_devices[1]) {
            scsi->current_devices[0] = dev1;
            scsi->current_devices[1] = dev2;

            if (dev1 < 0 || dev2 < 0)
                time_log (c, "Invalid device selection: 0x%2.2x\n", ch);
            else if (ch & ~(1 << dev1 | 1 << dev2))
                time_log (c, "More than 2 devices selected in 0x%2.2x\n", ch);
            else
                time_log (c, "Selected device: 0x%2.2x (dev1: %d dev2: %d active: %d)\n",
                        ch, dev1, dev2, scsi->active_device);
        }
    }

    if (capture_bit (c, scsi->nrst) != capture_bit (prev, scsi->nrst))
        time_log (c, "Bus reset %s\n", capture_bit (c, scsi->nrst) ? "end" : "start");

    if (scsi->debug_pins) for (i = 0; i < 4; i++)
    {
        if (capture_bit (c, scsi->dbg[i]) != capture_bit (prev, scsi->dbg[i]))
            time_log (c, "DBG %d change: %d\n", i, capture_bit (c, scsi->dbg[i]));
    }

    return (scsi->active_device == -1)
        || (scsi->current_devices[0] == scsi->active_device)
        || (scsi->current_devices[1] == scsi->active_device);
}


#if 1 // new state-machine based approach

/** handle data capture

    \returns TRUE if data to be captured is present, FALSE if not */

static capture *handle_data (scsi_info *scsi, capture *c)
{
    int nio = capture_bit (c, scsi->nio);
    capture *got_data = NULL;
    capture *prev = scsi->prev;
    int nreq, nack;

    nreq = capture_bit_change(c, prev, scsi->nreq);
    nack = capture_bit_change(c, prev, scsi->nack);
//     time_log (c, "nreq = %d [0x%x], nack = %d [0x%x]\n", nreq, scsi->nreq_count, nack, scsi->nack_count);

    // handle synchronous data transfers
    if (scsi->sync)
    {
        // have we received data
        if (nio
            ? nack == TRANSITION_high_to_low  // to target
            : nreq == TRANSITION_high_to_low) // from target
            got_data = c;
        scsi->nreq_count += nreq == TRANSITION_high_to_low;
        scsi->nack_count += nack == TRANSITION_high_to_low;
//         time_log (c, "   now: nreq = %d [0x%x], nack = %d [0x%x]\n", nreq, scsi->nreq_count, nack, scsi->nack_count);
    }
    else switch (scsi->data_state)
    {
        case STATE_idle :
            /* this is the starting state. We await nreq going low to indicate the start
            of an async data transaction */
            if (nreq == TRANSITION_high_to_low)
            {
                scsi->data_state = STATE_requesting;
                scsi->time_reqed = c;
            }
            if (scsi->detect_glitches)
            {
/*                if (nack == TRANSITION_high_to_low)
                    time_log(c, "nack low when data_state = %d, buffer pos %d\n", data_state, buffer_len);*/
                if (nreq == TRANSITION_low_to_high)
                    time_log(c, "nreq high when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
                if (nack == TRANSITION_low_to_high)
                    time_log(c, "nack high when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
            }
            break;

        case STATE_requesting :
            if (nack == TRANSITION_high_to_low)
            {
                // host receives data byte at this point
                scsi->data_state = STATE_acknowledging;
                if (!nio)
                    got_data = c;
            }
            if (nreq == TRANSITION_low_to_high)
            {
                /* this means nreq has gone low and high without nack changing.
                   Probably this is a synchronous transfer */
                scsi->sync = 1;
                time_log (c, "syncronous data detected, buffer_len = %d\n", scsi->buffer_len);

                // count the transition we have already seen
                scsi->nreq_count++;

                // reset the state
                scsi->data_state = STATE_idle;

                // nio TRUE for transfers from host to target
                // transfers from target read on falling edge of nreq
                got_data = nio ? NULL : scsi->time_reqed;
                if (scsi->detect_glitches)
                    ;
//                     time_log(c, "unreq when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
            }
            if (scsi->detect_glitches)
            {
                if (nreq == TRANSITION_high_to_low)
                    time_log(c, "req when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
                if (nack == TRANSITION_low_to_high)
                    time_log(c, "unack when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
            }
            break;

        case STATE_acknowledging :
            if (nreq == TRANSITION_high_to_low && scsi->detect_glitches)
                time_log(c, "req when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
//             if (nack == TRANSITION_high_to_low)
//                 time_log(c, "ack when data_state = %d, buffer pos %d\n", data_state, buffer_len);
            // target receives data byte at this point
            if (nreq == TRANSITION_low_to_high)
            {
                scsi->data_state = STATE_completing;
                scsi->time_unreqed = capture_time (c);
                if (nio)
                    got_data = c;
                if (scsi->detect_glitches && scsi->got_glitch
                    && scsi->glitch_data != get_data (scsi, c))
                    time_log (c, "glitch with different data 0x%x vs. 0x%x\n",
                              scsi->glitch_data, get_data (scsi, c));
                scsi->got_glitch = 0;
            }
            if (nack == TRANSITION_low_to_high)
                scsi->data_state = STATE_requesting;   // glitch
            break;

        case STATE_completing:
            if (nreq == TRANSITION_high_to_low)
            {
                int len = capture_time (c) - capture_time (scsi->time_reqed);
                scsi->data_state = STATE_acknowledging;  // it was just a glitch
                if (scsi->detect_glitches)
                    time_log (c, "un-nreq glitch length %d\n", len);
                if (len > scsi->longest_unreq_glitch)
                    scsi->longest_unreq_glitch = len;
            }
//             if (nack == TRANSITION_high_to_low)
//                 time_log(c, "ack when data_state = %d, buffer pos %d\n", data_state, buffer_len);
            if (nreq == TRANSITION_low_to_high && scsi->detect_glitches)
                time_log(c, "unreq when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
            if (nack == TRANSITION_low_to_high)
                scsi->data_state = STATE_idle;
            break;
    }
    return got_data;
}


#else

/** handle data capture

    \returns TRUE if data to be captured is present, FALSE if not */

static int handle_data (scsi_info *scsi, capture *c)
{
    int nio = capture_bit (c, scsi->nio);
    int got_data = 0;
    capture *prev = scsi->prev;

    if (capture_bit_transition(c, prev, scsi->nreq, TRANSITION_high_to_low))
    {
        if (scsi->data_state == STATE_completing)
        {
            int len = capture_time (c) - capture_time (scsi->time_reqed);
            scsi->data_state = STATE_idle;  // it was just a glitch
            if (scsi->detect_glitches)
                time_log (c, "un-nreq glitch length %d\n", len);
            if (len > scsi->longest_unreq_glitch)
                scsi->longest_unreq_glitch = len;
        }
        else if (scsi->data_state != STATE_idle)
        {
            if (scsi->detect_glitches)
                time_log(c, "req when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
        }
        else
        {
            scsi->data_state = STATE_requesting;
            scsi->time_reqed = c;
        }
    }
    else if (capture_bit_transition(c, prev, scsi->nack, TRANSITION_high_to_low))
    {
    //             if (data_state == STATE_idle)
    //                 data_state = STATE_completing;
        if (scsi->data_state != STATE_requesting)
            ; //time_log(c, "ack when data_state = %d, buffer pos %d\n", data_state, buffer_len);
        else
        {
            // host receives data byte at this point
            scsi->data_state = STATE_acknowledging;
            if (!nio)
                got_data = 1;
        }
    }
    else if (capture_bit_transition(c, prev, scsi->nreq, TRANSITION_low_to_high))
    {
        if (scsi->data_state == STATE_requesting)
        {
            int len = capture_time (c) - capture_time(scsi->time_reqed);

            scsi->data_state = STATE_idle;  // it was just a glitch
            if (scsi->detect_glitches)
                time_log (c, "nreq glitch length %d\n", len);
            if (len > scsi->longest_req_glitch)
                scsi->longest_req_glitch = len;
            scsi->got_glitch = 1;
            scsi->glitch_data = get_data (scsi, c);
        }
        else if (scsi->data_state != STATE_acknowledging)
        {
            if (scsi->detect_glitches)
                time_log(c, "unreq when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
        }
        else
        {
            // target receives data byte at this point
            scsi->data_state = STATE_completing;
            scsi->time_unreqed = capture_time (c);
            if (nio)
                got_data = 1;
            if (scsi->detect_glitches && scsi->got_glitch
                && scsi->glitch_data != get_data (scsi, c))
                time_log (c, "glitch with different data 0x%x vs. 0x%x\n",
                          scsi->glitch_data, get_data (scsi, c));
            scsi->got_glitch = 0;
        }
    }
    else if (capture_bit_transition(c, prev, scsi->nack, TRANSITION_low_to_high))
    {
        if (scsi->data_state == STATE_acknowledging)
            scsi->data_state = STATE_requesting;   // glitch
        else if (scsi->data_state != STATE_completing)
        {
            if (scsi->detect_glitches)
                time_log(c, "unack when data_state = %d, buffer pos %d\n", scsi->data_state, scsi->buffer_len);
        }
        else
            scsi->data_state = STATE_idle;
    }
    return got_data;
}

#endif


/**

    \param last_cap     TRUE if this is the last sample in the file, FALSE otherwise */

static void parse_scsi_cap (scsi_info *scsi, capture *c, list_t *channels, int last_cap)
{
    capture *prev = scsi->prev;
    capture *cdata;
    int phase;
    //printf("waiting: %d\n", scsi->waiting_for_idle);

    if (scsi->waiting_for_idle)
    {
        // wait until we see all these lines high before starting
        if (capture_bit (c, scsi->nsel) &&
            capture_bit (c, scsi->nbsy) &&
            capture_bit (c, scsi->natn) &&
            capture_bit (c, scsi->nmsg) &&
            capture_bit (c, scsi->nio) &&
            capture_bit (c, scsi->ncd))
            goto out;
        scsi->waiting_for_idle = 0;
    }

     //time_log (c, "nbsy=%d, data=%x, \n", capture_bit (c, scsi->nbsy), get_data (scsi, c));
    check_phase (scsi, c);

    // do select / arbitration processing - returns 0 if this transaction is not for us
    if (!handle_select (scsi, c))
        return;

    if (!capture_bit (c, scsi->nbsy)) {
        /* Decode the phase signals - see Table 8, Pg 45 of spec */
        phase = (capture_bit (c, scsi->nmsg) << 2)
                | (capture_bit (c, scsi->ncd) << 1)
                | (capture_bit (c, scsi->nio) << 0);
        phase = (~phase) & 0x7; // invert, since signals are negative logic

        if (phase != scsi->last_phase)
        {
            scsi->last_phase_capture = c;
            scsi->last_phase = phase;
//             time_log (c, "phase change %s [%d]\n", scsi_phase (phase), phase);
        }

        if (cdata = handle_data (scsi, c), cdata)
        {
            int ch;

//             time_log(c, "data_state = %d\n", data_state);
            scsi->last_got_data = capture_time(cdata);

            ch = get_data (scsi, cdata);
//             time_log (c, "Got data: 0x%x [nio=%d]\n", ch, nio);
            if (scsi->last_valid_phase == 2 && scsi->buffer_len == 0) // COMMAND phase, first byte, so record the command
                scsi->last_phase_command = ch;

            if (scsi->buffer_len == 0 && !scsi->sync)
            {
                time_log (cdata, "Data start\n");
                scsi->data_start = c;
            }
            if (scsi->last_valid_phase == 1 && scsi->last_phase_command == 8) // read command, data_in
                add_output_data (ch);
            scsi->buffer [scsi->buffer_len] = ch;
            scsi->buffer_len++;
            if (scsi->buffer_len > sizeof (scsi->buffer))
            {
                fprintf (stderr, "ARGH BUFFER OVERUN\n");
                abort ();
            }
        }
    }
#if 0
    // bsy went high, end of transaction
    if (((last_phase != -1
         && (capture_bit_transition (c, prev, scsi->nbsy, TRANSITION_low_to_high) || last_cap)) )
         || capture_bit (c, scsi->nrst) != capture_bit (prev, scsi->nrst))
    {
        if (last_cap)
            time_log(c, "CAPTURE CUT SHORT ON PHASE\n");
        else if (capture_bit (c, scsi->nrst) != capture_bit (prev, scsi->nrst))
            time_log(c, "DUMPING DATA BEFORE RESET\n");
//         else
//             time_log(c, "PREMATURE PHASE END\n");
	time_log (last_phase_capture, "Phase: %s (%d) (nbsy)\n", scsi_phases[last_phase], last_phase);
	decode_scsi_command (last_phase, buffer, buffer_len, last_phase_command);
//         if (buffer_len != req_ack_count)
//             time_log(c, "req/ack mismatch: %d vs %d\n", buffer_len, req_ack_count);
	display_data_buffer (buffer, buffer_len, DISP_FLAG_both);
	buffer_len = 0;
//         req_ack_count = 0;
	last_phase = -1;
        last_phase_capture = NULL;
    }
#endif
#if 0
    // when we start up, the phase that we are in is meaningless since we were not around for the whole thing
    if (phase != last_phase)
    {
        last_phase = phase;
        last_phase_capture = c;
    }
#endif
out:
    prev = c;
}

void parse_scsi (bulk_capture *b, char *filename, list_t *channels)
{
    scsi_info scsi;
    int i;
    capture *c;
    char buffer[100];

    printf ("SCSI analysis of file: '%s' (%d samples)\n", filename,
            b->length / sizeof (capture));

    printf ("All times are in us (microseconds)\n");

    memset (&scsi, '\0', sizeof (scsi));
    scsi.active_device = -1;

    if (option_val ("device", buffer, 10))
        scsi.active_device = strtoul (buffer, NULL, 0);
    printf("device: %d\n", scsi.active_device);
    if (option_set ("glitch"))
        scsi.detect_glitches = 1;

    if (option_set ("debug"))
        scsi.debug_pins = 1;

    if (option_val ("summary", buffer, 100)) {
        printf("Saving log to %s\n", buffer);
        scsi.summary = fopen(buffer, "wb");
    }

    c = b->data;

    scsi_init (&scsi, channels);
//     scsi->first_capture = c;
    scsi.prev = c;
    for (i = 1, c++; i < b->length / sizeof (capture); i++, c++)
    {
	parse_scsi_cap (&scsi, c, channels, i == (b->length / sizeof (capture)) - 1);
        scsi.prev = c;
    }

    if (scsi.summary)
        fclose(scsi.summary);
}
