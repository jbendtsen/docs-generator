#include "docs-generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

void *vector_add(Vector *vec, int elem_size, int count)
{
	if (count <= 0)
		return NULL;

	int new_size = vec->n + count;

    if (!vec->buf || new_size >= vec->cap) {
        int old_cap = vec->cap;
        int new_cap = old_cap;

        if (new_cap < 16)
            new_cap = 16;
        while (new_size >= new_cap)
            new_cap = (int)((float)new_cap * 1.7f) + 1;

        void *new_buf = malloc(new_cap * elem_size);
        if (vec->buf) {
            memcpy(new_buf, vec->buf, old_cap * elem_size);
            free(vec->buf);
        }
        vec->buf = new_buf;
        vec->cap = new_cap;
    }

	char *ptr = &((char*)vec->buf)[vec->n * elem_size];
	vec->n = new_size;
	return (void*)ptr;
}

void vector_append_array(Vector *vec, int elem_size, const void *data, int count)
{
	if (count <= 0)
		return;

	int bytes = elem_size * count;
	char *space = vector_add(vec, elem_size, count);
	memcpy(space, data, bytes);
}

void vector_append_cstring(Vector *vec, const char *str)
{
	size_t len;
	if (!str || (len = strlen(str)) == 0)
		return;

	char *space = vector_add(vec, 1, len);
	memcpy(space, str, len);
}

void vector_append_utf8_html(Vector *vec, const char *str, int len)
{
	if (!str || len <= 0)
		return;

	char esc_c = 0;
	bool done = false;
	int esc_left = 0;
	int utf8_left = 0;
	char *out;
	const char *ip = str;

	int to_copy = len;
	int prev_copy = to_copy;

	while (ip - str < len && to_copy > 0) {
		out = vector_add(vec, 1, to_copy);

		char *op = out;
		while (op - out < prev_copy) {
			if (esc_left > 0) {
				if (esc_left == 6) *op++ = '&';
				else if (esc_left == 5) *op++ = '#';
				else if (esc_left == 4) *op++ = 'x';
				else if (esc_left == 3) *op++ = "0123456789abcdef"[(esc_c >> 4) & 0xf];
				else if (esc_left == 2) *op++ = "0123456789abcdef"[esc_c & 0xf];
				else if (esc_left == 1) *op++ = ';';
				esc_left--;
				continue;
			}

			char c = *ip;
			if (c == 0) {
				done = true;
				break;
			}

			if (utf8_left-- > 0) {
				*op++ = *ip++;
			}
			else if (c < 0) {
				uint8_t b = c;
				if ((b & 0xe0) == 0xc0) utf8_left = 1;
				else if ((b & 0xf0) == 0xe0) utf8_left = 2;
				else if ((b & 0xf8) == 0xf0) utf8_left = 3;
				*op++ = *ip++;
			}
			else {
				utf8_left = 0;
				if ((c < ' ' && c != '\n' && c != '\t') || c > '~' || c == '&' || c == '<' || c == '>' || c == '"') {
					esc_c = *ip++;
					esc_left = 6;
					to_copy += esc_left - 1;
				}
				else {
					*op++ = *ip++;
				}
			}
		}

		to_copy -= prev_copy;
		prev_copy = to_copy;
	}
}

void vector_free(Vector *vec)
{
    if (vec->buf) {
        free(vec->buf);
        vec->buf = NULL;
    }
}

File read_whole_file(char *path)
{
	File file = {0};

	int path_len;
	if (!path || (path_len = strlen(path)) == 0) {
		printf("Invalid source file path\n");
		return file;
	}

	if (path[path_len-1] == '/' || path[path_len-1] == '\\') {
		path[path_len-1] = '\0';
		path_len--;
	}

	FILE *f = fopen(path, "rb");
	if (!f) {
		printf("Could not find \"%s\"\n", path);
		return file;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	if (sz < 0) {
		printf("Could not read from \"%s\"\n", path);
		return file;
	}

	char *buf = malloc(sz + 1);
	if (sz > 0) fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	file.buf = buf;
	file.size = sz;

	char *last_slash = strrchr(path, '/');
	if (!last_slash)
		last_slash = strrchr(path, '\\');

	if (last_slash) {
		*last_slash = '\0';
		file.path = path;
		file.name = last_slash + 1;
	}
	else {
		file.path = NULL;
		file.name = path;
	}

	return file;
}

void source_close(Source *s)
{
	if (s->file.buf) {
		free(s->file.buf);
		s->file.buf = NULL;
	}

	vector_free(&s->docs);
	vector_free(&s->tags);
	vector_free(&s->descs);
}
