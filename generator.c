#include "docs-generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void print_docs(Source *source, const char *output_root)
{
    const char *comment = "\n\ncomment: ";
    const char *code1 = "\ncode: ";
    const char *param = "\n\tparam: ";
    const char *ret = "\n\treturn: ";
    const char *code2 = "\n\n\tcode: ";
	Doc *d = source->docs.buf;
	char *in_buf = source->file.buf;

    for (int i = 0; i < source->docs.n; i++) {
    	/*
		    fwrite(comment, 1, strlen(comment), stdout);
		    fwrite(&in_buf[d->main.cmt_start], 1, d->main.cmt_end - d->main.cmt_start + 1, stdout);
        */

		putchar('\n');

		if (d->access == DOC_ACCESS_PUBLIC) printf("PUBLIC ");
		else if (d->access == DOC_ACCESS_PROTECTED) printf("PROTECTED ");
		else if (d->access == DOC_ACCESS_PRIVATE) printf("PRIVATE ");

		if (d->flags & DOC_FLAG_KOTLIN) printf("KOTLIN ");
		if (d->flags & DOC_FLAG_METHOD) printf("METHOD ");
		if (d->flags & DOC_FLAG_FIELD)  printf("FIELD ");
		if (d->flags & DOC_FLAG_STATIC) printf("STATIC ");
		if (d->flags & DOC_FLAG_FINAL)  printf("FINAL ");
		if (d->flags & DOC_FLAG_SYNC)   printf("SYNC ");

		if (d->name.start >= 0) {
			fwrite("\nname: ", 1, 7, stdout);
			fwrite(&in_buf[d->name.start], 1, d->name.end - d->name.start + 1, stdout);
		}

		fwrite(code1, 1, strlen(code1), stdout);
	    fwrite(&in_buf[d->main.code_start], 1, d->main.code_end - d->main.code_start + 1, stdout);

		Span *s = d->first_desc_line >= 0 ? &((Span*)source->descs.buf)[d->first_desc_line] : NULL;
		for (int j = 0; s && j < d->n_desc_lines; j++) {
			if (s->start >= 0)
				fwrite(&in_buf[s->start], 1, s->end - s->start + 1, stdout);
			fputc('\n', stdout);
			s++;
		}

        Tag *p = d->first_param >= 0 ? &((Tag*)source->tags.buf)[d->first_param] : NULL;
        for (int j = 0; p && j < d->n_params; j++) {
            if (p->code_start >= 0 && p->code_end >= 0) {
                fwrite(code2, 1, strlen(code2), stdout);
                fwrite(&in_buf[p->code_start], 1, p->code_end - p->code_start + 1, stdout);
            }
            if (p->cmt_start >= 0 && p->cmt_end >= 0) {
                fwrite(param, 1, strlen(param), stdout);
                fwrite(&in_buf[p->cmt_start], 1, p->cmt_end - p->cmt_start + 1, stdout);
            }

            p++;
        }

        if (d->ret.cmt_start >= 0) {
            fwrite(ret, 1, strlen(ret), stdout);
            fwrite(&in_buf[d->ret.cmt_start], 1, d->ret.cmt_end - d->ret.cmt_start + 1, stdout);
        }

        d++;
    }
}

File generate_html(Source *source, File *css, int should_embed_css)
{
	Vector html = {0};
	char *in = source->file.buf;
	
	const char *class_name = NULL;
	int class_name_len = 0;

	if (source->class_name.start < source->class_name.end) {
		class_name = &in[source->class_name.start];
		class_name_len = source->class_name.end - source->class_name.start + 1;
	}
	else {
		class_name = source->file.name;
		class_name_len = strlen(source->file.name);

		for (int i = class_name_len-1; i >= 0; i--) {
			if (i > 0 && class_name[i] == '.') {
				class_name_len = i;
				break;
			}
		}
	}

	vector_append_cstring(&html, "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>");
	vector_append_utf8_html(&html, class_name, class_name_len);
	vector_append_cstring(&html, "</title>");

	if (should_embed_css) {
		vector_append_cstring(&html, "<style>\n");
		vector_append_utf8_html(&html, css->buf, css->size);
		vector_append_cstring(&html, "\n</style>");
	}
	else {
		vector_append_cstring(&html, "<link rel=\"stylesheet\" href=\"");
		vector_append_utf8_html(&html, css->name, strlen(css->name));
		vector_append_cstring(&html, "\">");
	}

	vector_append_cstring(&html, "</head>\n<body><h1>");
	vector_append_utf8_html(&html, class_name, class_name_len);
	vector_append_cstring(&html, "</h1>");

	Doc *d = source->docs.buf;
	int n_docs = source->docs.n;

	for (int i = 0; d && i < n_docs; i++) {
		
	}

	vector_append_cstring(&html, "</body></html>\n");

	fwrite(html.buf, 1, html.n, stdout);

	File res = {0};
	res.buf = html.buf;
	res.size = html.n;
	return res;
}
