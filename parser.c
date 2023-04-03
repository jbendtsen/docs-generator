#include "docs-generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

void span_reset(Span *s)
{
	s->start = -1;
	s->end = -1;
}

void tag_reset(Tag *t)
{
    t->cmt_start = -1;
    t->cmt_end = -1;
    t->code_start = -1;
    t->code_end = -1;
}

void doc_reset(Doc *d)
{
	span_reset(&d->name);
    tag_reset(&d->main);
    tag_reset(&d->ret);
    d->first_desc_line = -1;
    d->n_desc_lines = 0;
    d->first_param = -1;
    d->n_params = 0;
    d->code_lineno = -1;
    d->flags = 0;
    d->access = 0;
}

void add_doc(Source *source, Doc *d)
{
    if (d->main.cmt_start < 0 || d->main.cmt_end < 0 || d->main.code_start < 0 || d->main.code_end < 0 || d->code_lineno < 0) {
        fprintf(stderr, "%s:", source->file.name);
        if (d->code_lineno >= 0) fprintf(stderr, "%d:", d->code_lineno + 1);
        fprintf(stderr, " Invalid doc");
    }
    else {
    	if ((~d->flags & (DOC_FLAG_PAREN | DOC_FLAG_CURLY | DOC_FLAG_EQUALS)) == DOC_FLAG_EQUALS)
    		d->flags |= DOC_FLAG_METHOD;

		if (d->flags & DOC_FLAG_SEMIC)
			d->flags |= DOC_FLAG_FIELD;

        Doc *new_doc = vector_add(&source->docs, sizeof(Doc), 1);
        *new_doc = *d;
    }
    doc_reset(d);
}

void add_param(Source *source, Doc *doc, Tag *tag)
{
    if (doc->first_param < 0)
        doc->first_param = source->tags.n;

    Tag *new_tag = vector_add(&source->tags, sizeof(Tag), 1);
    *new_tag = *tag;
    doc->n_params++;

    tag_reset(tag);
}

void add_dline(Source *source, Doc *doc, Span *dline)
{
    if (doc->first_desc_line < 0)
        doc->first_desc_line = source->descs.n;

    Span *new_span = vector_add(&source->descs, sizeof(Span), 1);
    *new_span = *dline;
    doc->n_desc_lines++;

    span_reset(dline);
}

void sort_source_docs(Source *source)
{
	
}

