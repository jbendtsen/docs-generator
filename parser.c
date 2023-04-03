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
    d->parent_doc = -1;
    d->first_desc_line = -1;
    d->n_desc_lines = 0;
    d->first_param = -1;
    d->n_params = 0;
    d->code_lineno = -1;
    d->flags = 0;
    d->access = 0;
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
    if (dline->end < dline->start)
        return;

    if (doc->first_desc_line < 0) {
        int i = dline->start;
        bool is_empty = true;

        // only discard empty lines before the first non-empty line
        while (i > 0 && i <= dline->end) {
            char c = source->file.buf[i];
            if (c != '*' && c != '\t' && c != '\r' && c != '\n' && c != ' ') {
                is_empty = false;
                break;
            }
            i++;
        }
        if (is_empty)
            return;

        doc->first_desc_line = source->descs.n;
    }

    Span *new_span = vector_add(&source->descs, sizeof(Span), 1);
    *new_span = *dline;
    doc->n_desc_lines++;

    span_reset(dline);
}

void maybe_add_doc(Source *source, Doc *doc, int64_t *class_index, int *class_level, bool curly_open_not_closed, int n_open_curly)
{
    doc->parent_doc = *class_level >= 0 ? class_index[*class_level] & 0x7fffFFFF : -1;

    const char *in = source->file.buf;

    if ((~doc->flags & (DOC_FLAG_PAREN | DOC_FLAG_CURLY | DOC_FLAG_EQUALS)) == DOC_FLAG_EQUALS) {
        bool is_ctor = false;
        if (doc->name.start >= 0 && doc->name.end >= doc->name.start) {
            int class_name_start = source->class_name.start;
            int class_name_end   = source->class_name.end;

            if (doc->parent_doc >= 0) {
                Doc *parent = &source->docs.buf[doc->parent_doc];
                class_name_start = parent->name.start;
                class_name_end   = parent->name.end;
            }

            if (doc->name.end - doc->name.start == class_name_end - class_name_start)
                is_ctor = !memcmp(&in[doc->name.start], &in[class_name_start], class_name_end - class_name_start);
        }

        if (is_ctor)
            doc->flags |= DOC_FLAG_CTOR;
        else
            doc->flags |= DOC_FLAG_METHOD;
    }

    if (doc->flags & DOC_FLAG_SEMIC)
        doc->flags |= DOC_FLAG_FIELD;

    if (doc->flags & (DOC_FLAG_CLASS | DOC_FLAG_STRUCT | DOC_FLAG_INTERFACE | DOC_FLAG_EXTENSION))
        doc->flags |= DOC_FLAG_IS_PARENT;

    if ((~doc->flags & (DOC_FLAG_IS_PARENT | DOC_FLAG_COLON)) == 0)
        doc->flags |= DOC_FLAG_INHERITS;

    if ((doc->flags & DOC_FLAG_IS_PARENT) && curly_open_not_closed && *class_level < MAX_CLASS_LEVELS-1) {
        *class_level += 1;
        class_index[*class_level] = ((int64_t)n_open_curly << 32) | (int64_t)source->docs.n;
    }

    //if ((doc->flags & DOC_FLAG_IS_PARENT) || (doc->main.cmt_start >= 0 && doc->main.code_start >= 0)) {
    if (
        doc->access >= source->access_level ||
        (doc->flags & DOC_FLAG_IS_PARENT) ||
        (doc->main.cmt_start >= 0 && doc->main.code_start >= 0)
    ) {
        Doc *new_doc = vector_add(&source->docs, sizeof(Doc), 1);
        *new_doc = *doc;
    }

    doc_reset(doc);
}

