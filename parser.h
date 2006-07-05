/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     t_STRING = 258,
     t_IDENT = 259,
     t_C = 260,
     t_DATASET = 261,
     t_NUMBER = 262,
     t_BOOL = 263,
     t_LBRACE = 264,
     t_RBRACE = 265,
     t_EQUALS = 266,
     t_MINUS = 267,
     t_PLUS = 268,
     t_DOT = 269,
     t_BINARY_HEADER = 270,
     t_PREAMBLE = 271,
     t_COMPOSITE_CELL = 272,
     t_STRING_CELL = 273,
     t_ARRAY_CELL = 274,
     t_BYTE_CELL = 275,
     t_LONG_CELL = 276,
     t_LONG_LONG_CELL = 277,
     t_BOOLEAN_CELL = 278,
     t_DOUBLE_CELL = 279,
     t_CHANNEL_CELL = 280,
     t_RDA_INTERNAL = 281,
     t_CCM_TIME_PER_DIV = 282,
     t_CAP_ROOT = 283
   };
#endif
/* Tokens.  */
#define t_STRING 258
#define t_IDENT 259
#define t_C 260
#define t_DATASET 261
#define t_NUMBER 262
#define t_BOOL 263
#define t_LBRACE 264
#define t_RBRACE 265
#define t_EQUALS 266
#define t_MINUS 267
#define t_PLUS 268
#define t_DOT 269
#define t_BINARY_HEADER 270
#define t_PREAMBLE 271
#define t_COMPOSITE_CELL 272
#define t_STRING_CELL 273
#define t_ARRAY_CELL 274
#define t_BYTE_CELL 275
#define t_LONG_CELL 276
#define t_LONG_LONG_CELL 277
#define t_BOOLEAN_CELL 278
#define t_DOUBLE_CELL 279
#define t_CHANNEL_CELL 280
#define t_RDA_INTERNAL 281
#define t_CCM_TIME_PER_DIV 282
#define t_CAP_ROOT 283




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 22 "parser.y"
typedef union YYSTYPE {
char *string;
int integer;
struct 
{
   unsigned int length;
   unsigned char *data;
} data;
capture *cap;
list_t *list;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 106 "parser.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



