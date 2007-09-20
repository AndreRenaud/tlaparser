#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <regex.h>

#include "dumpdata.h"

#define log(a,b...) printf (a "\n" , ##b)

list_t *final_channels = NULL;

static void extract_match (regmatch_t *match, char *buffer, char *result)
{
    int len = match->rm_eo - match->rm_so;
    strncpy (result, buffer + match->rm_so, len);
    result[len] = '\0';
}

static void add_group (char *name, char *members)
{
    log ("Found group: '%s' '%s'", name, members);
#warning "Not doing anything with the group information at this stage"
}

bulk_capture *tla_parse_file (char *file_name)
{
    regex_t channel, group_start, group_members, data_set, data_set_group, cafc_long_cell;
    char buffer[1024];
    char current_group[100];
    char current_data_set[100];
    regmatch_t pmatch[3];
    long here;
    bulk_capture *retval = NULL;
    FILE *fp;

    if ((fp = fopen (file_name, "rb")) == 0)
    {
        fprintf (stderr, "Unable to open '%s': %s\n", file_name, strerror (errno));
        return NULL;
    }

    /* All the fancy regexps to identify the relevant parts of a .tla file
     */
    regcomp (&channel, "CjmChannel \"([[:alnum:][:punct:]]+)\" \"\\$([[:alnum:][:punct:]]+)\\$\"", REG_EXTENDED);
    regcomp (&group_start, "CjmChannelGroup \"([[:alnum:][:punct:]]+)\" \"\\$([[:alnum:][:punct:]]+)\\$\"", REG_EXTENDED);
    regcomp (&group_members, "CafcStringCell \"ClaGroupDefinition\" \"\\$\\$\" = \\{ \"([[:alnum:][:punct:]]+)\"[[:space:]]*\\}", REG_EXTENDED);
    regcomp (&data_set, "CcmDataSet \"DaSetNormal\" \"\\$\\$\" = #([[:digit:]])", REG_EXTENDED);
    regcomp (&data_set_group, "CjmTimebaseData \"([[:alnum:][:punct:]]+)\" \"\\$\\$\"", REG_EXTENDED);
    regcomp (&cafc_long_cell, "CafcLongCell \"([[:alnum:][:punct:]]+)\" \"\\$\\$\" = \\{ ([[:digit:]]+)[[:space:]]+\\}", REG_EXTENDED);

    current_group[0] = '\0';

    while ((here = ftell (fp)) != -1 && 
           fgets (buffer, sizeof (buffer), fp) != NULL)
    {
        if (regexec (&channel, buffer, 3, pmatch, 0) == 0)
        {
            char probe[100];
            char channel[100];

            extract_match (&pmatch[1], buffer, probe);
            extract_match (&pmatch[2], buffer, channel);

            final_channels = list_prepend (final_channels, build_channel (probe, channel, 0)); // not detecting inversion
        }

        if (regexec (&group_start, buffer, 3, pmatch, 0) == 0)
            extract_match (&pmatch[2], buffer, current_group);

        if (regexec (&group_members, buffer, 3, pmatch, 0) == 0)
        {
            char group_members[1024];
            extract_match (&pmatch[1], buffer, group_members);
            add_group (current_group, group_members);
        }

        if (regexec (&data_set, buffer, 3, pmatch, 0) == 0)
        {
            if (strstr (current_data_set, "HiRes") != NULL)
                log ("Skipping data for dataset '%s', since it is High Res", current_data_set);
            else
            {
                int len_length, len;
                char length[20];
                char *data;

                len_length = buffer[pmatch[1].rm_so] - '0';

                fseek (fp, here + pmatch[1].rm_so + 1, SEEK_SET);
                fread (length, 1, len_length, fp);
                length[len_length] = '\0';
                len = atoi (length);

#if 0
                log ("Data set of length: %d (%s, %d) %s", 
                        len_length, length, len, current_data_set);
#endif

                data = malloc (len);
                fread (data, 1, len, fp);

                if (retval)
                    log ("Warning: Already found one dataset, so skipping this one");
                else
                    retval = build_dump (data, len);

                free (data);
            }
        }

        if (regexec (&data_set_group, buffer, 3, pmatch, 0) == 0)
        {
            extract_match (&pmatch[1], buffer, current_data_set);
            //log ("Current data set: %s", current_data_set);
        }

        if (regexec (&cafc_long_cell, buffer, 3, pmatch, 0) == 0)
        {
            char cell_name[100];
            char value_str[100];
            int value;

            extract_match (&pmatch[1], buffer, cell_name);
            extract_match (&pmatch[2], buffer, value_str);
            value = atoi (value_str);
            //log ("Cafccell: %s %d", cell_name, value);

#if 0
            if (strcmp (cell_name, "DaNumSamples") == 0)
                log ("Num samples for '%s' is %d", current_data_set, value);
            else if (strcmp (cell_name, "DaBytesPerSample") == 0)
                log ("Num samples for '%s' is %d", current_data_set, value);
#endif
        }

    }

    regfree (&channel);
    regfree (&group_start);
    regfree (&group_members);
    regfree (&data_set);
    regfree (&data_set_group);
    regfree (&cafc_long_cell);

    fclose (fp);

    return retval;
}

#if 0
int main (int argc, char *argv[])
{

    if (argc < 2)
    {
        fprintf (stderr, "Usage: %s tlafile\n", argv[0]);
        return EXIT_FAILURE;
    }

    tla_parse_file (argv[1]);

    return EXIT_SUCCESS;
}
#endif
