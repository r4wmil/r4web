#define NOB_STRIP_PREFIX

#define SSR_CONVERT_IMPLEMENTATION
#include "src/ssr_convert.h"
#undef SSR_CONVERT_IMPLEMENTATION

/* Always last */
#define NOB_IMPLEMENTATION
#include "src/nob.h"
#undef NOB_IMPLEMENTATION

#define CC "gcc"

int main(int argc, char** argv) {

	NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "src/ssr_convert.h");
	
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

	//if (needs_rebuild1("ssr_convert", "ssr_convert.c")) {
	//	cmd_append(&cmd, CC, "ssr_convert.c", "-o", "ssr_convert");
	//	if (!cmd_run(&cmd)) return 1;
	//}

	if (!nob_mkdir_if_not_exists("src/ssr_generated")) return 1;
	SSRConvert("web/index.html", "src/ssr_generated/ssr_root.h", "ssr_root");
	SSRConvert("web/about.html", "src/ssr_generated/ssr_about.h", "ssr_about");
	SSRConvert("web/page404.html", "src/ssr_generated/ssr_page404.h", "ssr_page404");
	SSRConvert("web/proj/mna.html", "src/ssr_generated/ssr_proj_mna.h", "ssr_proj_mna");
	SSRConvert("web/proj/askme.html", "src/ssr_generated/ssr_proj_askme.h", "ssr_proj_askme");
	SSRConvert("web/templates/default-after.html", "src/ssr_generated/ssr_template_default_after.h", "ssr_template_default_after");
	SSRConvert("web/templates/default-before.html", "src/ssr_generated/ssr_template_default_before.h", "ssr_template_default_before");
	
	cmd_append(&cmd, CC, "src/main.c", "-o", "out/r4web");
	cmd_append(&cmd, "out/mongoose.o");
	cmd_append(&cmd, "-I./src");
	cmd_append(&cmd, "-I./src/3rd_party");
	if (!cmd_run(&cmd)) return 1;

	return 0;
}
