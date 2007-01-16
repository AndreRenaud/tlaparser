#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
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
extern list_t *final_capture;
extern list_t *final_channels;

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
		     "\t-o options  : List of comma separated options (ie: option1,option2=foo,option3)\n"
	    );
}

static list_t *load_capture (char *filename)
{
    list_t *cap;
    off_t len;
    char *buf;

    
    printf ("About to load %s\n", filename);

    yyin = fopen (filename, "r");
    if (!yyin)
    {
	fprintf (stderr, "Unable to open '%s': %s\n", filename, strerror (errno));
	return NULL;
    }
    fseeko (yyin, 0, SEEK_END);
    len = ftello (yyin);
    fseeko (yyin, 0, SEEK_SET);
    if (len >= MAX_DATA_LEN)
    {
	fclose (yyin);
	fprintf (stderr, "File too large: %s - increase MAX_DATA_LEN\n", filename);
	return NULL;
    }
    buf = malloc (1 * 1024 * 1024);

    setvbuf (yyin, buf, _IOFBF, 1 * 1024 * 1024);

    yyparse (NULL);
    cap = final_capture;
    fclose (yyin);
    free (buf);

    return cap;
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
	dump_channel_list (final_channels);

    if (dump)
    {
	if (cap1)
	    dump_capture_list (cap1, file1, final_channels);
	if (cap2)
	    dump_capture_list (cap2, file2, final_channels);
    }

#ifdef PARSE_SCSI
    if (scsi)
    {
	if (cap1)
	    parse_scsi (cap1, file1, final_channels);
	if (cap2)
	    parse_scsi (cap2, file2, final_channels);
    }
#endif

#ifdef PARSE_XD
    if (xd)
    {
	if (cap1)
	    parse_xd (cap1, file1, final_channels);
    }
#endif

#ifdef PARSE_PERTEC
    if (pertec)
	if (cap1)
	    parse_pertec (cap1, file1, final_channels);
#endif

    return 0;
}