void parse_source_file(Source *source)
{
	int companion_brace_level = -1;

    bool is_javadoc = false;
    bool inside_cmd = false;
    bool is_line_cmd = false;
    bool is_param = false;
    bool is_return = false;
    bool seen_ws = false;
    bool seen_code_atsym = false;
    bool seen_semicolon = false;
    bool seen_curly = false;
    bool seen_paren = false;
    int n_open_paren = 0;
    int n_lines = 0;
    int last_nonname_idx = -1;
    uint64_t last8 = 0;
    int utf8_left = 0;

    Doc doc; doc_reset(&doc);
    Tag tag; tag_reset(&tag);
    Span dline; span_reset(&dline);

	char *buf = source->file.buf;
	int sz = source->file.size;
    for (int i = 0; i < sz; i++) {
        char c = buf[i];
        last8 = (last8 << 8LL) | ((int64_t)c & 0xffLL);

		if (utf8_left-- > 0) {
			continue;
		}
		else if (c < 0) {
			uint8_t b = (uint8_t)c;
			if ((b & 0xe0) == 0xc0) utf8_left = 1;
			else if ((b & 0xf0) == 0xe0) utf8_left = 2;
			else if ((b & 0xf8) == 0xf0) utf8_left = 3;
			continue;
		}
		utf8_left = 0;

        if (c == '\n')
            n_lines++;

        if (!is_javadoc && i >= 2 && buf[i-2] == '/' && buf[i-1] == '*' && c == '*') {
            is_javadoc = true;
            if (doc.main.cmt_start >= 0 && doc.main.code_start >= 0) {
                doc.main.code_end = i-3;
                add_doc(source, &doc);
            }
            doc.main.cmt_start = i-2;
        }
        else if (i >= 1 && buf[i-1] == '*' && c == '/') {
            is_javadoc = false;
            doc.main.cmt_end = i;

            if (tag.code_start >= 0 && tag.code_end >= 0) {
                tag.cmt_end = i - 2;
                if (is_return)
                    doc.ret = tag;
                else
                    add_param(source, &doc, &tag);
            }
            tag_reset(&tag);

			if (!is_line_cmd && dline.start >= 0) {
				dline.end = i - 2;
				add_dline(source, &doc, &dline);
			}
            span_reset(&dline);

            inside_cmd = false;
            is_line_cmd = false;
            is_param = false;
            is_return = false;
            seen_ws = false;
            seen_code_atsym = false;
            seen_semicolon = false;
            seen_curly = false;
            seen_paren = false;
            n_open_paren = 0;
        }

        if (doc.main.cmt_start >= 0) {
            if (is_javadoc) {
                bool is_ws = c == '\t' || c == '\r' || c == '\n' || c == ' ';
                if (is_ws) {
                    if (inside_cmd) {
                    	uint64_t prev7 = last8 >> 8;
                        if ((prev7 << 24) == 0x706172616d000000LL) { // param
                            is_param = true;
                        }
                        else if ((prev7 << 16) == 0x72657475726e0000LL) { // return
                            is_return = true;
                        }
                    }
                    else if (is_line_cmd && ((is_param && tag.code_start >= 0) || (is_return && tag.cmt_start >= 0))) {
                        if (is_param && tag.code_end < 0) {
                            tag.code_end = i - 1;
                        }
                        if (c == '\r' || c == '\n') {
                            tag.cmt_end = i - 1;
                            if (is_return) {
                                doc.ret = tag;
                                tag_reset(&tag);
                            }
                            else {
                                add_param(source, &doc, &tag);
                            }
                        }
                    }
                    if (c == '\r' || c == '\n') {
                    	if (!is_line_cmd) {
                    		dline.end = i - 1;
                    		add_dline(source, &doc, &dline);
						}
                		span_reset(&dline);
                		is_line_cmd = false;
                    }
                    inside_cmd = false;
                }
                else if (is_line_cmd) {
                    if (!inside_cmd) {
                        if (is_param) {
                            if (tag.code_start < 0)
                                tag.code_start = i;
                            else if (tag.code_end >= 0 && tag.cmt_start < 0)
                                tag.cmt_start = i;
                        }
                        else if (is_return) {
                            if (tag.cmt_start < 0)
                                tag.cmt_start = i;
                        }
                    }
                }
                else {
		            if (c == '@') {
		                inside_cmd = true;
		                is_line_cmd = true;
		            }
		            if (c != '*') {
		            	if (dline.start < 0)
		            		dline.start = i;
		            }
                }
            }
            else {
            	bool is_ws = c == '\t' || c == '\r' || c == '\n' || c == ' ';
            	bool is_nonname = c != '_' && (c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z');

                if (doc.main.code_start >= 0) {
                	if (is_nonname) {
						uint64_t prev7 = last8 >> 8;
						int wlen = i - last_nonname_idx - 1;
						if (wlen == 3 && ((prev7 << 40) >> 40) == 0x66756eLL) { // fun
							doc.flags |= DOC_FLAG_KOTLIN | DOC_FLAG_METHOD;
						}
						else if (wlen == 3 && ((prev7 << 40) >> 40) == 0x766172LL) { // var
							doc.flags |= DOC_FLAG_KOTLIN | DOC_FLAG_FIELD;
						}
						else if (wlen == 3 && ((prev7 << 40) >> 40) == 0x76616cLL) { // val
							doc.flags |= DOC_FLAG_KOTLIN | DOC_FLAG_FIELD | DOC_FLAG_FINAL;
						}
						else if (wlen == 5 && ((prev7 << 24) >> 24) == 0x66696e616cLL) { // final
							doc.flags |= DOC_FLAG_FINAL;
						}
						else if (wlen == 6 && ((prev7 << 16) >> 16) == 0x737461746963LL) { // static
							doc.flags |= DOC_FLAG_STATIC;
						}
						else if (wlen == 12 && prev7 == 0x726f6e697a6564LL) { // ronized (synchronized)
							doc.flags |= DOC_FLAG_SYNC;
						}
						else if (wlen == 6 && ((prev7 << 16) >> 16) == 0x7075626c6963LL) { // public
							doc.access = DOC_ACCESS_PUBLIC;
						}
						else if (wlen == 9 && prev7 == 0x6f746563746564LL) { // otected (protected)
							doc.access = DOC_ACCESS_PROTECTED;
						}
						else if (wlen == 7 && prev7 == 0x70726976617465LL) { // private
							doc.access = DOC_ACCESS_PRIVATE;
						}
						else if (wlen >= 1) {
							if (!seen_code_atsym && (doc.flags & (DOC_FLAG_COLON | DOC_FLAG_SEMIC | DOC_FLAG_CURLY | DOC_FLAG_PAREN | DOC_FLAG_EQUALS)) == 0) {
								doc.name.start = last_nonname_idx + 1;
								doc.name.end = i - 1;
							}
						}
					}

					if (c == ':') {
						doc.flags |= DOC_FLAG_COLON;
					}
					if (c == ';') {
                        seen_semicolon = true;
                        doc.flags |= DOC_FLAG_SEMIC;
                        doc.main.code_end = i - 1;
                    }
                    else if (c == '{') {
                        seen_curly = true;
                        doc.flags |= DOC_FLAG_CURLY;
                        doc.main.code_end = i - 1;
                    }
                    else if (c == '(') {
                        seen_paren = true;
                        doc.flags |= DOC_FLAG_PAREN;
                        n_open_paren++;
                    }
                    else if (c == ')') {
                        n_open_paren--;
                        doc.main.code_end = i;
                    }
                    else if (c == '=') {
                    	doc.flags |= DOC_FLAG_EQUALS;
                    }

                    if (c == '\n' && n_open_paren == 0) {
                        if (seen_semicolon || seen_curly || seen_paren) {
                            if (doc.main.code_end < 0)
                            	doc.main.code_end = i;

                            add_doc(source, &doc);
                        }
                    }
                }
                else {
                	if (c == '(')
                		n_open_paren++;
                	else if (c == ')')
                		n_open_paren--;

                	if (seen_code_atsym && is_ws && n_open_paren == 0)
                		seen_code_atsym = false;

                    if (is_ws) {
                        seen_ws = true;
                    }
                    else if (seen_ws) {
                    	if (c == '@') {
                    		seen_ws = false;
                    		seen_code_atsym = true;
                		}
                		else if (!seen_code_atsym) {
		                    doc.main.code_start = i;
		                    doc.code_lineno = n_lines;
	                    }
                    }
                }
            }
        }

		if (c != '_' && (c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z'))
			last_nonname_idx = i;
    }

	sort_source_docs(source);
}
