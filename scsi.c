#include <stdio.h>

#include "dumpdata.h"
#include "common.h"

//  from pg 45 table 8 of scsi 2 spec, based on msg, c/d, i/o
static char *scsi_phases[] = { "DATA OUT", "DATA IN", "COMMAND", "STATUS", "*1", "*2", "MESSAGE OUT", "MESSAGE IN"};

// direct access commands

struct scsi_cmd
{
    char *name;
    int code;
};

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
    {"WRITE SAME", 0x41}
};
#define SCSI_COMMAND_LEN (sizeof (scsi_commands) / sizeof (scsi_commands[0]))

static uint8_t printable_char (int data)
{
    if (data >= ' ' && data <= 126)
	return data;
    return '.';
}

#if 0
static char ebcdic_to_ascii (unsigned char c)
{
    static char convert[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x09, 0x06, 0x7F, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x0A, 0x08, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x1C, 0x23, 0x24, 0x0A, 0x17, 0x1B, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x05, 0x06, 0x07,
	0x30, 0x31, 0x16, 0x0D, 0x34, 0x35, 0x36, 0x04, 0x38, 0x09, 0x0C, 0x3B, 0x14, 0x15, 0x3E, 0x1A,
	0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5, 0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
	0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF, 0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0x5E,
	0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5, 0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
	0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,               0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,
	0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,               0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0x5B, 0xDE, 0xAE,
	0xAC, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC, 0xBD, 0xBE, 0xDD, 0xA8, 0xAF, 0x5D, 0xB4, 0xD7,               0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,
	0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,               0x5C, 0xF7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0xFF};          return convert[c];
}
#endif

static void dump_buffer (unsigned char *buffer, int len)
{
    int i, j;

    if (!len)
	return;

    printf ("Data length: %d\n", len);
#define LINELEN 16
    for (i = 0; i < len; i+=LINELEN)
    {
	int this_len = min(LINELEN, len - i);
	printf ("\t");
	for (j = 0; j < this_len; j++)
	    printf ("%2.2x ", buffer[i + j]);

	for (j = 0; j < this_len; j++)
	    printf ("%c", printable_char (buffer[i + j]));
	printf ("\n");
    }
    printf ("\n");

}

static unsigned char flip (unsigned char ch)
{
    return ((ch & 0x80) >> 7) | ((ch & 0x40) >> 5) | ((ch & 0x20) >> 3) | ((ch & 0x10) >> 1) |
	    ((ch & 0x08) << 1) | ((ch & 0x04) << 3) | ((ch & 0x02) << 5) | ((ch & 0x01) << 7);
}

static char *scsi_command_name (int cmd)
{
    int i;
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
	    }
	    break;
	}

	case 2: // COMMAND
	{
	    int cmd = buf[0];
	    printf ("\t%s\n", scsi_command_name (cmd));
	    switch (cmd)
	    {
		case 0x08: // read
		    printf (" address=0x%2.2x%2.2x len=0x%x", buf[2], buf[3], buf[4]);
		    break;

		default:
		    break;
	    }
	    break;
	}

	case 6: // message out
	{
	    int cmd = buf[0];
	    if (cmd & 0x80)
		printf ("\tIdentify\n");
	    else
		printf ("\tUnknown command: %d\n", cmd);
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
	sprintf (name, "db%d", i);
	bit = capture_bit_name (c, name, channels) ? 0 : 1; // invert the logic
	retval |= bit << i;
    }

    par = capture_bit_name (c, "parity", channels);

    if (par != parity (retval))
	time_log (c, "parity error: 0x%x (par = %d, not %d)\n", retval, par, parity (retval));

    //printf ("get_data: %d\n", retval);

    return retval;
#if 0
#warning "Data probe hard coded to e0"
    return (~flip (c->data[3])) & 0xff;
#endif
}

static void parse_scsi_cap (capture *c, list_t *channels)
{
    static capture *prev = NULL;
    static int last_phase = -1;
    static unsigned char buffer[20];
    static int buffer_len = 0;
    static int current_device = -1;
    static int last_phase_command = -1;

    if (!prev)
	goto out;
 
    // SEL went low -> high, device selection 
    if (capture_bit_transition_name (c, prev, "nSEL", channels, TRANSITION_high_to_low))
    {
	int ch = get_data (c, channels);
	if (ch != current_device && !option_set ("device1") && !option_set("device2"))
	    time_log (c, "Selected device: 0x%2.2x\n", ch);
	current_device = ch;
    }

    if (option_set ("device1") && current_device != 1)
	return;

    if (option_set ("device2") && current_device != 2)
	return;

    // bsy is low, and nack goes from high to low
    if (!capture_bit_name (c, "nBSY", channels) && 
	capture_bit_transition_name (c, prev, "nACK", channels, TRANSITION_high_to_low))
    {
	int phase = 0;
	int ch;


	phase |= capture_bit_name (c, "nMSG", channels) << 2;
	phase |= capture_bit_name (c, "nCD", channels) << 1;
	phase |= capture_bit_name (c, "nIO", channels) << 0;
	phase = (~phase) & 0x7; // invert, since signals are negative logic

	if (phase != last_phase)
	{
	    decode_scsi_command (last_phase, buffer, last_phase_command);
	    dump_buffer (buffer, buffer_len);
	    buffer_len = 0;
	    time_log (c, "Phase: %s\n", scsi_phases[phase]);
	}
	ch = get_data (c, channels);
	if (phase == 2 && buffer_len == 0) // COMMAND phase, first byte, so record the command
	    last_phase_command = ch;

	buffer[buffer_len] = ch;
	buffer_len++;
	last_phase = phase;
    }
   
    // bsy went high, end of transaction 
    if (capture_bit_name (c, "nBSY", channels) && !capture_bit_name (prev, "nBSY", channels))
    {
	decode_scsi_command (last_phase, buffer, last_phase_command);
	dump_buffer (buffer, buffer_len);
	buffer_len = 0;
	last_phase = -1;
    }

out:
    prev = c;
}

static void parse_scsi_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c;

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_scsi_cap (c, channels);
	c++;
    }
}

void parse_scsi (list_t *cap, char *filename, list_t *channels)
{
    list_t *n;
    int i;

    printf ("SCSI analysis of file: '%s'\n", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_scsi_bulk_cap (n->data, channels);
    }

}
