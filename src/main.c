#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <time.h>

#define FLAG_IMPLEMENTATION
#include "flag.h"

#include "mongoose/mongoose.h"

#define NOB_IMPLEMENTATION
#include "nob.h"
#undef NOB_IMPLEMENTATION

#define ENUMS_IMPLEMENTATION
#include "enums.h"
#undef ENUMS_IMPLEMENTATION

#define BINARY_RW_IMPLEMENTATION
#include "binary_rw.h"
#undef BINARY_RW_IMPLEMENTATION

#define LETISCH_IMPLEMENTATION
#include "letisch.h"
#undef LETISCH_IMPLEMENTATION

// --- UTILS ---

void RandomBytes(void *buf, size_t len) {
	int fd = open("/dev/urandom", O_RDONLY);
	assert(fd >= 0);
	size_t n = read(fd, buf, len);
	assert(n == len);
	close(fd);
}

// --- GLOBALS ---

size_t active_conns = 0;

// --- SSR ---

#define VISITORS_IMPLEMENTATION
#include "visitors.h"
#undef VISITORS_IMPLEMENTATION

// --- APP ---

struct a_config {
	int port;
	char* web_dir;
} aconf;

void a_parse_flags(int argc, char** argv) {
	mg_log_set(MG_LL_NONE);

	bool* f_help = flag_bool("help", 0, "help");
	uint64_t* f_ll = flag_uint64("log-level", 0, "none, error, info, debug, verbose (0, 1, 2, 3, 4)");
	uint64_t* f_port = flag_uint64("port", 6969, "port for the server");
	char** f_web_dir = flag_str("webdir", "./web", "directory for the server");

	if (!flag_parse(argc, argv)) {
    flag_print_options(stdout);
		flag_print_error(stderr);
		exit(1);
	}

	if (*f_help) {
    flag_print_options(stdout);
		exit(0);
	}

	aconf.web_dir = *f_web_dir;
	aconf.port = *f_port;
	mg_log_set(*f_ll);
}

// --- EVENTS ---

// -- HTTP --

char URICompare(struct mg_str uri, struct mg_str exp) {
	if (uri.len < exp.len) return 0;
	if (strncmp(uri.buf, exp.buf, exp.len) != 0) return 0;
	for (size_t i = exp.len; i < uri.len; i++) { // Trailing '/'
		if (uri.buf[i] != '/') return 0;
	}
	return 1;
}

char HTTPIsLangRu(struct mg_http_message* hm) {
	struct mg_str var_lang = mg_http_var(hm->query, mg_str("lang"));
	if (var_lang.len >= 2)
		return strncmp("ru", var_lang.buf, 2) == 0;
	struct mg_str* header_lang = mg_http_get_header(hm, "Accept-Language");
	if (header_lang && header_lang->len >= 2)
		return strncmp("ru", header_lang->buf, 2) == 0;
	return 0;
}

// TODO: find this thing purpose
//bool ConnCooldown(struct mg_connection* c) {
//	ConnData* cd = (ConnData*)c->fn_data;
//	uint64_t curr_sec = nob_nanos_since_unspecified_epoch() / NOB_NANOS_PER_SEC;
//	uint64_t last_sec = cd->last_msg_nanos / NOB_NANOS_PER_SEC;
//	bool result = curr_sec > last_sec + 10.0;
//	if (result) { cd->last_msg_nanos = nob_nanos_since_unspecified_epoch(); }
//	return result;
//}

void HandleAskmeQuestion(struct mg_connection* c, BReader* br) {
	char result = 0;
	uint64_t nanos = nob_nanos_since_unspecified_epoch();
	if (br->count > 256) { nob_return_defer(1); }
	if (br->count == 0) { nob_return_defer(2); }
	//if (!ConnCooldown(c)) { nob_return_defer(3); } // TODO: IP timeout
	nob_write_entire_file(nob_temp_sprintf("dbs/askme/%lu", nanos), br->data, br->count);
	nob_temp_reset();
defer:
	bw_temp.count = 0;
	BWriteU8(&bw_temp, result);
	Nob_String_Builder sb = {0};
	nob_sb_appendf(&sb,
			"HTTP/1.0 200 OK\r\n"
			"Content-Length: %zu\r\n"
			"Content-Type: application/octet-stream\r\n\r\n", bw_temp.count);
	nob_da_append_many(&sb, bw_temp.items, bw_temp.count);
	//mg_hexdump(sb.items, sb.count);
	mg_send(c, sb.items, sb.count);
	nob_sb_free(sb);
}

