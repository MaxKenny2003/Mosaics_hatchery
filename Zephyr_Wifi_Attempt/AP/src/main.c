#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(hab_sta, LOG_LEVEL_INF);

#define AP_SSID    "HAB-NET"
#define AP_PSK     "balloon1234"
#define MY_IP      "192.168.4.2"
#define MY_MASK    "255.255.255.0"
#define SERVER_IP  "192.168.4.1"
#define TCP_PORT   5000

static K_SEM_DEFINE(wifi_up, 0, 1);
static struct net_mgmt_event_callback wifi_cb;

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint64_t event, struct net_if *iface)
{
	if (event == NET_EVENT_WIFI_CONNECT_RESULT) {
		const struct wifi_status *st = cb->info;
		if (st->status == 0) {
			k_sem_give(&wifi_up);
		}
	}
}

static bool connect_wifi(struct net_if *iface)
{
	struct wifi_connect_req_params params = {
		.ssid        = (const uint8_t *)AP_SSID,
		.ssid_length = strlen(AP_SSID),
		.psk         = (const uint8_t *)AP_PSK,
		.psk_length  = strlen(AP_PSK),
		.channel     = WIFI_CHANNEL_ANY,
		.band        = WIFI_FREQ_BAND_2_4_GHZ,
		.security    = WIFI_SECURITY_TYPE_PSK,
	};

	k_sem_reset(&wifi_up);
	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params))) {
		return false;
	}
	return k_sem_take(&wifi_up, K_SECONDS(15)) == 0;
}

static bool send_telemetry(const char *msg)
{
	int s = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		return false;
	}

	struct sockaddr_in srv = {
		.sin_family = AF_INET,
		.sin_port   = htons(TCP_PORT),
	};
	zsock_inet_pton(AF_INET, SERVER_IP, &srv.sin_addr);

	bool ok = false;
	if (zsock_connect(s, (struct sockaddr *)&srv, sizeof(srv)) == 0) {
		ok = zsock_send(s, msg, strlen(msg), 0) > 0;
	}
	zsock_close(s);
	return ok;
}

int main(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	struct in_addr addr, mask;

	/* Static IP so we don't depend on DHCP */
	net_addr_pton(AF_INET, MY_IP, &addr);
	net_addr_pton(AF_INET, MY_MASK, &mask);
	net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
	net_if_ipv4_set_netmask_by_addr(iface, &addr, &mask);

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	k_sleep(K_SECONDS(2));

	uint32_t seq = 0;
	while (1) {
		/* Resilient: (re)connect whenever needed, never give up */
		if (!connect_wifi(iface)) {
			LOG_WRN("Wi-Fi connect failed, retrying");
			k_sleep(K_SECONDS(2));
			continue;
		}

		char msg[96];
		snprintf(msg, sizeof(msg), "seq=%u,uptime=%lld",
			 seq++, k_uptime_get() / 1000);

		if (!send_telemetry(msg)) {
			LOG_WRN("send failed");
		}
		k_sleep(K_SECONDS(5));
	}
	return 0;
}