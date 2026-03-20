#ifndef LETISCH_H_
#define LETISCH_H_

#define LS_QUEUE_MAX 1024

typedef struct LSQueue {
	struct mg_connection* buf[LS_QUEUE_MAX];
	size_t head;
	size_t tail;
} LSQueue;

typedef struct LSMgr {
	struct mg_connection* conn;
	LSQueue queue;
} LSMgr;

extern LSMgr lsmgr;

void LSConnect(struct mg_mgr* mgr);
void LSFetchTeacherSchedule(struct mg_connection* c, char* id);
void LSHandler(struct mg_connection* c, int ev, void* ev_data);

#endif /* LETISCH_H_ */

#ifdef LETISCH_IMPLEMENTATION

LSMgr lsmgr;

void LSConnect(struct mg_mgr* mgr) {
	lsmgr.conn = mg_http_connect(mgr, "https://digital.etu.ru", LSHandler, NULL);
}

void LSFetchTeacherSchedule(struct mg_connection* c, char* id) {
	lsmgr.queue.buf[lsmgr.queue.tail] = c;
	lsmgr.queue.tail = (lsmgr.queue.tail + 1) % LS_QUEUE_MAX;
	char* msg = nob_temp_sprintf(
			"GET %s%s HTTP/1.1\r\n"
			"Host: digital.etu.ru\r\n"
			"User-Agent: curl/8.19.0\r\n"
			"Accept: */*\r\n\r\n",
			"/api/schedule/objects/publicated?subjectType=%D0%9B%D0%B5%D0%BA&subjectType=%D0%9F%D1%80&subjectType=%D0%9B%D0%B0%D0%B1&subjectType=%D0%9A%D0%9F&subjectType=%D0%9A%D0%A0&subjectType=%D0%94%D0%BE%D0%B1&subjectType=%D0%9C%D0%AD%D0%BA&subjectType=%D0%9F%D1%80%D0%B0%D0%BA&subjectType=%D0%A2%D0%B5%D1%81%D1%82&withSubjectCode=true&withURL=true&noEmptyGroups=true&teacherRequired=true&withFaculty=true&anyTeacherId=", id);
	nob_temp_reset();
	mg_send(lsmgr.conn, msg, strlen(msg));
}

void LSHandler(struct mg_connection* c, int ev, void* ev_data) {
	switch (ev) {
		case MG_EV_CONNECT:
			MG_INFO(("CONNECTION\n"));
			struct mg_str ca = mg_file_read(&mg_fs_posix, "/etc/ssl/certs/ca-certificates.pem");
			struct mg_tls_opts opts = { .ca=ca, .name=mg_str("digital.etu.ru") };
			mg_tls_init(c, &opts);
			break;
		case MG_EV_TLS_HS:
			MG_INFO(("HANDSHAKE\n"));
			break;
		case MG_EV_HTTP_MSG:
			// TODO: safe proxy (connection closes case)
			MG_INFO(("MSG\n"));
			struct mg_http_message* hm = (struct mg_http_message*)ev_data;
			MG_INFO(("'%.*s'", (int)hm->message.len, hm->message.buf));
			mg_http_reply(lsmgr.queue.buf[lsmgr.queue.head], 200, "", "%.*s", (int)hm->body.len, hm->body.buf);
			lsmgr.queue.head = (lsmgr.queue.head + 1) % LS_QUEUE_MAX;
			break;
		case MG_EV_ERROR:
			MG_INFO(("ERROR '%s'\n", ev_data));
			break;
		case MG_EV_CLOSE:
			LSConnect(c->mgr);
			break;
	}
}

#endif /* LETISCH_IMPLEMENTATION */
