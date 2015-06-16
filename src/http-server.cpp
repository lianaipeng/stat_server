/*
  A trivial static http webserver using Libevent's evhttp.

  This is not the best code in the world, and it does some fairly stupid stuff
  that you would never want to do in a production webserver. Caveat hackor!

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include "config.h"
#include <iostream>
#include <boost/lexical_cast.hpp>

#ifdef _EVENT_HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

/* Compatibility for possible missing IPv6 declarations */
#include "util-internal.h"

#include "stat-online.h"
#include "stat-response.h"

char uri_root[512];

static const struct table_entry {
	const char *extension;
	const char *content_type;
} content_type_table[] = {
	{ "txt", "text/plain" },
	{ "c", "text/plain" },
	{ "h", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/htm" },
	{ "css", "text/css" },
	{ "gif", "image/gif" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "png", "image/png" },
	{ "pdf", "application/pdf" },
    { "json", "application/json" },
	{ "ps", "application/postsript" },
	{ NULL, NULL },
};

/* Try to guess a good content-type for 'path' */
static const char *
guess_content_type(const char *path)
{
	const char *last_period, *extension;
	const struct table_entry *ent;
	last_period = strrchr(path, '.');
	if (!last_period || strchr(last_period, '/'))
		goto not_found; /* no exension */
	extension = last_period + 1;
	for (ent = &content_type_table[0]; ent->extension; ++ent) {
		if (!evutil_ascii_strcasecmp(ent->extension, extension))
			return ent->content_type;
	}

not_found:
	return "application/misc";
}

/* Callback used for the /dump URI, and for every non-GET request:
 * dumps all information to stdout and gives back a trivial 200 ok */
static void
dump_request_cb(struct evhttp_request *req, void *arg)
{
	const char *cmdtype;
	struct evkeyvalq *headers;
	struct evkeyval *header;
	struct evbuffer *buf;

	switch (evhttp_request_get_command(req)) {
	case EVHTTP_REQ_GET: cmdtype = "GET"; break;
	case EVHTTP_REQ_POST: cmdtype = "POST"; break;
	case EVHTTP_REQ_HEAD: cmdtype = "HEAD"; break;
	case EVHTTP_REQ_PUT: cmdtype = "PUT"; break;
	case EVHTTP_REQ_DELETE: cmdtype = "DELETE"; break;
	case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
	case EVHTTP_REQ_TRACE: cmdtype = "TRACE"; break;
	case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
	case EVHTTP_REQ_PATCH: cmdtype = "PATCH"; break;
	default: cmdtype = "unknown"; break;
	}

	printf("Received a %s request for %s\nHeaders:\n",
	    cmdtype, evhttp_request_get_uri(req));

	headers = evhttp_request_get_input_headers(req);
	for (header = headers->tqh_first; header;
	    header = header->next.tqe_next) {
		printf("  %s: %s\n", header->key, header->value);
	}

	buf = evhttp_request_get_input_buffer(req);
	puts("Input data: <<<");
	while (evbuffer_get_length(buf)) {
		int32_t n;
		char cbuf[128];
		n = evbuffer_remove(buf, cbuf, sizeof(buf)-1);
		if (n > 0)
			(void) fwrite(cbuf, 1, n, stdout);
	}
	puts(">>>");
}

/* This callback gets invoked when we get any http request that doesn't match
 * any other callback.  Like any evhttp server callback, it has a simple job:
 * it must eventually call evhttp_send_error() or evhttp_send_reply().
 */
static void
send_document_cb(struct evhttp_request *req, void *arg)
{
	struct evbuffer *evb = NULL;
	//const char *docroot = reinterpret_cast<char *>(arg);
	const char *uri = evhttp_request_get_uri(req);
	struct evhttp_uri *decoded = NULL;
	const char *path;
	char *decoded_path;
	//char *whole_path = NULL;
	size_t len;
	int32_t fd = -1;
	struct stat st;

	if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
		dump_request_cb(req, arg);
        evhttp_send_reply(req, 200, "OK", NULL);
		return;
	}

	printf("Got a GET request for <%s>\n",  uri);

	/* Decode the URI */
	decoded = evhttp_uri_parse(uri);
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

	/* Let's see what path the user asked for. */
	path = evhttp_uri_get_path(decoded);
	if (!path) path = "/";

	/* We need to decode it, to see what path the user really wanted. */
	decoded_path = evhttp_uridecode(path, 0, NULL);
	if (decoded_path == NULL)
		goto err;
	/* Don't allow any ".."s in the path, to avoid exposing stuff outside
	 * of the docroot.  This test is both overzealous and underzealous:
	 * it forbids aceptable paths like "/this/one..here", but it doesn't
	 * do anything to prevent symlink following." */
	if (strstr(decoded_path, ".."))
		goto err;

err:
	evhttp_send_error(req, 404, "Not found");
	if (fd>=0)
		close(fd);
done:
	if (decoded)
		evhttp_uri_free(decoded);
	if (decoded_path)
		free(decoded_path);
}

