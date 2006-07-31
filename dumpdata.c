#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "dumpdata.h"

list_t *datasets = NULL;
list_t *channels = NULL;

#define bit(d,b) ((d & (1 << b)) ? 1 : 0)
#define falling_edge(b) (bit(diff, b) && !bit(val, b))
#define rising_edge(b) (bit(diff, b) && bit(val, b))

static int dump_single_capture (capture *c, capture *prev_cap)
{
    int i;

    if (prev_cap && capture_time (c) < capture_time (prev_cap))
	printf ("ARGH WE WENT BACKWARDS\n");

    printf ("%llx: ", capture_time (c));
    for (i = 0; i < CAPTURE_DATA_BYTES; i++)
    {
         printf ("%2.2x ", c->data[i]);
    }
    printf ("\n");

    return 0;
}

uint64_t capture_time (capture *c)
{
    uint64_t t, b;
    t = ntohl (c->time_top) & 0xffff; // not sure what the story is here, packed data isn't all time stamp
    //t = 0;
    b = ntohl (c->time_bottom);

    // timings are in 1/8th of a nano-second, so change it into micro seconds
    return (t << 32 | b) / 8000; 
}

int dump_capture (bulk_capture *b)
{
   int i;
   capture *c, *prev = NULL;

   c = (capture *)(b+1);//(char *)b+sizeof (bulk_capture);

   //printf ("Capture Group: %p\n", b);
   for (i = 0; i < b->length / sizeof (capture); i++)
   {
      dump_single_capture (c, prev);
      prev = c;
      c++;
   }

   return 0;
}

void dump_capture_list (list_t *cap, char *name, list_t *channels)
{
    list_t *n;

    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	printf ("%s->%s\n", c->probe_name, c->name);
    }

    printf ("Capture '%s'\n", name);
    for (n = cap; n != NULL; n = n->next)
    {
	printf ("Capture item: %p\n", n);
	dump_capture (n->data);
    }
}

int capture_bit_raw (capture *cap, int probe, int index)
{
    return (cap->data[probe] & (1 << index)) ? 1 : 0;
}

int capture_bit (capture *cap, char *channel_name, list_t *channels)
{
    list_t *n;

    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	if (strcasecmp (c->name, channel_name) == 0)
	{
	    char *probes={"EADC"};
	    int i;
	    int probe = -1;
	    int index;

	    if (strlen (c->probe_name) != 4 || c->probe_name[2] != '_')
	    {
		printf ("Weird probe: %s\n", c->probe_name);
		return -1;
	    }

	    index = c->probe_name[3] - '0';


	    for (i = 0; i < 4; i++)
		if (probes[i] == toupper (c->probe_name[0]))
		    probe = i * 4;
	    if (probe == -1)
	    {
		printf ("Can't identify probe %s\n", c->probe_name);
		return -1;
	    }

	    probe += (3 - (c->probe_name[1] - '0'));

	    //printf ("Probe %s = %d, index=%d, channel=%s\n", c->probe_name, probe, index, channel_name);

	    return capture_bit_raw (cap, probe, index);
	    //return (cap->data[probe] & (1 << index)) ? 1 : 0;
	}
    }

    printf ("Unknown channel: %s\n", channel_name);
    return -1;
}

int capture_bit_transition (capture *cur, capture *prev, char *name, list_t *channels, int dir)
{
    int n, p;

    p = capture_bit (prev, name, channels);
    if (p < 0)
	return -1;
    n = capture_bit (cur, name, channels);
    if (n < 0)
	return -1;

    if (n && !p && dir == TRANSITION_low_to_high)
	return 1;
    if (!n && p && dir == TRANSITION_high_to_low)
	return 1;

    return 0;
}

channel_info *build_channel (char *probe, char *name)
{
    channel_info *retval;

    retval = malloc (sizeof (channel_info));
    strncpy (retval->probe_name, probe, 20);
    retval->probe_name[19] = '\0';

    // they seem to have leading & trailing $ signs on them, so chop them off if they're there
    if (name[0] == '$' && name[strlen(name) - 1] == '$')
    {
	strncpy (retval->name, &name[1], strlen(name) - 2);
	retval->name[strlen(name) - 2] = '\0';
    }
    else
	strncpy (retval->name, name, 20);
    retval->name[19] = '\0';

    return retval;
}

