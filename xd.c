#include <stdio.h>

#include "dumpdata.h"
#include "common.h"

#if 0 // normal
#define ALE_NAME "ale"
#define CLE_NAME "cle"
#define NCE_NAME "nce"
#define DATA_PREFIX "d"
#define NWE_NAME "nwe"
#define NRE_NAME "nre"
#elif 0 // marlin
#define ALE_NAME "ale"
#define CLE_NAME "cle"
#define NCE_NAME "cen"
#define DATA_PREFIX "md"
#define NWE_NAME "nfweb"
#define NRE_NAME "nfreb"
#elif 1 // snapper9260
#define ALE_NAME "ale"
#define CLE_NAME "cle"
#define NCE_NAME "ncs"
#define DATA_PREFIX "d"
#define NWE_NAME "nwe"
#define NRE_NAME "nre"
#endif


static uint8_t printable_char (int data)
{
    if (data >= ' ' && data <= 126)
	return data;
    return '.';
}

static uint8_t xd_data (capture *c, list_t *channels)
{
    uint8_t retval = 0;
    char name[100];
    int i;

    for (i = 0; i < 8; i++) {
        sprintf (name, "%s%d", DATA_PREFIX, i);
        retval |= (capture_bit_name (c, name, channels) ? 1 : 0) << i;
    }

    //printf ("data: 0x%x\n", retval);
    return retval;
}

static char *nand_command (unsigned int command)
{
    switch (command)
    {
	case 0x80:
	    return "Serial Data Input";
	case 0x00:
	    return "Read (1)";
	case 0x01:
	    return "Read (2)";
	case 0x50:
	    return "Read (3)";
	case 0xFF:
	    return "Reset";
	case 0x10:
	    return "True Page Program";
	case 0x11:
	    return "Dummy Page Program for Multi Block Programming";
	case 0x15:
	    return "Multi Block Program";
	case 0x60:
	    return "Block Erase (step 1)";
	case 0xD0:
	    return "Block Erase (step 2)";
	case 0x70:
	    return "Status Read (1)";
	case 0x71:
	    return "Status Read (2)";
	case 0x90:
	    return "ID Read (1)";
	case 0x91:
	    return "ID Read (2)";
	case 0x9A:
	    return "ID Read (3)";
        case 0x31:
            return "Page Read Cache Mode Start";
	default:
	    return "Unknown";
    }
}

static void parse_xd_cap (capture *c, capture *prev, list_t *channels)
{
    static unsigned char data_buffer[1024];
    static unsigned int data_len = 0;
    static int64_t address = -1;

    if (!prev) // need it to detect edges
	return;

#define DUMP_DATA() do {if (data_len) { \
                            if (address != -1) \
                                printf ("Address: 0x%llx (Block: 0x%llx, Page: 0x%llx, Byte: 0x%llx)\n", \
                                    address, address >> 22, (address >> 16) & 0x1f, (address & 0xffff)); \
                            display_data_buffer (data_buffer, data_len, 0);  \
                            data_len = 0; \
                            address = -1; \
                            } \
                     } while (0)

    if (!capture_bit_name (c, NCE_NAME, channels)) // we're accessing xD
    {
#if 0
	printf ("nce: %d %d %d\n", 
		capture_bit_name (c, ALE_NAME, channels),
		capture_bit_name (c, CLE_NAME, channels),
		capture_bit_name (c, NWE_NAME, channels));
#endif
	if (capture_bit_transition_name (c, prev, NWE_NAME, channels, TRANSITION_low_to_high)) {
	    uint8_t data = xd_data (c, channels);
            if (capture_bit_name (c, ALE_NAME, channels)) {
                DUMP_DATA();
                if (address == -1)
                    address = data;
                else
                    address = (address << 8) | data;
            } else if (capture_bit_name (c, CLE_NAME, channels)) {
                DUMP_DATA ();
                printf ("Command: %s (0x%x)\n", nand_command (data), data);
            } else {
                data_buffer[data_len++] = data;
            }
        }

	if (capture_bit_transition_name (c, prev, NRE_NAME, channels, TRANSITION_low_to_high))
	{
	    uint8_t data = xd_data (c, channels);
	    //printf ("read\n");
	    if (capture_bit_name (c, ALE_NAME, channels) || capture_bit_name (c, CLE_NAME,  channels))
		printf ("WARNING: READ WHILE IN ALE/CLE\n");

	    data_buffer[data_len] = data;
	    data_len++;
	}
    } else {
        DUMP_DATA ();
    }
}

void parse_xd (bulk_capture *b, char *filename, list_t *channels)
{
    int i;
    capture *c, *prev = NULL;

    printf ("xD analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_xd_cap (c, prev, channels);
	prev = c;
	c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));
}