Nob_String_Builder http_page_string = {0};
Nob_String_Builder http_headers_string = {0};

void HandleHTTPMessage(struct mg_connection* c, void* ev_data) {

	http_page_string.count = 0;
	http_headers_string.count = 0;

	struct mg_http_message* hm = (struct mg_http_message*)ev_data;

	if (!mg_strcmp(hm->uri, mg_str("/ws"))) {
		mg_ws_upgrade(c, hm, NULL);
		return;
	}

	Visitor* visitor = HTTPProcessVisitor(hm);
	if (visitor == NULL) {
		visitor = HTTPAddPendingVisitor(&http_headers_string);
		if (visitor == NULL) { return; }
	}

	if (!mg_strcmp(hm->method, mg_str("POST"))) {
		if (!mg_strcmp(hm->uri, mg_str("/binary"))) {
			BReader br = { .data=hm->body.buf, .count=hm->body.len };
			uint16_t msg_type;
			// TODO: fix reloading freeze
			if (!BReadU16(&br, &msg_type)) { return; }
			switch (msg_type) {
			case CME_ASKME_QUESTION:
				HandleAskmeQuestion(c, &br);
				break;
			default:
				break;
			}
			return;
		}
	}

	if (!mg_strcmp(hm->method, mg_str("GET"))) {
		if (!mg_strcmp(hm->uri, mg_str("/letisch/teacher"))) {
			char id[32];
			if (mg_http_get_var(&hm->query, "id", id, 32) > 0) {
				LSFetchTeacherSchedule(c, id);
			}
			return;
		}
		if (!mg_strcmp(hm->uri, mg_str("/letisch/teacher_list"))) {
			LSFetchTeacherList(c);
			return;
		}
		struct mg_http_serve_opts opts = { .root_dir = aconf.web_dir };
		mg_http_serve_dir(c, hm, &opts);
		return;
	}
}

void HandleWSMessage(struct mg_connection* c, void* ev_data) {
	struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
	BReader br = {0};
	br.count = wm->data.len;
	br.data = wm->data.buf;
	uint16_t gcmt;
	//mg_hexdump(br.data, br.count);
	if (!BReadU16(&br, &gcmt)) { return; }
	switch (gcmt) {
		case CME_ASKME_QUESTION:
			//HandleAskmeQuestion(c, &br);
			break;
	}
}

void EventHandler(struct mg_connection* c, int ev, void* ev_data) {
	switch (ev) {
		case MG_EV_WS_MSG:
			NOB_ASSERT(c->fn_data);
			HandleWSMessage(c, ev_data);
			break;
		case MG_EV_HTTP_MSG:
			HandleHTTPMessage(c, ev_data);
			break;
		case MG_EV_POLL:
			VisitorsManageUnactive();
			break;
		case MG_EV_CLOSE:
			break;
	}
}


// --- MAIN ---

char is_working = 1;

void app_terminate(int sig) { is_working = 0; }

int main(int argc, char* argv[]) {
	nob_mkdir_if_not_exists("dbs");
	nob_mkdir_if_not_exists("dbs/askme");

	a_parse_flags(argc, argv);

	printf("log_level: %d\n", mg_log_level);
	printf("aconf.web_dir: %s\n", aconf.web_dir);
	printf("aconf.port: %d\n", aconf.port);

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	char addrstr[32];
	snprintf(addrstr, sizeof(addrstr), "http://0.0.0.0:%d", aconf.port);
	LSConnect(&mgr);
	mg_http_listen(&mgr, addrstr, EventHandler, NULL);

	signal(SIGINT, app_terminate);
	signal(SIGTERM, app_terminate);

	int count = 0;
	while (is_working) {
		mg_mgr_poll(&mgr, 1000);
	}

	// Closing
	LSDisconnect();
	mg_mgr_free(&mgr);
	printf("Server closed.\n");
	nob_sb_free(http_page_string);
	nob_sb_free(http_headers_string);

	return 0;
}
