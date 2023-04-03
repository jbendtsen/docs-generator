#include "docs-generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void print_help()
{
	puts(
		"Documentation Generator\n\n"
		"Options:\n"
		"   --sort <method>\n"
		"      Sort fields and methods by a particular order\n"
		"      Supported: \"alpha\", \"content\"\n"
		"      Default: \"content\"\n"
		"   --yes-list <text file>\n"
		"      File listing every source file to generate docs from\n"
		"   --no-list <text file>\n"
		"      File listing every file to exclude\n"
		"   --exts <list of file extensions>\n"
		"      Comma separated without spaces, eg. \"java,kt\"\n"
		"   --in-single <source file>\n"
		"      Add one source file to the list of inputs\n"
		"   --in-zip <ZIP-compatible file>\n"
		"      Archive containing input source files\n"
		"   --in-folder <folder>\n"
		"      Folder containing input source files\n"
		"   --out-single <HTML file>\n"
		"      Generate an HTML file\n"
		"      Only valid if there is only one input source file\n"
		"   --out-zip <ZIP file>\n"
		"      Output to a new ZIP file\n"
		"      NOTE: this operation deletes and replaces any existing output file\n"
		"   --out-folder <folder>\n"
		"      Output to a new folder\n"
		"      NOTE: this operation deletes and replaces any existing output folder\n"
		"   --css <file>\n"
		"      Select the CSS file to use\n"
		"      Defaults to \"style.css\"\n"
		"   --embed-css <mode>\n"
		"      Set whether the CSS file will be embedded into HTML or kept separate\n"
		"      Supported modes are \"always\", \"never\" or \"auto\"\n"
		"      Defaults to \"auto\", where the CSS is embedded only if\n"
		"       exactly one source file is given\n\n"
		"If any --in-* command is given \"-\", contents will be read from stdin.\n"
		"If any --out-* command is given \"-\", contents will be written to stdout.\n"
	);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		print_help();
		return 1;
	}

	char *style_css_name = NULL;

	int sort_order = SORT_CONTENT;
	int embed_css_mode = EMBED_AUTO;

	Vector source_name_list = {0};
	Vector yes_list = {0};
	Vector no_list = {0};

	for (int i = 1; i < argc-1; i += 2) {
		if (!strcmp(argv[i], "--sort")) {
			if (strlen(argv[i+1]) >= 5 && memcmp(argv[i+1], "alpha", 5) == 0)
				sort_order = SORT_ALPHA;
		}
		else if (!strcmp(argv[i], "--yes-list")) {
			
		}
		else if (!strcmp(argv[i], "--no-list")) {
			
		}
		else if (!strcmp(argv[i], "--exts")) {
			
		}
		else if (!strcmp(argv[i], "--in-single")) {
			vector_append_cstring(&source_name_list, argv[i+1]);
			*(char*)vector_add(&source_name_list, 1, 1) = '\0';
		}
		else if (!strcmp(argv[i], "--in-zip")) {
			
		}
		else if (!strcmp(argv[i], "--in-folder")) {
			
		}
		else if (!strcmp(argv[i], "--out-single")) {
			
		}
		else if (!strcmp(argv[i], "--out-zip")) {
			
		}
		else if (!strcmp(argv[i], "--out-folder")) {
			
		}
		else if (!strcmp(argv[i], "--css")) {
			if (style_css_name)
				free(style_css_name);

			int len = strlen(argv[i+1]);
			style_css_name = malloc(len + 1);
			strcpy(style_css_name, argv[i+1]);
		}
		else if (!strcmp(argv[i], "--embed-css")) {
			if (!strcmp(argv[i+1], "always"))
				embed_css_mode = EMBED_ALWAYS;
			else if (!strcmp(argv[i+1], "never"))
				embed_css_mode = EMBED_NEVER;
		}
		else {
			print_help();
			return strcmp(argv[i], "--help") != 0;
		}
	}

	if (!source_name_list.buf) {
		print_help();
		return 1;
	}

	if (!style_css_name) {
		const char *default_name = "style.css";
		style_css_name = malloc(strlen(default_name) + 1);
		strcpy(style_css_name, default_name);
	}

	File css_file = read_whole_file(style_css_name);
	if (!css_file.buf)
		return 2;

	Vector source_list = {0};

	*(char*)vector_add(&source_name_list, 1, 1) = '\0';
	char *fname = (char*)source_name_list.buf;
	bool missing_sources = false;

	while (*fname) {
		int name_len = strlen(fname);

		Source source = {0};
		source.file = read_whole_file(fname);
		source.sort_order = sort_order;
		source.access_level = DOC_ACCESS_PRIVATE;
		*(Source*)vector_add(&source_list, sizeof(Source), 1) = source;

		if (!source.file.buf) {
			missing_sources = true;
			break;
		}

		fname += name_len + 1;
	}

	bool should_embed_css = embed_css_mode == EMBED_AUTO ?
		source_list.n == 1 :
		embed_css_mode == EMBED_ALWAYS;

	for (int i = 0; i < source_list.n; i++) {
		Source *s = &((Source*)source_list.buf)[i];
		if (!missing_sources) {
			parse_source_file(s);
			generate_html(s, &css_file, should_embed_css);
		}
		source_close(s);
	}

    return 0;
}
