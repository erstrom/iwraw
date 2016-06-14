/*
 * Copyright (C) 2016  Erik Stromdahl
 * Heavily based on iw:
 * git://git.sipsolutions.net/iw.git
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdbool.h>
#include <getopt.h>

#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include "nl80211.h"
#include "log.h"
#include <iwraw_config.h>

#define NLA_INPUT_STREAM_MAX_LEN (1024)

int nl_get_multicast_id(struct nl_sock *sock, const char *family,
			const char *group);

enum nl80211_commands nl80211_cmd_from_str(const char *str);
void print_nl80211_cmds(void);

struct nl80211_state {
	struct nl_sock *nl_sock;
	int nl80211_id;
};

int log_level = LOG_WARNING;
bool log_stderr = true, log_initialized;

static bool print_ascii, dev_by_phy, devidx_set, cmd_set;
static uint32_t devidx;
static struct nl80211_state state;
static uint8_t nla_input_stream[NLA_INPUT_STREAM_MAX_LEN];
static enum nl80211_commands cur_cmd;

static int nl80211_init(void)
{
	int err;

	state.nl_sock = nl_socket_alloc();
	if (!state.nl_sock) {
		LOG_ERR_("Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	nl_socket_set_buffer_size(state.nl_sock, 8192, 8192);

	if (genl_connect(state.nl_sock)) {
		LOG_ERR_("Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	state.nl80211_id = genl_ctrl_resolve(state.nl_sock, "nl80211");
	if (state.nl80211_id < 0) {
		LOG_ERR_("nl80211 not found.\n");
		err = -ENOENT;
		goto out_handle_destroy;
	}

	return 0;

out_handle_destroy:
	nl_socket_free(state.nl_sock);

	return err;
}

static int write_ascii(int fd, const uint8_t *buf, int len)
{
	int i, n = 0;
	char *ascii_buf;

	ascii_buf = calloc(1, 3 * len + 2);
	if (!ascii_buf)
		return -ENOMEM;

	for (i = 0; i < len; i++)
		n += snprintf(ascii_buf + n, 3 * len + 2 - n, "%02X ", buf[i]);
	n += snprintf(ascii_buf + n, 3 * len + 2 - n, "\n");

	(void) write(fd, ascii_buf, n);
	free(ascii_buf);

	return NL_OK;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;

	LOG_DBG_("%s: ret %d\n", __func__, *ret);
	(void) nla;
	*ret = err->error;

	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;

	LOG_DBG_("%s: ret %d\n", __func__, *ret);
	(void) msg;
	*ret = 0;

	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;

	LOG_DBG_("%s: ret %d\n", __func__, *ret);
	(void) msg;
	*ret = 0;

	return NL_STOP;
}

static int valid_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *head_attr = genlmsg_attrdata(gnlh, 0);
	int attr_len = genlmsg_attrlen(gnlh, 0);

	LOG_DBG_("%s\n", __func__);
	(void) arg;
	if (print_ascii)
		return write_ascii(1, (uint8_t *) head_attr, attr_len);

	(void) write(1, (uint8_t *) head_attr, attr_len);

	return NL_OK;
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	(void) msg;
	(void) arg;

	return NL_OK;
}

static int prepare_listen_events(void)
{
	int mcid, ret;

	/* Configuration multicast group */
	mcid = nl_get_multicast_id(state.nl_sock, "nl80211", "config");
	if (mcid < 0)
		return mcid;

	ret = nl_socket_add_membership(state.nl_sock, mcid);
	if (ret)
		return ret;

	/* Scan multicast group */
	mcid = nl_get_multicast_id(state.nl_sock, "nl80211", "scan");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state.nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* Regulatory multicast group */
	mcid = nl_get_multicast_id(state.nl_sock, "nl80211", "regulatory");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state.nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* MLME multicast group */
	mcid = nl_get_multicast_id(state.nl_sock, "nl80211", "mlme");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state.nl_sock, mcid);
		if (ret)
			return ret;
	}

	mcid = nl_get_multicast_id(state.nl_sock, "nl80211", "vendor");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state.nl_sock, mcid);
		if (ret)
			return ret;
	}

	return 0;
}

static int do_listen_events(void)
{
	struct nl_cb *cb = nl_cb_alloc((log_level > LOG_WARNING) ?
				       NL_CB_DEBUG : NL_CB_DEFAULT);

	if (!cb) {
		LOG_ERR_("failed to allocate netlink callbacks\n");
		return -ENOMEM;
	}

	/* no sequence checking for multicast messages */
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, valid_handler, NULL);

	for (;;)
		nl_recvmsgs(state.nl_sock, cb);

	nl_cb_put(cb);

	return 0;
}

