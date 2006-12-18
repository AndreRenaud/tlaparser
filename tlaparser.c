#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dumpdata.h"
#include "parser.h"
#ifdef PARSE_SCSI
#include "scsi.h"
#endif
#ifdef PARSE_XD
#include "xd.h"
#endif
#ifdef PARSE_PERTEC
#include "pertec.h"
#endif

extern FILE *yyin;
extern int yyparse (void *YYPARSE_PARAM);

static void usage (char *prog)
{
    fprintf (stderr, "Usage: %s [options]\n", prog);
    fprintf (stderr, "\t-1 filename : First file\n"
		     "\t-2 filename : Second file (optional)\n"
		     "\t-d	    : Dump file contents\n"
		     "\t-l	    : Dump channel names\n"
		     "\t-c	    : Compare two files\n"
#ifdef PARSE_SCSI
		     "\t-s	    : SCSI check\n"
#endif
#ifdef PARSE_XD
		     "\t-x	    : xD check\n"
#endif
#ifdef PARSE_PERTEC
		     "\t-p	    : pertec check\n"
#endif
		     "\t-o options  : List of comma separated options\n"
	    );
}

static list_t *load_capture (char *filename)
{
    list_t *cap;
    off_t len;
    
    printf ("About to load %s\n", filename);

    yyin = fopen (filename, "r");
    fseeko (yyin, 0, SEEK_END);
    len = ftello (yyin);
    fseeko (yyin, 0, SEEK_SET);
    if (len >= MAX_DATA_LEN)
    {
	fclose (yyin);
	fprintf (stderr, "File too large: %s - increase MAX_DATA_LEN\n", filename);
	return NULL;
    }

    yyparse (NULL);
    cap = datasets;
    fclose (yyin);

    return cap;
}

static char *options = NULL;

int option_set (char *name)
{
    char *s = options;
    int namelen = strlen (name);

    for (s = options; s != NULL; s = strchr (s, ','))
    {
	if (strncmp (s, name, namelen) == 0 &&
  	    strlen (s) >= namelen &&
	    (s[namelen] == ',' || s[namelen] == '\0'))
	    return 1;
    }
    return 0;
}

int main (int argc, char *argv[])
{
    char *file1 = NULL, *file2 = NULL;
    int dump = 0, compare = 0, list_channels = 0;
#ifdef PARSE_SCSI
    int scsi = 0;
#endif
#ifdef PARSE_XD
    int xd = 0;
#endif
#ifdef PARSE_PERTEC
    int pertec = 0;
#endif
    list_t *cap1 = NULL, *cap2 = NULL;

    while (1)
    {
	int o = getopt (argc, argv, "1:2:dlco:"
#ifdef PARSE_SCSI
		"s"
#endif
#ifdef PARSE_XD
		"x"
#endif
#ifdef PARSE_PERTEC
		"p"
#endif
		);
	if (o == -1)
	    break;
	switch (o)
	{
	    case '1': file1 = optarg; break;
	    case '2': file2 = optarg; break;
	    case 'd': dump = 1; break;
	    case 'l': list_channels = 1; break;
	    case 'c': compare = 1; break;
#ifdef PARSE_PERTEC
	    case 'p': pertec = 1; break;
#endif
#ifdef PARSE_SCSI
	    case 's': scsi = 1; break;
#endif
#ifdef PARSE_XD
	    case 'x': xd = 1; break;
#endif
	    case 'o': options = optarg; break;
	    default:
		fprintf (stderr, "Unknown option: %d (%c)\n", o, o);
		usage (argv[0]);
		return (EXIT_FAILURE);
	}
    }
    if (optind < argc)
    {
	usage (argv[0]);
	return (EXIT_FAILURE);
    }

    if (!dump && !compare && !file1)
    {
	usage (argv[0]);
	return (EXIT_FAILURE);
    }

    if (file1)
	cap1 = load_capture (file1);
    if (file2)
	cap2 = load_capture (file2);

#if 0 // for pertec really, drop for now
    if (compare && cap1 && cap2)
    {
	stream_info_t *s1, *s2;
	s1 = build_stream_info (cap1, file1);
	s2 = build_stream_info (cap2, file2);
	compare_streams (s1, s2);
    }
#endif

    if (list_channels)
	dump_channel_list (channels);

    if (dump)
    {
	if (cap1)
	    dump_capture_list (cap1, file1, channels);
	if (cap2)
	    dump_capture_list (cap2, file2, channels);
    }

#ifdef PARSE_SCSI
    if (scsi)
    {
	if (cap1)
	    parse_scsi (cap1, file1, channels);
	if (cap2)
	    parse_scsi (cap2, file2, channels);
    }
#endif

#ifdef PARSE_XD
    if (xd)
    {
	if (cap1)
	    parse_xd (cap1, file1, channels);
    }
#endif

#ifdef PARSE_PERTEC
    if (pertec)
	if (cap1)
	    parse_pertec (cap1, file1, channels);
#endif

    return 0;
}
