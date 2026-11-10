#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/socket.h>
#include <string.h>

LOG_MODULE_REGISTER(hab_ap, LOG_LEVEL_INF);

#define AP_SSID   "HAB-NET"
#define AP_PSK    "balloon1234"      /* WPA2 needs >= 8 chars */
#define AP_IP     "192.168.4.1"
#define AP_MASK   "255.255.255.0"
#define TCP_PORT  5000

static void start_ap(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	struct in_addr addr, mask;

	/* Static IP on the AP interface (no DHCP server needed) */
	net_addr_pton(AF_INET, AP_IP, &addr);
	net_addr_pton(AF_INET, AP_MASK, &mask);
	net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
	net_if_ipv4_set_netmask_by_addr(iface, &addr, &mask);

	struct wifi_connect_req_params params = {
		.ssid        = (const uint8_t *)AP_SSID,
		.ssid_length = strlen(AP_SSID),
		.psk         = (const uint8_t *)AP_PSK,
		.psk_length  = strlen(AP_PSK),
		.channel     = 6,
		.band        = WIFI_FREQ_BAND_2_4_GHZ,
		.security    = WIFI_SECURITY_TYPE_PSK,
	};

	int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface,
			   &params, sizeof(params));
	if (ret) {
		LOG_ERR("AP enable failed: %d", ret);
	} else {
		LOG_INF("AP up: SSID=%s IP=%s", AP_SSID, AP_IP);
	}
}

int main(void)
{
	k_sleep(K_SECONDS(1));   /* let the Wi-Fi driver come up */
	start_ap();

	int srv = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (srv < 0) {
		LOG_ERR("socket failed: %d", errno);
		return 0;
	}

	struct sockaddr_in bind_addr = {
		.sin_family = AF_INET,
		.sin_port   = htons(TCP_PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};

	if (zsock_bind(srv, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0 ||
	    zsock_listen(srv, 2) < 0) {
		LOG_ERR("bind/listen failed: %d", errno);
		return 0;
	}
	LOG_INF("Listening on port %d", TCP_PORT);

	while (1) {
		struct sockaddr_in client;
		socklen_t len = sizeof(client);
		int c = zsock_accept(srv, (struct sockaddr *)&client, &len);
		if (c < 0) {
			k_sleep(K_MSEC(500));
			continue;
		}

		char buf[128];
		int n = zsock_recv(c, buf, sizeof(buf) - 1, 0);
		if (n > 0) {
			buf[n] = '\0';
			LOG_INF("Telemetry: %s", buf);
			/* TODO: write to flash / SD / UART downlink */
		}
		zsock_close(c);
	}
	return 0;
}