static int phy_lookup(char *name)
{
	char buf[200];
	int fd, pos;

	snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

	fd = open(buf, O_RDONLY);
	if (fd < 0)
		return -1;
	pos = read(fd, buf, sizeof(buf) - 1);
	if (pos < 0) {
		close(fd);
		return -1;
	}
	buf[pos] = '\0';
	close(fd);
	return atoi(buf);
}

static void add_nla_stream_to_msg(struct nl_msg *msg, void *nla, size_t nla_len)
{
	struct nlattr *tail;
	struct nlmsghdr *hdr;

	LOG_DBG_("%s: Appending %u bytes of user defined attributes\n",
		 __func__, nla_len);
	hdr = nlmsg_hdr(msg);
	tail = nlmsg_tail(hdr);
	/* nla is assumed to be padded correctly,
	 * so we dont bother with padding
	 */
	memcpy(tail, nla, nla_len);
	hdr->nlmsg_len += nla_len;
}

static int send_recv_nlcmd(void *nla, size_t nla_len)
{
	int err, nla_offset = 0;
	struct nl_cb *cb;
	struct nl_cb *s_cb;
	struct nl_msg *msg;

	if (cur_cmd <= NL80211_CMD_UNSPEC) {
		LOG_ERR_("Unsupported nl command: %d\n", cur_cmd);
		return 1;
	}

	if (devidx_set)
		/* Since devidx is a uint32_t the attribute will consume 8
		 * bytes. The nla input stream must be appended after this
		 * attribute.
		 */
		nla_offset = 8;

	LOG_DBG_("%s: Allocating %d bytes for nlmsg\n", __func__,
		  nla_len + nla_offset + NLMSG_HDRLEN + GENL_HDRLEN);
	msg = nlmsg_alloc_size(nla_len + nla_offset + NLMSG_HDRLEN + GENL_HDRLEN);
	if (!msg) {
		LOG_ERR_("failed to allocate netlink message\n");
		return 2;
	}

	cb = nl_cb_alloc((log_level > LOG_WARNING) ?
			 NL_CB_DEBUG : NL_CB_DEFAULT);
	s_cb = nl_cb_alloc((log_level > LOG_WARNING) ?
			   NL_CB_DEBUG : NL_CB_DEFAULT);
	if (!cb || !s_cb) {
		LOG_ERR_("failed to allocate netlink callbacks\n");
		err = 2;
		goto out;
	}

	genlmsg_put(msg, 0, 0, state.nl80211_id, 0, 0, cur_cmd, 0);

	if (devidx_set) {
		LOG_DBG_("%s: Adding devidx %d attribute\n", __func__, devidx);
		if (dev_by_phy)
			NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, devidx);
		else
			NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
	}

	add_nla_stream_to_msg(msg, nla, nla_len);

	nl_socket_set_cb(state.nl_sock, s_cb);

	err = nl_send_auto_complete(state.nl_sock, msg);
	if (err < 0) {
		LOG_ERR_("nl_send_auto_complete %d\n", err);
		goto out;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, valid_handler, NULL);

	while (err > 0)
		nl_recvmsgs(state.nl_sock, cb);
 out:
	nl_cb_put(cb);
	nl_cb_put(s_cb);
	nlmsg_free(msg);
	return err;
 nla_put_failure:
	LOG_ERR_("building message failed\n");
	return 2;
}

static int validate_nla_stream(uint8_t *buf, size_t buflen)
{
	struct nlattr *cur_attr;
	int remaining, attr_cnt = 0;

	cur_attr = (struct nlattr *) buf;
	remaining = buflen;

	while (nla_ok(cur_attr, remaining)) {
		attr_cnt++;
		cur_attr = nla_next(cur_attr, &remaining);
	}

	if (!attr_cnt) {
		LOG_ERR_("No valid attributes found in the input!\n");
		return -EINVAL;
	}

	if (remaining)
		LOG_WARN_("%d invalid bytes at the end detected of the"
			  " input. Skipping these..\n", remaining);

	LOG_NOTICE_("Found %d attributes in the input stream\n", attr_cnt);
	return 0;
}

static ssize_t read_nla_stream(uint8_t *buf, size_t buflen)
{
	ssize_t n = 0;

	for (;;) {
		ssize_t read_len;

		read_len = read(0, buf + n, buflen - n);
		if (read_len < 0)
			return -errno; /*Error*/
		if (read_len == 0)
			break; /*EOF*/
		n += read_len;
	}

	if (validate_nla_stream(buf, n))
		return -EINVAL;

	return n;
}

