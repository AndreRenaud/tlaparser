#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "dumpdata.h"
#include "parser_prototypes.h"

extern FILE *yyin;
extern int yyparse (void *YYPARSE_PARAM);
extern int nprobes;
extern list_t *final_channels;

struct parser_info
{
    parser_function func;
    char code;
    char *description;
};

struct parser_info parsers[] = {
    {parse_61k, '6', "Parse 61K tape drive"},
    {parse_8250, '8', "Parse 8250 uart bus"},
    {parse_kennedy, 'k', "Parse Kennedy tape drive bus"},
    {parse_pertec, 'p', "Parse Pertec tape drive bus"},
    {parse_scsi, 's', "Parse SCSI bus"},
    {parse_xd, 'x', "Parse xD/NAND data bus"},
    {parse_pci, 'P', "Parse PCI bus"},
    {parse_spi, 'S', "Parse SPI bus"},
    {parse_nor, 'n', "Parse NOR flash"},
    {parse_dm9000, '9', "Parse DM9000"},
    {parse_camera, 'c', "Parse Camera"},
};
#define NPARSERS (sizeof (parsers) / sizeof (parsers[0]))

static void usage (char *prog)
{
    int i;
    fprintf (stderr, "Usage: %s [options] tlafile\n", prog);
    fprintf (stderr, "\t-d	    : Dump file contents\n"
		     "\t-l	    : Dump channel names\n"
		     "\t-b          : Dump changing bits\n"
		     "\t-o options  : List of comma separated options (ie: option1,option2=foo,option3)\n");
    for (i = 0; i < NPARSERS; i++)
	printf ("\t-%c	    : %s\n", parsers[i].code, parsers[i].description);
}

static char *options = NULL;

int option_set (char *name)
{
    return option_val (name, NULL, 0);
}

int option_val (char *name, char *buffer, int buff_len)
{
    char *s;
    int namelen = strlen (name);

    for (s = options; s != NULL; s = strchr (s, ','))
    {
	if (*s == ',')
	    s++;
	if (strncmp (s, name, namelen) == 0 &&
  	    strlen (s) >= namelen &&
	    (s[namelen] == '=' || s[namelen] == ',' || s[namelen] == '\0'))
	{
	    char *optval;
	    int optlen;
	    char *end;

	    if (s[namelen] == '\0' || s[namelen] == ',')
		optval = NULL;
	    else
		optval = &s[namelen + 1];

	    if (optval)
	    {
		end = strchr (optval, ',');

		if (end)
		    optlen = end - optval;
		else
		    optlen = strlen (optval);

		optlen = min (optlen, buff_len);

		if (buffer)
		{
		    memcpy (buffer, optval, optlen);
		    if (optlen < buff_len)
			buffer[optlen] = '\0';
		    else
			buffer[optlen - 1] = '\0';
		}
	    }
	    else if (buffer && optlen)
		*buffer = '\0';

	    return 1;
	}
    }
    return 0;
}



int main (int argc, char *argv[])
{
    char *file;
    int dump = 0, list_channels = 0, changing = 0;
    bulk_capture *cap = NULL;
    int parse_func = -1;
    char opt_strings[100] = "dblo:";
    int i;

    for (i = 0; i < NPARSERS; i++)
    {
	int len = strlen (opt_strings);
	opt_strings[len] = parsers[i].code;
	opt_strings[len + 1] = '\0';
    }

    while (1)
    {
	int o = getopt (argc, argv, opt_strings);
	if (o == -1)
	    break;
	switch (o)
	{
	    case 'd': dump = 1; break;
	    case 'b': changing = 1; break;
	    case 'l': list_channels = 1; break;
	    case 'o': options = optarg; break;
	    default:
		for (i = 0; i < NPARSERS; i++)
		    if (parsers[i].code == o)
		    {
			parse_func = i;
			break;
		    }

		if (i == NPARSERS)
		{
		    fprintf (stderr, "Unknown option: %d (%c)\n", o, o);
		    usage (argv[0]);
		    return (EXIT_FAILURE);
		}
	}
    }

    file = argv[optind];
    optind++;

    if (optind < argc)
    {
	usage (argv[0]);
	return (EXIT_FAILURE);
    }


    if (!dump && !file)
    {
	usage (argv[0]);
	return (EXIT_FAILURE);
    }

    //cap = load_capture (file);
    cap = tla_parse_file (file);

    if (list_channels)
	dump_channel_list (final_channels);

    if (!cap)
        return EXIT_FAILURE;

    if (changing)
	dump_changing_channels (cap, file, final_channels);

    if (dump)
	dump_capture (cap, file, final_channels);

    if (parse_func >= 0)
	parsers[parse_func].func (cap, file, final_channels);

    return 0;
}