void parse_source_file(Source *source)
{
	int companion_brace_level = -1;

    bool is_javadoc = false;
    bool is_block_comment = false;
    bool is_line_comment = false;
    bool inside_cmd = false;
    bool is_line_cmd = false;
    bool is_param = false;
    bool is_return = false;
    bool seen_ws = false;
    bool seen_code_atsym = false;
    bool seen_semicolon = false;
    bool seen_open_curly = false;
    bool seen_close_curly = false;
    bool seen_paren = false;
    int n_open_paren = 0;
    int n_lines = 0;
    int last_nonname_idx = -1;
    uint64_t last16 = 0;
    uint64_t last8 = 0;
    int utf8_left = 0;

    int n_open_curly = 0;
    int class_level = -1;
    int64_t class_index[MAX_CLASS_LEVELS];

    Doc doc; doc_reset(&doc);
    Tag tag; tag_reset(&tag);
    Span dline; span_reset(&dline);

	char *buf = source->file.buf;
	int sz = source->file.size;
    for (int i = 0; i < sz; i++) {
        char c = buf[i];
        last16 = (last16 << 8LL) | (last8 >> 56LL);
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

        if (c == '\n') {
            n_lines++;
            is_line_comment = false;
        }

        if (i >= 1 && buf[i-1] == '/' && c == '/')
            is_line_comment = true;
        if (i >= 1 && buf[i-1] == '/' && c == '*')
            is_block_comment = true;

        if (!is_javadoc && i >= 2 && buf[i-2] == '/' && buf[i-1] == '*' && c == '*') {
            is_javadoc = true;

            doc.main.code_end = i-3;
            maybe_add_doc(source, &doc, class_index, &class_level, seen_open_curly && !seen_close_curly, n_open_curly);

            doc.main.cmt_start = i-2;
        }
        else if (i >= 1 && buf[i-1] == '*' && c == '/') {
            is_javadoc = false;
            is_block_comment = false;
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
            seen_open_curly = false;
            seen_close_curly = false;
            seen_paren = false;
            n_open_paren = 0;
        }

        if (c == '{') {
            n_open_curly++;
        }
        else if (c == '}') {
            if (class_level >= 0 && (class_index[class_level] >> 32) == n_open_curly)
                class_level--;
            n_open_curly--;
        }

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
        else if (!is_block_comment && !is_line_comment) { // else if (doc.main.cmt_start >= 0) 
            bool is_ws = c == '\t' || c == '\r' || c == '\n' || c == ' ';
            bool is_nonname = c != '_' && (c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z');

            if (doc.main.code_start >= 0) {
                if (is_nonname) {
                    uint64_t prev15 = last16;
                    uint64_t prev7 = last8 >> 8;
                    int wlen = i - last_nonname_idx - 1;
                    if (wlen == 3 && ((prev7 << 40) >> 40) == 0x66756eLL) { // fun
                        doc.flags |= DOC_FLAG_KOTLIN | DOC_FLAG_METHOD;
                    }
                    else if (wlen == 4 && ((prev7 << 32) >> 32) == 0x66756e63LL) { // func
                        doc.flags |= DOC_FLAG_SWIFT | DOC_FLAG_METHOD;
                    }
                    else if (wlen == 3 && ((prev7 << 40) >> 40) == 0x766172LL) { // var
                        doc.flags |= DOC_FLAG_KOTLIN | DOC_FLAG_SWIFT | DOC_FLAG_FIELD;
                    }
                    else if (wlen == 3 && ((prev7 << 40) >> 40) == 0x76616cLL) { // val
                        doc.flags |= DOC_FLAG_KOTLIN | DOC_FLAG_FIELD | DOC_FLAG_FINAL;
                    }
                    else if (wlen == 3 && ((prev7 << 40) >> 40) == 0x6c6574LL) { // let
                        doc.flags |= DOC_FLAG_SWIFT | DOC_FLAG_FIELD | DOC_FLAG_FINAL;
                    }
                    else if (wlen == 5 && ((prev7 << 24) >> 24) == 0x66696e616cLL) { // final
                        doc.flags |= DOC_FLAG_FINAL;
                    }
                    else if (wlen == 6 && ((prev7 << 16) >> 16) == 0x737461746963LL) { // static
                        doc.flags |= DOC_FLAG_STATIC;
                    }
                    else if (wlen == 6 && ((prev7 << 16) >> 16) == 0x737472756374LL) { // struct
                        doc.flags |= DOC_FLAG_STRUCT;
                    }
                    else if (wlen == 5 && ((prev7 << 24) >> 24) == 0x636c617373LL) { // class
                        doc.flags |= DOC_FLAG_CLASS;
                    }
                    else if (wlen == 9 && (prev15 & 0xffff) == 0x696e && prev7 == 0x74657266616365LL) { // interface
                        doc.flags |= DOC_FLAG_INTERFACE;
                    }
                    else if (wlen == 8 && (prev15 & 0xff) == 0x70 && prev7 == 0x726f746f636f6cLL) { // protocol
                        doc.flags |= DOC_FLAG_SWIFT | DOC_FLAG_INTERFACE;
                    }
                    else if (wlen == 9 && (prev15 & 0xffff) == 0x6578 && prev7 == 0x74656e73696f6eLL) { // extension
                        doc.flags |= DOC_FLAG_EXTENSION;
                    }
                    else if (wlen == 8 && (prev15 & 0xff) == 0x61 && prev7 == 0x62737472616374LL) { // abstract
                        doc.flags |= DOC_FLAG_ABSTRACT;
                    }
                    else if (
                        (wlen == 7 && prev7 == 0x657874656e6473LL) || // extends
                        (wlen == 10 && (prev15 & 0xffffff) == 0x696d70 && prev7 == 0x6c656d656e7473LL) // implements
                    ) {
                        doc.flags |= DOC_FLAG_INHERITS;
                    }
                    else if (wlen == 12 && ((prev15 << 24) >> 24) == 0x73796e6368LL && prev7 == 0x726f6e697a6564LL) { // synchronized
                        doc.flags |= DOC_FLAG_SYNC;
                    }
                    else if (wlen == 6 && ((prev7 << 16) >> 16) == 0x7075626c6963LL) { // public
                        doc.access = DOC_ACCESS_PUBLIC;
                    }
                    else if (wlen == 9 && (prev15 & 0xffff) == 0x7072 && prev7 == 0x6f746563746564LL) { // protected
                        doc.access = DOC_ACCESS_PROTECTED;
                    }
                    else if (wlen == 7 && prev7 == 0x70726976617465LL) { // private
                        doc.access = DOC_ACCESS_PRIVATE;
                    }
                    else if (wlen >= 1) {
                        if (!seen_code_atsym && (doc.flags & (DOC_FLAG_INHERITS | DOC_FLAG_COLON | DOC_FLAG_SEMIC | DOC_FLAG_CURLY | DOC_FLAG_PAREN | DOC_FLAG_EQUALS)) == 0) {
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
                    if (doc.main.code_end < 0)
                        doc.main.code_end = i - 1;
                }
                else if (c == '{') {
                    seen_open_curly = true;
                    doc.flags |= DOC_FLAG_CURLY;
                    if (doc.main.code_end < 0)
                        doc.main.code_end = i - 1;
                }
                else if (c == '}') {
                    seen_close_curly = true;
                }
                else if (c == '(') {
                    seen_paren = true;
                    doc.flags |= DOC_FLAG_PAREN;
                    n_open_paren++;
                }
                else if (c == ')') {
                    n_open_paren--;
                    if (doc.main.code_end < 0)
                        doc.main.code_end = i;
                }
                else if (c == '=') {
                    doc.flags |= DOC_FLAG_EQUALS;
                    doc.main.code_end = last_nonname_idx;
                }

                if (c == '\n' && n_open_paren == 0) {
                    if (seen_semicolon || seen_open_curly || seen_paren) {
                        if (doc.main.code_end < 0)
                            doc.main.code_end = i;

                        maybe_add_doc(source, &doc, class_index, &class_level, seen_open_curly && !seen_close_curly, n_open_curly);

                        seen_ws = false;
                        seen_code_atsym = false;
                        seen_semicolon = false;
                        seen_open_curly = false;
                        seen_close_curly = false;
                        seen_paren = false;
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

		if (c != '_' && (c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z'))
			last_nonname_idx = i;
    }
}
