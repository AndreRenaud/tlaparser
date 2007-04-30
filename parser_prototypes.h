#ifndef PARSERS_H
#define PARSERS_H

typedef void (*parser_function)(list_t *cap, char *filename, list_t *channels);

void parse_61k (list_t *cap, char *filename, list_t *channels);
void parse_8250 (list_t *cap, char *filename, list_t *channels);
void parse_kennedy (list_t *cap, char *filename, list_t *channels);
void parse_pertec (list_t *cap, char *filename, list_t *channels);
void parse_scsi (list_t *cap, char *filename, list_t *channels);
void parse_xd (list_t *cap, char *filename, list_t *channels);

#endif

