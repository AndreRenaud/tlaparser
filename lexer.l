%{
#include <string.h>
#include "dumpdata.h"
#include "parser.h"

#define MAX_STR_CONST 10000

static char string_buf[MAX_STR_CONST];
static char *string_buf_ptr;

static int datasetpos;
static int datasetlen;
static int percentage = 0; /* how much of the data have we loaded */

static unsigned char data[MAX_DATA_LEN];

int line_number = 1;
%}

%x str comment dataset
%option noyywrap

%%

"PREAMBLE"       {return t_PREAMBLE;}
"CafcCompositeCell"  {return t_COMPOSITE_CELL;}
"CafcStringCell"  {return t_STRING_CELL;}
"CafcArrayCell"   {return t_ARRAY_CELL;}
"CafcByteCell"    {return t_BYTE_CELL;}
"CafcLongCell"    {return t_LONG_CELL;}
"CafcLongLongCell" {return t_LONG_LONG_CELL;}
"CafcBooleanCell" {return t_BOOLEAN_CELL;}
"CafcDoubleCell" {return t_DOUBLE_CELL;}
"CjmChannel"	{return t_CHANNEL_CELL;}
"CjmInstrument"	{return t_INSTRUMENT_CELL;}
"CjmProbe"	{return t_CJM_PROBE;}

"CapRoot"         {return t_CAP_ROOT;}

"RDAInternal"[a-zA-Z]+ {return t_RDA_INTERNAL;}

"CcmTimePerDiv"      {return t_CCM_TIME_PER_DIV;}

C(l|w|j|s|n|net|c)m[a-zA-Z0-9]+    {yylval.string = strdup (yytext); return t_C;}

"{"            {return t_LBRACE;}
"}"            {return t_RBRACE;}
"="            {return t_EQUALS;}
"+"            {return t_PLUS;}
"-"            {return t_MINUS;}
"."            {return t_DOT;}

"TRUE"         {yylval.integer = 1; return t_BOOL;}
"FALSE"        {yylval.integer = 0; return t_BOOL;}

[0-9\.e\-+]+     {return t_NUMBER;}
[a-zA-Z_][a-zA-Z_0-9]*     {yylval.string = strdup (yytext); return t_IDENT;}

"#"F?[0-9]+      { /* for some reason it says #3480 for the size, when 480 is the size, so we skip # & 3 */
                  BEGIN(dataset); 
                  datasetpos = 0; 
		  datasetlen = atoi (&yytext[2]);
		  if (datasetlen >= MAX_DATA_LEN)
		  {
		      printf ("datasetlen too large %d > %d\n", datasetlen, MAX_DATA_LEN);
		      abort ();
		  }
                  //printf ("Dataset of length: %d\n", datasetlen);
               }

[ \t\r]*         {;}
"\n"           {line_number++;}

"\""      string_buf_ptr = string_buf; BEGIN(str);
.	    {printf ("Weird char %x (%c) @ 0x%lx\n", (int)yytext[0], yytext[0], ftell (yyin));}

<dataset>"\n" { data[datasetpos++]=yytext[0]; 
		line_number++;
		/* Do percentage for larger datasets */
		if (datasetlen > 1000000 && datasetpos * 10 / datasetlen != percentage) // 10% gaps, don't do below as it is slightly slower
		{
		    percentage = datasetpos * 10 / datasetlen;
		    printf ("%d%% loaded\n", percentage * 10);
		}
	      }
<dataset>.    {data[datasetpos++]=yytext[0]; 
		if (datasetpos >= datasetlen)
		{
		    BEGIN(INITIAL);
		    //printf ("finished dataset, line: %d, len: %d: '%s'\n", line_number,datasetpos, yytext);
		    line_number++;
		    yylval.data.data = data;
		    yylval.data.length = datasetpos;
		    return t_DATASET;
		}
	      }


<str>[^\"]  {*string_buf_ptr++ = yytext[0];}
<str>\\(.|\n)  {*string_buf_ptr++ = yytext[1];}
<str>\"        { /* saw closing quote - all done */
      BEGIN(INITIAL);
      *string_buf_ptr = '\0';
      yylval.string = strdup (string_buf);
      return t_STRING;
      }


"//"                   BEGIN(comment);
<comment>[^\n]*        /* eat anything that's not a '*' */
<comment>\n            {line_number++; BEGIN(INITIAL);}

%%

