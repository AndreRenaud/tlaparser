#ifndef PARSERS_H
#define PARSERS_H

typedef void (*parser_function)(bulk_capture *cap, char *filename, list_t *channels);

void parse_61k (bulk_capture *cap, char *filename, list_t *channels);
void parse_8250 (bulk_capture *cap, char *filename, list_t *channels);
void parse_kennedy (bulk_capture *cap, char *filename, list_t *channels);
void parse_pertec (bulk_capture *cap, char *filename, list_t *channels);
void parse_scsi (bulk_capture *cap, char *filename, list_t *channels);
void parse_xd (bulk_capture *cap, char *filename, list_t *channels);
void parse_pci (bulk_capture *cap, char *filename, list_t *channels);
void parse_spi (bulk_capture *b, char *filename, list_t *channels);
void parse_nor (bulk_capture *b, char *filename, list_t *channels);
void parse_dm9000 (bulk_capture *b, char *filename, list_t *channels);
void parse_camera (bulk_capture *b, char *filename, list_t *channels);
void parse_ssc_audio (bulk_capture *b, char *filename, list_t *channels);
void parse_ov3640 (bulk_capture *b, char *filename, list_t *channels);
void parse_unformatted (bulk_capture *b, char *filename, list_t *channels);
void parse_half_formatted (bulk_capture *b, char *filename, list_t *channels);
void parse_fetex (bulk_capture *b, char *filename, list_t *channels);

#endif

