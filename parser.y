%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dumpdata.h"
#include "lists.h"

extern char *yytext;
extern int yychar;
extern int line_number;
extern FILE *yyin;

int yyerror (char *error)
{
    printf ("yyerror: line %d\ntoken: %s: %s\nexpecting: %d\npos: %ld\n", 
	    line_number, yytext, error, yychar, ftell (yyin));
    return 0;
}

int yylex ();

list_t *final_capture = NULL;
list_t *final_channels = NULL;
int nprobes;
%}

%union{
char *string;
int integer;
struct 
{
   unsigned int length;
   unsigned char *data;
} data;
capture *cap;
list_t *list;
} 

%token <string> t_STRING t_IDENT t_C
%token <data> t_DATASET
%token <integer> t_BOOL t_NUMBER
%token t_LBRACE t_RBRACE t_EQUALS t_MINUS t_PLUS t_DOT
%token t_BINARY_HEADER
%token t_PREAMBLE
%token t_COMPOSITE_CELL t_STRING_CELL t_ARRAY_CELL t_BYTE_CELL t_LONG_CELL t_LONG_LONG_CELL 
%token t_BOOLEAN_CELL t_DOUBLE_CELL t_CHANNEL_CELL t_INSTRUMENT_CELL
%token t_RDA_INTERNAL t_CCM_TIME_PER_DIV
%token t_CAP_ROOT t_CJM_PROBE

%type <string> ident
%type <list> cblock cell_list block composite_cell array_cell rda_internal cap_root byte_cell string_cell boolean_cell long_cell long_long_cell double_cell channel_cell ccm_time_per_div instrument_cell cell cjm_probe
%type <integer> channel_contents

%%

tlafile: t_PREAMBLE cell_list {final_capture = $2}

composite_cell: t_COMPOSITE_CELL t_STRING t_STRING block {$$ = NULL;}

byte_cell:  t_BYTE_CELL t_STRING t_STRING t_EQUALS t_LBRACE number number number t_RBRACE {$$ = NULL;}

array_cell: t_ARRAY_CELL t_STRING t_STRING block {$$ = $4;}

string_cell: t_STRING_CELL t_STRING t_STRING t_EQUALS t_LBRACE t_STRING t_RBRACE {$$ = NULL;}

long_cell: t_LONG_CELL t_STRING t_STRING t_EQUALS t_LBRACE number t_RBRACE {$$ = NULL;}

long_long_cell: t_LONG_LONG_CELL t_STRING t_STRING t_EQUALS t_LBRACE number number number number ident t_RBRACE {$$= NULL;}

boolean_cell:  t_BOOLEAN_CELL t_STRING t_STRING t_EQUALS t_LBRACE t_BOOL t_RBRACE {$$ = NULL;}

double_cell: t_DOUBLE_CELL t_STRING t_STRING t_EQUALS t_LBRACE number t_RBRACE {$$ = NULL;}

rda_internal: t_RDA_INTERNAL t_STRING t_STRING block {$$ = $4;}

channel_cell: t_CHANNEL_CELL t_STRING t_STRING channel_contents {final_channels = list_prepend (final_channels, build_channel ($2, $3, $4)); $$ = NULL;}

channel_contents: t_LBRACE t_RBRACE		{$$ = 0;}
	       | t_LBRACE t_BOOLEAN_CELL t_STRING t_STRING t_EQUALS  t_LBRACE t_BOOL t_RBRACE t_RBRACE {$$ = $7;}

instrument_cell: t_INSTRUMENT_CELL t_STRING t_STRING block {$$ = $4;}

ccm_time_per_div: t_CCM_TIME_PER_DIV t_STRING t_STRING t_EQUALS t_LBRACE number number number number ident t_RBRACE {$$ = NULL;}

cjm_probe: t_CJM_PROBE t_STRING t_STRING block {$$ = $4; nprobes++}

cap_root: t_CAP_ROOT t_STRING t_STRING block {$$ = $4;};

cblock: t_C t_STRING t_STRING block
            {
               //printf ("got block: %s (%s, %p)\n", $1, $2, $4); 
	       $$ = NULL;
	       if (strcmp ($2, "TbTimebaseSet") == 0)
		  $$ = $4;
	       else if (strcmp ($1, "CcmSaveDataCompositeCell") == 0) // contains the cjmtimebasedata
		  $$ = $4;
	       else if (strcmp ($1, "CjmTimebaseData") == 0 && strcmp ($2, "TbHiResTimebaseData") != 0) // contains the dasetnormal
		  $$ = $4;
	       else if (strcmp ($2, "DaSetNormal") == 0)
                  $$ = $4;
#if 0
	       else if (strcmp ($2, "DataRoot") == 0)
		  $$ = $4;
	       else if (strcmp ($2, "PlugInInstance") == 0 || strcmp ($2, "PluginInstance0") == 0)
		  $$ = $4;
#endif
               //$$ = $4; // override it, this is broken
	       //if ($$)
		    //printf ("Using set %s (%s)\n", $1, $2);
            };

block:   t_LBRACE cell_list t_RBRACE {$$ = $2;}
   | t_EQUALS t_DATASET { $$ = list_prepend (NULL, build_dump ($2.data, $2.length)); }
   ;

cell_list:  cell cell_list {$$ = list_list_append ($1, $2);}
      |                    {$$ = NULL;}
      ;

cell: composite_cell
   |  byte_cell
   | array_cell
   | string_cell
   | long_cell
   | long_long_cell
   | boolean_cell
   | double_cell
   | channel_cell
   | instrument_cell
   | cap_root
   | cblock
   | rda_internal
   | ccm_time_per_div
   | cjm_probe
   ;

number: t_NUMBER;

ident:   t_IDENT;
%%
bulk_capture *load_capture (char *filename)
{
    list_t *cap;
    off_t len;
    char *buf;
    
    fprintf (stderr, "About to load %s\n", filename);

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
    buf = malloc (10 * 1024 * 1024);

    setvbuf (yyin, buf, _IOFBF, 10 * 1024 * 1024);

    nprobes = 0;
    yyparse ();
    cap = final_capture;
    fclose (yyin);
    free (buf);
    yyin = NULL;

    // each probe needs 2 bytes, plus the 2 bytes for the clocking lines
    if (nprobes * 2 + 2 != CAPTURE_DATA_BYTES)
    {
	fprintf (stderr, "Probes doesn't line up with CAPTURE_DATA_BYTES (%d, %d)\n", nprobes, CAPTURE_DATA_BYTES);
	fprintf (stderr, "Possibly setup for incorrect scope\n");
	return NULL;
    }

    return cap ? cap->data : NULL;
}

