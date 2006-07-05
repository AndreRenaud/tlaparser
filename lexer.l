%{
#include <string.h>
#include "dumpdata.h"
#include "parser.h"

#define MAX_STR_CONST 100
#define MAX_DATA_LEN 50 * 1024 * 1024

char string_buf[MAX_STR_CONST];
char *string_buf_ptr;

int datasetpos;

unsigned char data[MAX_DATA_LEN];

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

"#"F?[0-9]+      {
                  yylval.integer = atoi (&yytext[1]); 
                  BEGIN(dataset); 
                  //printf ("Dataset of length: %d\n", yylval.integer); 
                  datasetpos = 0; 
               }

<dataset>("}\r\n"|"\r\n\t")  {
                     BEGIN(INITIAL);
                     //printf ("finished dataset, line: %d, len: %d\n", line_number,datasetpos);
                     line_number++;
                     yylval.data.data = data;
                     yylval.data.length = datasetpos; 
                     return t_DATASET;
                  }
<dataset>"\n" {data[datasetpos++]=yytext[0]; line_number++;}
<dataset>[\0\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17\20\21\22\23\24\25\26\27\30\31\32\33\34\35\36\37\40\41\42\43\44\45\46\47\48\50\51\52\53\54\55\56\57\60\61\62\63\64\65\66\67\70\71\72\73\74\75\76\77] { data[datasetpos++] = yytext[0];}
<dataset>[\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177] { data[datasetpos++] = yytext[0];}
<dataset>[\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277] { data[datasetpos++] = yytext[0];}
<dataset>[\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377] { data[datasetpos++] = yytext[0];}


 

[ \t\r]*         {;}
"\n"           {line_number++;}

"\""      string_buf_ptr = string_buf; BEGIN(str);

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