bulk_capture *build_dump (unsigned char *data, int length)
{
   bulk_capture *retval;

   //printf ("build_dump: %p, %d\n", data, length);   

   retval = malloc (length + sizeof (bulk_capture));

   //data += sizeof (capture * 16); // skip the header crap

   memcpy ((char *)retval + sizeof (bulk_capture), data, length);

   retval->length = length;

   return retval;
}

#if 0 // this stuff is all specific to Pertec - removed for now
static void compare_dump (bulk_capture *b1, bulk_capture *b2, char *file1, char *file2)
{
   printf ("file: %s\n", file2);
   dump_capture (b2);
   printf ("file: %s\n", file1);
   dump_capture (b1);
}

int capture_compare (list_t *data1, list_t *data2, char *file1, char *file2)
{
   int i;
   int done = 0;

   data1 = data1->next; // skip first
   data2 = data2->next; //  and again

   i = 0;
   while (!done)
   {
      compare_dump (data1->data, data2->data, file1, file2);
      data1 = data1->next;
      data2 = data2->next;
      if (!data1 || !data2)
         done = 1;
   }

#if 0
   list_t *n;
   bulk_capture *b;

   printf ("dataset 1\n");
   for (n = data1, i = 0; n != NULL; n = n->next, i++)
   {
      b = n->data;
      printf ("capture: %d, length: %d\n", i, b->length);
      //dump_capture (b);
   }

   printf ("dataset 2\n");
   for (n = data2, i = 0; n != NULL; n = n->next, i++)
   {
      b = n->data;
      printf ("capture: %d, length: %d\n", i, b->length);
      //dump_capture (b);
   }
#endif
   return 0;
}

static uint32_t load_val (capture *c)
{
   return (~(c->data[SCOPE_d0]) & 0xff) | (c->data[SCOPE_e0] << 8) | (c->data[SCOPE_d1] << 16);
}

stream_info_t *build_stream_info (list_t *data, char *filename)
{
   stream_info_t *s;

   s = malloc (sizeof (stream_info_t));
   s->data = data->next; // skip the first bogus record
   s->current = s->data->data;
   s->current_cap = (capture *)(s->current+1);//(char *)(s->current) + sizeof (bulk_capture);
   strncpy (s->filename, filename, 99);
   s->filename[99] = '\0';
   
   s->prev = -1;
   s->val = load_val (s->current_cap);
   s->byte = s->val & 0xff;
   s->diff = 0;
   s->command_count = 0;
   s->data_count = 0;

   s->finished = 0;

   return s;
}

void stream_next (stream_info_t *s)
{
   uint32_t val = s->val;
   capture *last = (capture *)((char *)(s->data->data + 1) + (s->current->length));
   s->current_cap++;
   if (s->current_cap >= last)
   {
      printf ("finished one record\n");
      s->data = s->data->next;
      if (s->data)
      {
         s->current = s->data->data;
         s->current_cap = (capture *)(s->current + 1);
      }
      else
      {
         s->current = NULL;
         s->current_cap = NULL;
         s->finished = 1;
      }
   }

   if (!s->finished)
   {
      s->prev = val;
      s->val = load_val (s->current_cap);
      s->byte = s->val & 0xff;
      s->diff = s->prev ^ s->val;
   }
}

int stream_finished (stream_info_t *s)
{
   return s->finished;
}
#if 0
#define stream_next_command(s) stream_next_transition (s, CMD, 0)
#define stream_next_data(s) stream_next_transition (s, DataReq, 1)

int stream_next_transition (stream_info_t *s, int control, int dir)
{
   do
   {
      stream_next (s);
      if (stream_finished (s))
         return -1;
   } while (!(bit (s->diff, control) && bit(s->val, control) == dir));

   return s->val & 0xff;
}
#endif
int stream_next_group (stream_info_t *s)
{
   do
   {
      stream_next (s);
      if (stream_finished (s))
         return -1;
      if (bit (s->diff, CMD) && !bit(s->val, CMD))
      {
         s->type = CMD;
         s->command_count++;
         return CMD;
      }
      if (bit (s->diff, DataReq) && bit(s->val, DataReq))
      {
         s->type = DataReq;
         s->data_count++;
         return DataReq;
      }
   } while (1);
}

