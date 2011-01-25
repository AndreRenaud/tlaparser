#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

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
    char *code_name;
    char *description;
};

struct parser_info parsers[] = {
    {parse_61k,		"61k",		"61K tape drive"},
    {parse_8250,	"8250",		"8250 uart bus"},
    {parse_kennedy,	"kennedy",	"Kennedy tape drive bus"},
    {parse_pertec,	"pertec",	"Pertec tape drive bus"},
    {parse_unformatted, "unformatted",  "Cook Unformatted tape drive bus"},
    {parse_half_formatted, "halfformatted",  "Cook Half formatted tape drive bus"},
    {parse_scsi,	"scsi",		"SCSI bus"},
    {parse_xd,		"xd",		"xD/NAND data bus"},
    {parse_pci,		"pci",		"PCI bus"},
    {parse_spi,		"spi",		"SPI bus"},
    {parse_fetex,	"fetex",	"Low Level Fetex-150 MTU bus"},
    {parse_nor,		"nor",		"NOR flash"},
    {parse_dm9000,	"dm9000",	"DM9000"},
    {parse_camera, 	"camera",	"Camera"},
    {parse_ssc_audio,	"ssc_audio",	"SSC Audio"},
    {parse_ov3640,	"ov3640",	"ov3640 image sensor"},
};
#define NPARSERS (sizeof (parsers) / sizeof (parsers[0]))

static void usage (char *prog)
{
    int i;
    fprintf(stderr, "Usage: %s [options] tlafile\n", prog);
    fprintf(stderr,
	    "  -d, --dump             : Dump file contents\n"
	    "  -l, --list-channels    : Dump channel names\n"
	    "  -b, --bits             : Dump changing bits\n"
	    "  -o, --options=OPTIONS  : List of comma separated options (ie: opt1,opt2=foo,opt3)\n"
	    "  -p, --parser=PARSER    : Parser to use\n"
	    );

    printf("\nAvailable parsers:\n");
    for (i = 0; i < NPARSERS; i++)
	printf ("  %-10s             : %s\n",
		parsers[i].code_name, parsers[i].description);
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
    const char *short_options = "dblo:p:";
    const struct option long_options[] = {
	{"dump",		no_argument,		NULL,	'd'},
	{"bits",		no_argument,		NULL,	'b'},
	{"list-channels",	no_argument,		NULL,	'l'},
	{"options",		required_argument,	NULL,	'o'},
	{"parser",		required_argument,	NULL,	'p'},
        {NULL,                  0,                      NULL,   0},
    };
    char *file, *parser = NULL;
    int dump = 0, list_channels = 0, changing = 0;
    bulk_capture *cap = NULL;
    int parse_func = -1;

    int i;

    while (1) {
	int o = getopt_long(argc, argv, short_options, long_options, NULL);
	if (o == -1)
	    break;
	switch (o) {
	case 'd': dump = 1; break;
	case 'b': changing = 1; break;
	case 'l': list_channels = 1; break;
	case 'o': options = optarg; break;
	case 'p': parser = optarg; break;
	default:
	    fprintf(stderr, "Unknown argument\n");
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }

    if (parse_func < 0) {
	if (!parser) {
	    fprintf(stderr, "No parser specified\n");
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
	for (i = 0; i < NPARSERS; i++) {
	    if (strcmp(parser, parsers[i].code_name) == 0) {
		parse_func = i;
		break;
	    }
	}
	if (parse_func < 0) {
	    fprintf(stderr, "Unknown parser '%s'\n", parser);
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
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
