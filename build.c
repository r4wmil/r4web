#define NOB_STRIP_PREFIX

#define NOB_IMPLEMENTATION
#include "src/nob.h"
#undef NOB_IMPLEMENTATION

#define ENUMS_IMPLEMENTATION
#include "src/enums.h"
#undef ENUMS_IMPLEMENTATION

String_Builder page_begin = {0};
String_Builder page_end = {0};

#define PAGE_CONVERT_SIMPLE(dir_) \
	do { \
		String_Builder sb = {0}; \
		NOB_ASSERT(nob_read_entire_file(dir_"/_index.html", &sb)); \
		NOB_ASSERT(nob_write_entire_file(dir_"/index.html", sb.items, sb.count)); \
	} while(0)

#define PAGE_CONVERT(dir_) \
	do { \
		String_Builder sb = {0}; \
		sb_append_buf(&sb, page_begin.items, page_begin.count); \
		NOB_ASSERT(nob_read_entire_file(dir_"/_index.html", &sb)); \
		sb_append_buf(&sb, page_end.items, page_end.count); \
		NOB_ASSERT(nob_write_entire_file(dir_"/index.html", sb.items, sb.count)); \
	} while(0)

#define CC "gcc"

int main(int argc, char** argv) {

	NOB_GO_REBUILD_URSELF(argc, argv);
	
	Cmd cmd = {0};

	if (!nob_mkdir_if_not_exists("out")) return 1;

	if (needs_rebuild1("out/mongoose.o", "src/3rd_party/mongoose/mongoose.c")) {
		cmd_append(&cmd, CC,
				"-c",
				"src/3rd_party/mongoose/mongoose.c",
				"-o",
				"out/mongoose.o",
				"-DMG_TLS=MG_TLS_BUILTIN");
		if (!cmd_run(&cmd)) return 1;
	}

	NOB_ASSERT(nob_read_entire_file("web/templates/page_begin.html", &page_begin));
	NOB_ASSERT(nob_read_entire_file("web/templates/page_end.html", &page_end));
	PAGE_CONVERT("web");
	PAGE_CONVERT("web/about");
	PAGE_CONVERT("web/404");
	PAGE_CONVERT("web/mna");
	PAGE_CONVERT("web/askme");
	PAGE_CONVERT("web/letisch");
	PAGE_CONVERT_SIMPLE("web/cells");
	Nob_String_Builder enums_js = enums_generate_js();
	NOB_ASSERT(nob_write_entire_file("web/enums.js", enums_js.items, enums_js.count)); \
	
	cmd_append(&cmd, CC, "src/main.c", "-o", "out/r4web");
	if (argc >= 2 && argv[1][0] == 's') { cmd_append(&cmd, "-fsanitize=address"); }
	cmd_append(&cmd, "out/mongoose.o");
	cmd_append(&cmd, "-I./src");
	cmd_append(&cmd, "-I./src/3rd_party");
	if (!cmd_run(&cmd)) return 1;

	return 0;
}
