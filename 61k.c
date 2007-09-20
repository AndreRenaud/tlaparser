#include <stdio.h>

#include "dumpdata.h"
#include "common.h"
#include "parser_prototypes.h"

enum
{
    CMD_SELECT = 0xe0,
    CMD_READ_FWD = 0x44,
    CMD_SPACE_FWD = 0x48,
    CMD_SPACE_REV = 0x4c,
    CMD_FILE_SEARCH_FWD = 0x50,
    CMD_FILE_SEARCH_REV = 0x54,
    CMD_WRITE = 0x24,
    CMD_ERASE = 0x28,
    CMD_WRITE_FILEMARK = 0x2c,
    CMD_REWIND = 0x1c,
    CMD_READ_STATUS0 = 0x60,
    CMD_READ_STATUS1 = 0x64,
    CMD_READ_STATUS2 = 0x68,
    CMD_READ_STATUS3 = 0x6c,
};

static uint8_t get_data (capture *c, list_t *channels)
{
    int retval = 0;
    int i;
    int bit;
    char name[10];

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "D%d", i);
	bit = capture_bit_name (c, name, channels) ? 1 : 0;
	retval |= bit << i;
    }
#warning "Not checking parity"

    return retval;
}

static const char *command_name (int cmd)
{
    switch (cmd)
    {
	case CMD_SELECT: return "Select";
	case CMD_READ_FWD: return "Read Fwd";
	case CMD_SPACE_FWD: return "Space Fwd";
	case CMD_SPACE_REV: return "Space Rev";
	case CMD_FILE_SEARCH_FWD: return "File Search Fwd";
	case CMD_FILE_SEARCH_REV: return "File Search Rev";
	case CMD_WRITE: return "Write";
	case CMD_ERASE: return "Erased Fixed Len";
	case CMD_WRITE_FILEMARK: return "Write Filemark";
	case CMD_REWIND: return "Rewind";
	case CMD_READ_STATUS0: return "Read Status0";
	case CMD_READ_STATUS1: return "Read Status1";
	case CMD_READ_STATUS2: return "Read Status2";
	case CMD_READ_STATUS3: return "Read Status3";

	default: return "Unknown";
    }
}

static int fixup (int data)
{
    data = (~data) & 0xff;

    return ((data & 0x80) >> 7) |
	   ((data & 0x40) >> 5) |
	   ((data & 0x20) >> 3) |
	   ((data & 0x10) >> 1) |
	   ((data & 0x08) << 1) |
	   ((data & 0x04) << 3) |
	   ((data & 0x02) << 5) |
	   ((data & 0x01) << 7);
}

struct pin_assignments
{
    int init;

    channel_info *data_req;
    channel_info *ack;
    channel_info *cmd;
    channel_info *last_dat;
    channel_info *cntrl_busy;
};

static void parse_61k_cap (capture *c, list_t *channels)
{
    static capture *prev = NULL;
    static struct pin_assignments pa = {-1};
    static unsigned char buffer[1024];
    static int buffer_pos = -1;
    static int last_cmd = -1;
    static uint64_t last_data_req = 0;

    if (pa.init == -1)
    {
	pa.init = 1;
	pa.data_req = capture_channel_details (c, "data_req", channels);
	pa.ack = capture_channel_details (c, "ack", channels);
	pa.cmd = capture_channel_details (c, "cmd", channels);
	pa.last_dat = capture_channel_details (c, "last_dat", channels);
	pa.cntrl_busy = capture_channel_details (c, "cntrl_busy", channels);
    }

    if (!prev) // need it to detect edges
	goto done;

    if (capture_bit_transition (c, prev, pa.cmd, TRANSITION_high_to_low))
    {
	int cmd = get_data (c, channels);
	last_cmd = cmd;
	time_log (c, "Command: %s (0x%x 0x%x)\n",
		command_name (fixup (cmd)), cmd, fixup (cmd));
    }

    if (capture_bit_transition (c, prev, pa.data_req, TRANSITION_high_to_low) && 
	(capture_time (c) - last_data_req) > 50) // at least 50ns between, or else it is jitter
    {
	uint8_t ch = get_data (c, channels);
	ch = fixup(ch);

	if (buffer_pos < 0)
	{
	    //time_log (c, "Data transfer start\n");
	    buffer_pos = 0;
	}
	buffer[buffer_pos] = ch;
	buffer_pos++;
	//time_log (c, "Got byte: 0x%x (%lld)\n", ch, capture_time (c));
	last_data_req = capture_time (c);
    }

    if (capture_bit_transition (c, prev, pa.cntrl_busy, TRANSITION_low_to_high) && buffer_pos > 0)
    {
	//time_log (c, "Data transfer end: %d\n", buffer_pos);
	display_data_buffer (buffer, buffer_pos, 0);
	buffer_pos = -1;
    }

    if (capture_bit (c, pa.cntrl_busy) != capture_bit (prev, pa.cntrl_busy))
	time_log (c, "Unit gone %s\n", !capture_bit (c, pa.cntrl_busy) ? "busy" : "free");

done:
    prev = c;
}

void parse_61k (bulk_capture *b, char *filename, list_t *channels)
{
    int i;
    capture *c;

    printf ("61K analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_61k_cap (c, channels);
	c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));
}
