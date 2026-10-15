#ifndef NET_H
#define NET_H

#include <stdint.h>

#define NET_MAX_IFACES    4
#define NET_MAX_PACKET    1514
#define NET_RX_QUEUE_SIZE 8

#define NET_ETH_IPV4 0x0800
#define NET_IP_PROTO_ICMP 1
#define NET_ICMP_ECHO_REPLY  0
#define NET_ICMP_ECHO_REQUEST 8

#define IP_LOOPBACK 0x7F000001UL /* 127.0.0.1 */
#define IP_VNET     0x0A000002UL /* 10.0.0.2  */

typedef struct netif netif_t;

typedef int (*netif_send_fn)(netif_t* iface, uint16_t ethertype,
                             const uint8_t* data, uint32_t len);
typedef int (*netif_recv_fn)(netif_t* iface, uint8_t* buf, uint32_t max_len);

struct netif {
    char name[16];
    uint8_t mac[6];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    int up;
    netif_send_fn send;
    netif_recv_fn recv;
    void* priv;
};

void net_init(void);
netif_t* net_iface_by_name(const char* name);
netif_t* net_iface_by_ip(uint32_t ip);
int net_send(netif_t* iface, uint16_t ethertype, const uint8_t* data, uint32_t len);
int net_recv(netif_t* iface, uint8_t* buf, uint32_t max_len);
int net_ping(uint32_t dst_ip, uint32_t timeout_ms);
void net_dump(void);

#endif