static int run_iwraw(void)
{
	int rc;

	rc = nl80211_init();
	if (rc)
		return rc;

	if (!cmd_set) {
		rc = prepare_listen_events();
		if (rc)
			return rc;
		rc = do_listen_events();
	} else {
		ssize_t nla_stream_len;

		nla_stream_len = read_nla_stream(nla_input_stream,
						 sizeof(nla_input_stream));
		if (nla_stream_len < 0)
			return -1;
		rc = send_recv_nlcmd(nla_input_stream, nla_stream_len);
	}

	return rc;
}

static void print_usage(char *argv0)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s OPTIONS\n", argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "iwraw will read a stream of netlink attributes from stdin, add them to a\n");
	fprintf(stderr, "netlink message and send the message to the wireless networking subsystem in the kernel.\n");
	fprintf(stderr, "It is essentially a stripped down version of iw where the netlink attributes\n");
	fprintf(stderr, "must be constructed outside of the program.\n");
	fprintf(stderr, "In case there is a response message from the kernel, the netlink attributes in the\n");
	fprintf(stderr, "response message will be printed to stdout\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "If no command argument is passed to the program, iwraw will listen for nl80211\n");
	fprintf(stderr, "events and print the raw event data to stdout.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "By default all input and output is in binary format. This behavior can be\n");
	fprintf(stderr, "overridden by command line options.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -c, --command      nl80211 command to send. Use --print-commands\n");
	fprintf(stderr, "                     to list all available commands.\n");
	fprintf(stderr, "  -a, --ascii        ASCII output. Print output in ASCII format\n");
	fprintf(stderr, "                     instead of binary.\n");
	fprintf(stderr, "  -v, --verbose      Enable debug prints (each -v option\n");
	fprintf(stderr, "                     increases the verbosity level)\n");
	fprintf(stderr, "  --if, --interface  Wireless Network interface. Use this\n");
	fprintf(stderr, "                     option or --phy\n");
	fprintf(stderr, "  --phy              Wireless Network phy. Use this option\n");
	fprintf(stderr, "                     or --if | --interface\n");
	fprintf(stderr, "  --print-commands   Print all available commands and exit\n");
	fprintf(stderr, "  --version          Print version info and exit.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--interface or --phy will read the device index of the interface\n");
	fprintf(stderr, "and add it to the nla stream in either of the attributes:\n");
	fprintf(stderr, "NL80211_ATTR_WIPHY or NL80211_ATTR_IFINDEX depending on which\n");
	fprintf(stderr, "argument was used.\n");
	fprintf(stderr, "If any of these arguments is omitted, the user is responsible\n");
	fprintf(stderr, "for adding the if index to the input nla stream (if needed).\n");
	fprintf(stderr, "\n");
}

static void print_version(void)
{
#if GIT_SHA_AVAILABLE
	fprintf(stderr, "\n%s-%s\n\n", VERSION, GIT_SHA);
#else
	fprintf(stderr, "\n%s-\n\n", VERSION);
#endif
}

int main(int argc, char **argv)
{
	int opt, optind = 0;
	struct option long_opts[] = {
		{"help", no_argument, 0, 'h'},
		{"command", required_argument, 0, 'c'},
		{"ascii", no_argument, 0, 'a'},
		{"verbose", no_argument, 0, 'v'},
		{"if", required_argument, 0, 1000},
		{"interface", required_argument, 0, 1001},
		{"phy", required_argument, 0, 1002},
		{"print-commands", no_argument, 0, 1003},
		{"version", no_argument, 0, 1004},
		{NULL, 0, 0, 0},
	};

	while ((opt = getopt_long(argc, argv, "hc:av", long_opts, &optind)) != -1) {
		switch (opt) {
		case 1000:
		/* Fallthrough */
		case 1001:
			dev_by_phy = false;
			devidx = if_nametoindex(optarg);
			devidx_set = true;
			break;
		case 1002:
			dev_by_phy = true;
			devidx = phy_lookup(optarg);
			devidx_set = true;
			break;
		case 1003:
			print_nl80211_cmds();
			return 0;
		case 1004:
			print_version();
			return 0;
		case 'a':
			print_ascii = true;
			break;
		case 'c':
			cur_cmd = nl80211_cmd_from_str(optarg);
			if (cur_cmd == NL80211_CMD_UNSPEC)
				fprintf(stderr, "Unknown command: %s\n", optarg);
			else
				cmd_set = true;
			break;
		case 'v':
			log_level++;
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	return run_iwraw();
}

