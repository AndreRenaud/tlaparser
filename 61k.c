#include <stdio.h>

#include "dumpdata.h"
#include "common.h"

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
    static int last_dat = 0;
    static int last_cmd = -1;

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

    if (capture_bit_transition (c, prev, pa.data_req, TRANSITION_high_to_low))
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
	//time_log (c, "Got byte: 0x%x\n", ch);
    }

    if (capture_bit_transition (c, prev, pa.cntrl_busy, TRANSITION_high_to_low) && buffer_pos > 0)
    {
	time_log (c, "Data transfer end: %d\n", buffer_pos);
	display_data_buffer (buffer, buffer_pos, 0);
	buffer_pos = -1;
	last_dat = 0;
    }

#if 0
    if (capture_bit (c, pa.cntrl_busy) != capture_bit (prev, pa.cntrl_busy))
	time_log (c, "cntrl_busy changed to %d (%d)\n", capture_bit (c, pa.cntrl_busy), buffer_pos);
#endif

#if 0
    if (capture_bit (c, pa.last_dat) != capture_bit (prev, pa.last_dat))
	time_log (c, "last_dat changed to %d (%d)\n", capture_bit (c, pa.last_dat), buffer_pos);
#endif

    if (buffer_pos > 0 && capture_bit_transition (c, prev, pa.last_dat, TRANSITION_low_to_high))
    //if (capture_bit_transition (c, prev, pa.last_dat, TRANSITION_high_to_low))
	last_dat = 1;

#if 0
    {
	uint8_t data = get_data (c, channels);
	printf ("data: 0x%x (%s)\n", data, command_name (fixup (data)));
    }
#endif

done:
    prev = c;
}

static void parse_61k_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c;

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_61k_cap (c, channels);
	c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));
}

void parse_61k (list_t *cap, char *filename, list_t *channels)
{
    list_t *n;
    int i;

    printf ("61K analysis of file: '%s'\n", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_61k_bulk_cap (n->data, channels);
    }
}