static void
customer_online_stat_cb(struct evhttp_request *req, void *arg)
{
    dump_request_cb(req, arg);

    const char* uri = evhttp_request_get_uri(req);
    printf("*************************************************uri = %s\n", uri);
    char* decoded_uri = evhttp_decode_uri(uri);
    printf("*************************************************decoded_uri = %s\n", decoded_uri);
    struct evkeyvalq params;
    struct evbuffer* evb = evbuffer_new();
    
    evhttp_parse_query(decoded_uri, &params);
    char* date_key = (char*)"date";
    const char* date_value = evhttp_find_header(&params, date_key);
    printf("************************************************* date = %s\n", date_value);
    char* days_key = (char*)"days";
    const char* days_value = evhttp_find_header(&params, days_key);
    printf("************************************************* days = %s\n", days_value);
    char* business_id_str_list_key = (char*)"business_id_list";
    const char* business_id_str_list_value = evhttp_find_header(&params, business_id_str_list_key);
    printf("************************************************* business_id_list = %s\n", business_id_str_list_value);

    if((date_value==NULL)||(days_value==NULL)||(business_id_str_list_value==NULL)){
        evhttp_send_error(req, 400, "Bad Request");    
    }
    std::string data;
    std::vector<online_t> stat_online_return;
    getOnlineStat(business_id_str_list_value, date_value, boost::lexical_cast<int32_t>(days_value), stat_online_return);
    onlineToJsonStr(stat_online_return, data);

    evbuffer_add_printf(evb, data.c_str());
    evhttp_send_reply(req, 200, "OK", evb);
}

static void
customer_response_stat_cb(struct evhttp_request *req, void *arg)
{
    dump_request_cb(req, arg);

    const char* uri = evhttp_request_get_uri(req);
    printf("************************************************* uri = %s\n", uri);
    char* decoded_uri = evhttp_decode_uri(uri);
    printf("************************************************* decoded_uri = %s\n", decoded_uri);
    struct evkeyvalq params;
    struct evbuffer* evb = evbuffer_new();

    evhttp_parse_query(decoded_uri, &params);
    char* date_key = (char*)"date";
    const char* date_value = evhttp_find_header(&params, date_key);
    printf("************************************************* date = %s\n", date_value);
    char* days_key = (char*)"days";
    const char* days_value = evhttp_find_header(&params, days_key);
    printf("************************************************* days = %s\n", days_value);
    char* business_id_str_list_key = (char*)"business_id_list";
    const char* business_id_str_list_value = evhttp_find_header(&params, business_id_str_list_key);
    printf("************************************************* business_id_list = %s\n", business_id_str_list_value);

    if((date_value==NULL)||(days_value==NULL)||(business_id_str_list_value==NULL)){
        evhttp_send_error(req, 400, "Bad Request");    
    }
    std::string data;
    std::vector<response_t> stat_response_return;
    getResponseStat(business_id_str_list_value, date_value, boost::lexical_cast<int32_t>(days_value), stat_response_return);
    responseToJsonStr(stat_response_return, data);

    evbuffer_add_printf(evb, data.c_str());
    evhttp_send_reply(req, 200, "OK", evb);
}

int32_t
main(int32_t argc, char **argv)
{
	struct event_base *base;
	struct evhttp *http;
	struct evhttp_bound_socket *handle;

	
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return (1);

    initConfig(argv[1]);

    std::string port = getConfig("server.port");
    std::string addr = getConfig("server.addr");

	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Couldn't create an event_base: exiting\n");
		return 1;
	}

	/* Create a new evhttp object to handle requests. */
	http = evhttp_new(base);
	if (!http) {
		fprintf(stderr, "couldn't create evhttp. Exiting.\n");
		return 1;
	}

	evhttp_set_cb(http, "/customer/online_duration_static", customer_online_stat_cb, NULL);
    evhttp_set_cb(http, "/customer/response_time_static", customer_response_stat_cb, NULL);

	/* We want to accept arbitrary requests, so we need to set a "generic"
	 * cb.  We can also add callbacks for specific paths. */
	evhttp_set_gencb(http, send_document_cb, NULL);

	/* Now we tell the evhttp what port to listen on */
	handle = evhttp_bind_socket_with_handle(http, addr.c_str(), boost::lexical_cast<int32_t>(port));
	if (!handle) {
		fprintf(stderr, "couldn't bind to port %s. Exiting.\n",
		    port.c_str());
		return 1;
	}

	{
		/* Extract and display the address we're listening on. */
		struct sockaddr_storage ss;
		evutil_socket_t fd;
		ev_socklen_t socklen = sizeof(ss);
		char addrbuf[128];
		void *inaddr;
		const char *addr;
		int32_t got_port = -1;
		fd = evhttp_bound_socket_get_fd(handle);
		memset(&ss, 0, sizeof(ss));
		if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
			perror("getsockname() failed");
			return 1;
		}
		if (ss.ss_family == AF_INET) {
			got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
			inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
		} else if (ss.ss_family == AF_INET6) {
			got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
			inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
		} else {
			fprintf(stderr, "Weird address family %d\n",
			    ss.ss_family);
			return 1;
		}
		addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf,
		    sizeof(addrbuf));
		if (addr) {
			printf("Listening on %s:%d\n", addr, got_port);
			evutil_snprintf(uri_root, sizeof(uri_root),
			    "http://%s:%d",addr,got_port);
		} else {
			fprintf(stderr, "evutil_inet_ntop failed\n");
			return 1;
		}
	}

	event_base_dispatch(base);

	return 0;
}
