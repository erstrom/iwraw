#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "nl80211.h"

#define STR_MATCH(str, fixed_str) \
(strncmp(str, fixed_str, strlen(fixed_str)) == 0)

static const char *commands[NL80211_CMD_MAX + 1] = {
/*
 * sed 's%^\tNL80211_CMD_%%;t n;d;:n s%^\([^=]*\),.*%\t[NL80211_CMD_\1] = \"\L\1\",%;t;d' nl80211.h | grep -v "reserved"
 */
	[NL80211_CMD_UNSPEC] = "unspec",
	[NL80211_CMD_GET_WIPHY] = "get_wiphy",
	[NL80211_CMD_SET_WIPHY] = "set_wiphy",
	[NL80211_CMD_NEW_WIPHY] = "new_wiphy",
	[NL80211_CMD_DEL_WIPHY] = "del_wiphy",
	[NL80211_CMD_GET_INTERFACE] = "get_interface",
	[NL80211_CMD_SET_INTERFACE] = "set_interface",
	[NL80211_CMD_NEW_INTERFACE] = "new_interface",
	[NL80211_CMD_DEL_INTERFACE] = "del_interface",
	[NL80211_CMD_GET_KEY] = "get_key",
	[NL80211_CMD_SET_KEY] = "set_key",
	[NL80211_CMD_NEW_KEY] = "new_key",
	[NL80211_CMD_DEL_KEY] = "del_key",
	[NL80211_CMD_GET_BEACON] = "get_beacon",
	[NL80211_CMD_SET_BEACON] = "set_beacon",
	[NL80211_CMD_START_AP] = "start_ap",
	[NL80211_CMD_STOP_AP] = "stop_ap",
	[NL80211_CMD_GET_STATION] = "get_station",
	[NL80211_CMD_SET_STATION] = "set_station",
	[NL80211_CMD_NEW_STATION] = "new_station",
	[NL80211_CMD_DEL_STATION] = "del_station",
	[NL80211_CMD_GET_MPATH] = "get_mpath",
	[NL80211_CMD_SET_MPATH] = "set_mpath",
	[NL80211_CMD_NEW_MPATH] = "new_mpath",
	[NL80211_CMD_DEL_MPATH] = "del_mpath",
	[NL80211_CMD_SET_BSS] = "set_bss",
	[NL80211_CMD_SET_REG] = "set_reg",
	[NL80211_CMD_REQ_SET_REG] = "req_set_reg",
	[NL80211_CMD_GET_MESH_CONFIG] = "get_mesh_config",
	[NL80211_CMD_SET_MESH_CONFIG] = "set_mesh_config",
	[NL80211_CMD_GET_REG] = "get_reg",
	[NL80211_CMD_GET_SCAN] = "get_scan",
	[NL80211_CMD_TRIGGER_SCAN] = "trigger_scan",
	[NL80211_CMD_NEW_SCAN_RESULTS] = "new_scan_results",
	[NL80211_CMD_SCAN_ABORTED] = "scan_aborted",
	[NL80211_CMD_REG_CHANGE] = "reg_change",
	[NL80211_CMD_AUTHENTICATE] = "authenticate",
	[NL80211_CMD_ASSOCIATE] = "associate",
	[NL80211_CMD_DEAUTHENTICATE] = "deauthenticate",
	[NL80211_CMD_DISASSOCIATE] = "disassociate",
	[NL80211_CMD_MICHAEL_MIC_FAILURE] = "michael_mic_failure",
	[NL80211_CMD_REG_BEACON_HINT] = "reg_beacon_hint",
	[NL80211_CMD_JOIN_IBSS] = "join_ibss",
	[NL80211_CMD_LEAVE_IBSS] = "leave_ibss",
	[NL80211_CMD_TESTMODE] = "testmode",
	[NL80211_CMD_CONNECT] = "connect",
	[NL80211_CMD_ROAM] = "roam",
	[NL80211_CMD_DISCONNECT] = "disconnect",
	[NL80211_CMD_SET_WIPHY_NETNS] = "set_wiphy_netns",
	[NL80211_CMD_GET_SURVEY] = "get_survey",
	[NL80211_CMD_NEW_SURVEY_RESULTS] = "new_survey_results",
	[NL80211_CMD_SET_PMKSA] = "set_pmksa",
	[NL80211_CMD_DEL_PMKSA] = "del_pmksa",
	[NL80211_CMD_FLUSH_PMKSA] = "flush_pmksa",
	[NL80211_CMD_REMAIN_ON_CHANNEL] = "remain_on_channel",
	[NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL] = "cancel_remain_on_channel",
	[NL80211_CMD_SET_TX_BITRATE_MASK] = "set_tx_bitrate_mask",
	[NL80211_CMD_REGISTER_FRAME] = "register_frame",
	[NL80211_CMD_FRAME] = "frame",
	[NL80211_CMD_FRAME_TX_STATUS] = "frame_tx_status",
	[NL80211_CMD_SET_POWER_SAVE] = "set_power_save",
	[NL80211_CMD_GET_POWER_SAVE] = "get_power_save",
	[NL80211_CMD_SET_CQM] = "set_cqm",
	[NL80211_CMD_NOTIFY_CQM] = "notify_cqm",
	[NL80211_CMD_SET_CHANNEL] = "set_channel",
	[NL80211_CMD_SET_WDS_PEER] = "set_wds_peer",
	[NL80211_CMD_FRAME_WAIT_CANCEL] = "frame_wait_cancel",
	[NL80211_CMD_JOIN_MESH] = "join_mesh",
	[NL80211_CMD_LEAVE_MESH] = "leave_mesh",
	[NL80211_CMD_UNPROT_DEAUTHENTICATE] = "unprot_deauthenticate",
	[NL80211_CMD_UNPROT_DISASSOCIATE] = "unprot_disassociate",
	[NL80211_CMD_NEW_PEER_CANDIDATE] = "new_peer_candidate",
	[NL80211_CMD_GET_WOWLAN] = "get_wowlan",
	[NL80211_CMD_SET_WOWLAN] = "set_wowlan",
	[NL80211_CMD_START_SCHED_SCAN] = "start_sched_scan",
	[NL80211_CMD_STOP_SCHED_SCAN] = "stop_sched_scan",
	[NL80211_CMD_SCHED_SCAN_RESULTS] = "sched_scan_results",
	[NL80211_CMD_SCHED_SCAN_STOPPED] = "sched_scan_stopped",
	[NL80211_CMD_SET_REKEY_OFFLOAD] = "set_rekey_offload",
	[NL80211_CMD_PMKSA_CANDIDATE] = "pmksa_candidate",
	[NL80211_CMD_TDLS_OPER] = "tdls_oper",
	[NL80211_CMD_TDLS_MGMT] = "tdls_mgmt",
	[NL80211_CMD_UNEXPECTED_FRAME] = "unexpected_frame",
	[NL80211_CMD_PROBE_CLIENT] = "probe_client",
	[NL80211_CMD_REGISTER_BEACONS] = "register_beacons",
	[NL80211_CMD_UNEXPECTED_4ADDR_FRAME] = "unexpected_4addr_frame",
	[NL80211_CMD_SET_NOACK_MAP] = "set_noack_map",
	[NL80211_CMD_CH_SWITCH_NOTIFY] = "ch_switch_notify",
	[NL80211_CMD_START_P2P_DEVICE] = "start_p2p_device",
	[NL80211_CMD_STOP_P2P_DEVICE] = "stop_p2p_device",
	[NL80211_CMD_CONN_FAILED] = "conn_failed",
	[NL80211_CMD_SET_MCAST_RATE] = "set_mcast_rate",
	[NL80211_CMD_SET_MAC_ACL] = "set_mac_acl",
	[NL80211_CMD_RADAR_DETECT] = "radar_detect",
	[NL80211_CMD_GET_PROTOCOL_FEATURES] = "get_protocol_features",
	[NL80211_CMD_UPDATE_FT_IES] = "update_ft_ies",
	[NL80211_CMD_FT_EVENT] = "ft_event",
	[NL80211_CMD_CRIT_PROTOCOL_START] = "crit_protocol_start",
	[NL80211_CMD_CRIT_PROTOCOL_STOP] = "crit_protocol_stop",
	[NL80211_CMD_GET_COALESCE] = "get_coalesce",
	[NL80211_CMD_SET_COALESCE] = "set_coalesce",
	[NL80211_CMD_CHANNEL_SWITCH] = "channel_switch",
	[NL80211_CMD_VENDOR] = "vendor",
	[NL80211_CMD_SET_QOS_MAP] = "set_qos_map",
	[NL80211_CMD_ADD_TX_TS] = "add_tx_ts",
	[NL80211_CMD_DEL_TX_TS] = "del_tx_ts",
	[NL80211_CMD_GET_MPP] = "get_mpp",
	[NL80211_CMD_JOIN_OCB] = "join_ocb",
	[NL80211_CMD_LEAVE_OCB] = "leave_ocb",
	[NL80211_CMD_CH_SWITCH_STARTED_NOTIFY] = "ch_switch_started_notify",
	[NL80211_CMD_TDLS_CHANNEL_SWITCH] = "tdls_channel_switch",
	[NL80211_CMD_TDLS_CANCEL_CHANNEL_SWITCH] = "tdls_cancel_channel_switch",
	[NL80211_CMD_WIPHY_REG_CHANGE] = "wiphy_reg_change",
	[NL80211_CMD_ABORT_SCAN] = "abort_scan",
};

enum nl80211_commands nl80211_cmd_from_str(const char *str)
{
	int i;

	for (i = 0; i < NL80211_CMD_MAX + 1; i++) {
		if (commands[i] && STR_MATCH(str, commands[i]))
			break;
	}

	if (i == NL80211_CMD_MAX + 1)
		i = NL80211_CMD_UNSPEC;

	return i;
}

void print_nl80211_cmds(void)
{
	int i;

	fprintf(stdout, "Available NL80211 commands:\n");
	for (i = 0; i < NL80211_CMD_MAX + 1; i++) {
		if (commands[i])
			fprintf(stdout, "  %s\n", commands[i]);
	}
}

static char cmdbuf[100];

const char *command_name(enum nl80211_commands cmd)
{
	if (cmd <= NL80211_CMD_MAX && commands[cmd])
		return commands[cmd];
	sprintf(cmdbuf, "Unknown command (%d)", cmd);
	return cmdbuf;
}


