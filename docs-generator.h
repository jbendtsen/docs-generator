#pragma once

#define SORT_CONTENT  0
#define SORT_ALPHA    1

#define EMBED_AUTO    0
#define EMBED_ALWAYS  1
#define EMBED_NEVER   2

#define DOC_FLAG_PAREN     0x1
#define DOC_FLAG_EQUALS    0x2
#define DOC_FLAG_CURLY     0x4
#define DOC_FLAG_SEMIC     0x8
#define DOC_FLAG_COLON    0x10
#define DOC_FLAG_METHOD   0x20
#define DOC_FLAG_FIELD    0x40
#define DOC_FLAG_STATIC   0x80
#define DOC_FLAG_FINAL   0x100
#define DOC_FLAG_SYNC    0x200
#define DOC_FLAG_KOTLIN  0x400

#define DOC_ACCESS_PACKAGE    0
#define DOC_ACCESS_PUBLIC     1
#define DOC_ACCESS_PROTECTED  2
#define DOC_ACCESS_PRIVATE    3

typedef struct {
    void *buf;
    int cap;
    int n;
} Vector;

typedef struct {
	int start;
	int end;
} Span;

typedef struct {
    int cmt_start;
    int cmt_end;
    int code_start;
    int code_end;
} Tag;

typedef struct {
	Span name;
    Tag main;
    Tag ret;
    int first_desc_line;
    int n_desc_lines;
    int first_param;
    int n_params;
    int code_lineno;
    unsigned short flags;
    short access;
} Doc;

typedef struct {
	char *name;
    char *path;
    char *buf;
    int size;
} File;

typedef struct {
    File file;
    Span package_name;
    Span class_name;
    Span extends_name;
    Vector implements_names;
    Vector docs;
    Vector tags;
    Vector descs;
    int sort_order;
} Source;

void parse_source_file(Source *file);

File generate_html(Source *source, File *css, int should_embed_css);

void *vector_add(Vector *vec, int elem_size, int count);
void vector_append_array(Vector *vec, int elem_size, const void *data, int count);
void vector_append_cstring(Vector *vec, const char *str);
void vector_append_utf8_html(Vector *vec, const char *str, int len);
void vector_free(Vector *vec);
File read_whole_file(char *path);
void source_close(Source *s);
