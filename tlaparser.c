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
#ifdef PARSE_8250
#include "8250.h"
#endif
#ifdef PARSE_61K
#include "61k.h"
#endif

extern FILE *yyin;
extern int yyparse (void *YYPARSE_PARAM);
extern list_t *final_capture;
extern list_t *final_channels;

static void usage (char *prog)
{
    fprintf (stderr, "Usage: %s [options] tlafile\n", prog);
    fprintf (stderr, "\t-d	    : Dump file contents\n"
		     "\t-l	    : Dump channel names\n"
		     "\t-c	    : Compare two files\n"
		     "\t-b          : Dump changing bits\n"
		     "\t-o options  : List of comma separated options (ie: option1,option2=foo,option3)\n"
#ifdef PARSE_SCSI
		     "\t-s	    : SCSI check\n"
#endif
#ifdef PARSE_XD
		     "\t-x	    : xD check\n"
#endif
#ifdef PARSE_PERTEC
		     "\t-p	    : pertec check\n"
#endif
#ifdef PARSE_8250
		     "\t-8          : 8250 check\n"
#endif
#ifdef PARSE_61K
		     "\t-k          : 61k check\n"
#endif
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
    int dump = 0, compare = 0, list_channels = 0, changing = 0;
#ifdef PARSE_SCSI
    int scsi = 0;
#endif
#ifdef PARSE_XD
    int xd = 0;
#endif
#ifdef PARSE_PERTEC
    int pertec = 0;
#endif
#ifdef PARSE_8250
    int p8250 = 0;
#endif
#ifdef PARSE_61K
    int p61k = 0;
#endif
    list_t *cap = NULL;

    while (1)
    {
	int o = getopt (argc, argv, "dblco:"
#ifdef PARSE_SCSI
		"s"
#endif
#ifdef PARSE_XD
		"x"
#endif
#ifdef PARSE_PERTEC
		"p"
#endif
#ifdef PARSE_8250
		"8"
#endif
#ifdef PARSE_61K
		"k"
#endif
		);
	if (o == -1)
	    break;
	switch (o)
	{
	    case 'd': dump = 1; break;
	    case 'b': changing = 1; break;
	    case 'l': list_channels = 1; break;
	    case 'c': compare = 1; break;
#ifdef PARSE_PERTEC
	    case 'p': pertec = 1; break;
#endif
#ifdef PARSE_8250
	    case '8': p8250 = 1; break;
#endif
#ifdef PARSE_61K
	    case 'k': p61k = 1; break;
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

    file = argv[optind];
    optind++;

    if (optind < argc)
    {
	usage (argv[0]);
	return (EXIT_FAILURE);
    }


    if (!dump && !compare && !file)
    {
	usage (argv[0]);
	return (EXIT_FAILURE);
    }

    cap = load_capture (file);

    if (list_channels)
	dump_channel_list (final_channels);

    if (changing)
	dump_changing_channels (cap, file, final_channels);

    if (dump)
	dump_capture_list (cap, file, final_channels);

#ifdef PARSE_SCSI
    if (scsi)
	parse_scsi (cap, file, final_channels);
#endif

#ifdef PARSE_XD
    if (xd)
	parse_xd (cap, file, final_channels);
#endif

#ifdef PARSE_PERTEC
    if (pertec)
	parse_pertec (cap, file, final_channels);
#endif

#ifdef PARSE_8250
    if (p8250)
	parse_8250 (cap, file, final_channels);
#endif

#ifdef PARSE_61K
    if (p61k)
	parse_61k (cap, file, final_channels);
#endif

    return 0;
}
