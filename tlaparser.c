#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dumpdata.h"
#include "parser.h"
#include "scsi.h"

extern FILE *yyin;
extern int yyparse (void *YYPARSE_PARAM);

static void usage (char *prog)
{
    fprintf (stderr, "Usage: %s [options]\n", prog);
    fprintf (stderr, "\t-1 filename : First file\n"
		     "\t-2 filename : Second file (optional)\n"
		     "\t-d	    : Dump file contents\n"
		     "\t-c	    : Compare two files\n"
		     "\t-s	    : SCSI check\n"
	    );
}

static list_t *load_capture (char *filename)
{
    list_t *cap;

    yyin = fopen (filename, "r");
    yyparse (NULL);
    cap = datasets;
    fclose (yyin);

    return cap;
}

int main (int argc, char *argv[])
{
    char *file1 = NULL, *file2 = NULL;
    int dump = 0, compare = 0, scsi = 0;
    list_t *cap1 = NULL, *cap2 = NULL;

    while (1)
    {
	int o = getopt (argc, argv, "1:2:dcs");
	if (o == -1)
	    break;
	switch (o)
	{
	    case '1': file1 = optarg; break;
	    case '2': file2 = optarg; break;
	    case 'd': dump = 1; break;
	    case 'c': compare = 1; break;
	    case 's': scsi = 1; break;
	    default:
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

    if (dump)
    {
	if (cap1)
	    dump_capture_list (cap1, file1, channels);
	if (cap2)
	    dump_capture_list (cap2, file2, channels);
    }

    if (scsi)
    {
	if (cap1)
	    parse_scsi (cap1, file1, channels);
	if (cap2)
	    parse_scsi (cap2, file2, channels);
    }

    return 0;
}