static uint8_t printable_char (int data)
{
   if (data >= ' ' && data <= 126)
      return data;
   return '.';
}

static void display_and_advance (stream_info_t *s1, stream_info_t *s2)
{
      if (s1->type != -1 && s2->type != -1)
      {
         printf ("%s%2.2x(%c)%s\t\t", s1->type == CMD ? "\t" : "", s1->byte, printable_char (s1->byte), s1->type == CMD ? "" : "\t");
         printf ("%s%2.2x(%c)%s", s2->type == CMD ? "\t" : "", s2->byte, printable_char (s2->byte), s2->type == CMD ? "" : "\t");
         printf ("\t%d,%d\t%d,%d", s1->command_count, s1->data_count, s2->command_count, s2->data_count);
         printf ("\n");
      }

      stream_next_group (s1);
      stream_next_group (s2);
}

static void read_data_block (stream_info_t *s)
{
   s->buff_size = 0;
   while (stream_next_group (s) == DataReq)
   {
      s->buffer[s->buff_size++] = s->byte;
   } 
}

static void display_buffers (stream_info_t *s1, stream_info_t *s2)
{
   int i;
   printf ("buffers: %d, %d\n", s1->buff_size, s2->buff_size);
   
   for (i = 0; i < s1->buff_size; i++)
      printf ("%c", printable_char (s1->buffer[i]));
   if (s1->buff_size)
      printf ("\n");
   for (i = 0; i < s2->buff_size; i++)
      printf ("%c", printable_char (s2->buffer[i]));
   if (s2->buff_size)
      printf ("\n");
#define max(a,b) (((a) > (b)) ? (a) : (b))
   for (i = 0; i < max (s1->buff_size, s2->buff_size); i++)
   {
      if ((i < s1->buff_size) && (i < s2->buff_size))
      {
         if (s1->buffer[i] != s2->buffer[i])
            printf ("buffer error: byte %d: %2.2x(%c) != %2.2x(%c)\n", i, s1->buffer[i], printable_char (s1->buffer[i]), s2->buffer[i], printable_char (s2->buffer[i]));
      }
      else if (i >= s1->buff_size)
         printf ("buffer error: byte: %d NULL != %2.2x(%c)\n", i, s2->buffer[i], printable_char (s2->buffer[i]));
      else if (i >= s2->buff_size)
         printf ("buffer error: byte: %d %2.2x(%c) != NULL\n", i, s1->buffer[i], printable_char (s1->buffer[i]));
      else
         printf ("THIS SHOULDN'T HAPPEN\n");
   }
}

static void parse_data_block (stream_info_t *s1, stream_info_t *s2)
{
   printf ("data block: command: %2.2x\n", s1->byte);

   //display_and_advance (s1, s2);
   //stream_next_group (s1);
   //stream_next_group (s2);
   read_data_block (s1);
   read_data_block (s2);

   display_buffers (s1, s2);
}

void compare_streams (stream_info_t *s1, stream_info_t *s2)
{
   //int i = 0;
   printf ("compare streams: %s, %s\n", s1->filename, s2->filename);

   //printf ("c1: %2.2x, c2: %2.2x\n", stream_next_command (s1), stream_next_command (s2));

   //stream_next (s1);
   //stream_next (s2);
   stream_next_group (s1);
   stream_next_group (s2);

   while (!stream_finished (s1) && !stream_finished (s2))
   {
      int handled = 0;

      if (s1->type == s2->type && s1->byte == s2->byte)
      {
         if (s1->type == CMD)
         {
            switch (s1->byte)
            {
               case PERTEC_write:
               case PERTEC_read:
                  parse_data_block (s1, s2);
                  handled = 1;
                  break;
            }
         }
      }

      if (!handled)
      {
         while (s2->type != s1->type || s2->byte != s1->byte)
         {
            printf ("skipping: %d: %2.2x (!= %d: %2.2x)\n", s2->type, s2->byte, s1->type, s1->byte);
            stream_next_group (s2); // walk second stream up
         }
      }

      display_and_advance (s1, s2);
   }
}
#endif
