#include "net.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"

static netif_t net_ifaces[NET_MAX_IFACES];
static int net_iface_count = 0;

/* ----- small helpers ----- */

static uint16_t net_htons(uint16_t x) {
    return (uint16_t)(((x & 0xFF) << 8) | ((x >> 8) & 0xFF));
}
static uint32_t net_htonl(uint32_t x) {
    return ((x & 0xFF) << 24) | (((x >> 8) & 0xFF) << 16) |
           (((x >> 16) & 0xFF) << 8) | ((x >> 24) & 0xFF);
}

/* Internet checksum (16-bit one's complement) */
static uint16_t net_checksum(const uint8_t* data, uint32_t len) {
    uint32_t sum = 0;
    uint32_t i = 0;
    while (len > 1) {
        sum += ((uint32_t)data[i] << 8) | data[i + 1];
        i += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += ((uint32_t)data[i]) << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/* ----- per-interface receive queue ----- */

typedef struct {
    uint8_t buf[NET_MAX_IFACES][NET_MAX_PACKET];
    uint32_t len[NET_MAX_IFACES];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} net_rx_queue_t;

static net_rx_queue_t net_rx;

static void net_rx_push(int iface_idx, const uint8_t* data, uint32_t len) {
    (void)iface_idx;
    if (len > NET_MAX_PACKET) len = NET_MAX_PACKET;
    if (net_rx.count >= NET_RX_QUEUE_SIZE) return; /* drop */
    for (uint32_t i = 0; i < len; i++) {
        net_rx.buf[net_rx.tail][i] = data[i];
    }
    net_rx.len[net_rx.tail] = len;
    net_rx.tail = (net_rx.tail + 1) % NET_RX_QUEUE_SIZE;
    net_rx.count++;
}

static int net_rx_pop(int iface_idx, uint8_t* buf, uint32_t max_len) {
    (void)iface_idx;
    if (net_rx.count == 0) return 0;
    uint32_t len = net_rx.len[net_rx.head];
    if (len > max_len) len = max_len;
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = net_rx.buf[net_rx.head][i];
    }
    net_rx.head = (net_rx.head + 1) % NET_RX_QUEUE_SIZE;
    net_rx.count--;
    return (int)len;
}

/* ----- IPv4 + ICMP headers (used by loopback + ping) ----- */

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed)) ipv4_hdr_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_hdr_t;

/* ----- loopback interface ----- */

static int loopback_send(netif_t* iface, uint16_t ethertype,
                         const uint8_t* data, uint32_t len) {
    (void)iface;
    uint8_t pkt[NET_MAX_PACKET];
    uint32_t plen = len;
    if (plen > NET_MAX_PACKET) plen = NET_MAX_PACKET;
    for (uint32_t i = 0; i < plen; i++) pkt[i] = data[i];

    /* If this is an IPv4 ICMP echo request, answer with an echo reply. */
    if (ethertype == NET_ETH_IPV4 && plen >= 20 + 8) {
        ipv4_hdr_t* iph = (ipv4_hdr_t*)pkt;
        if (iph->proto == NET_IP_PROTO_ICMP) {
            icmp_hdr_t* icmp = (icmp_hdr_t*)(pkt + 20);
            if (icmp->type == NET_ICMP_ECHO_REQUEST) {
                /* swap source/destination */
                uint32_t tmp = iph->src;
                iph->src = iph->dst;
                iph->dst = tmp;
                iph->checksum = 0;
                iph->checksum = net_htons(net_checksum(pkt, 20));
                /* convert request to reply and fix ICMP checksum */
                icmp->type = NET_ICMP_ECHO_REPLY;
                icmp->checksum = 0;
                icmp->checksum = net_htons(net_checksum(pkt + 20, plen - 20));
            }
        }
    }
    net_rx_push(0, pkt, plen);
    return (int)len;
}

static int loopback_recv(netif_t* iface, uint8_t* buf, uint32_t max_len) {
    (void)iface;
    return net_rx_pop(0, buf, max_len);
}

/* ----- virtual ethernet (vnet) interface ----- */

static uint8_t vnet_tx[NET_MAX_PACKET];
static uint32_t vnet_tx_len = 0;

static int vnet_send(netif_t* iface, uint16_t ethertype,
                     const uint8_t* data, uint32_t len) {
    (void)iface;
    (void)ethertype;
    if (len > NET_MAX_PACKET) len = NET_MAX_PACKET;
    for (uint32_t i = 0; i < len; i++) {
        vnet_tx[i] = data[i];
    }
    vnet_tx_len = len;
    return (int)len;
}

static int vnet_recv(netif_t* iface, uint8_t* buf, uint32_t max_len) {
    (void)iface;
    (void)buf;
    (void)max_len;
    /* vnet is a simulated uplink: nothing arrives unless injected */
    return 0;
}

/* ----- IPv4 + ICMP ----- */

static int net_route(uint32_t dst_ip, netif_t** out_iface) {
    for (int i = 0; i < net_iface_count; i++) {
        if (!net_ifaces[i].up) continue;
        if (net_ifaces[i].ip == dst_ip) {
            *out_iface = &net_ifaces[i];
            return 1;
        }
    }
    /* default route to first up interface (vnet) for non-local */
    for (int i = 0; i < net_iface_count; i++) {
        if (net_ifaces[i].up) {
            *out_iface = &net_ifaces[i];
            return 1;
        }
    }
    return 0;
}

static int net_send_ipv4(netif_t* iface, uint8_t proto, uint32_t src, uint32_t dst,
                         const uint8_t* payload, uint32_t payload_len) {
    uint8_t pkt[NET_MAX_PACKET];
    ipv4_hdr_t* hdr = (ipv4_hdr_t*)pkt;
    hdr->ver_ihl = 0x45;
    hdr->tos = 0;
    hdr->total_len = net_htons((uint16_t)(20 + payload_len));
    hdr->id = net_htons(0x0001);
    hdr->flags_frag = 0;
    hdr->ttl = 64;
    hdr->proto = proto;
    hdr->checksum = 0;
    hdr->src = net_htonl(src);
    hdr->dst = net_htonl(dst);
    hdr->checksum = net_htons(net_checksum(pkt, 20));
    for (uint32_t i = 0; i < payload_len; i++) {
        pkt[20 + i] = payload[i];
    }
    return net_send(iface, NET_ETH_IPV4, pkt, 20 + payload_len);
}

int net_ping(uint32_t dst_ip, uint32_t timeout_ms) {
    netif_t* iface = 0;
    if (!net_route(dst_ip, &iface)) {
        return -1;
    }
    if (!iface->up) {
        return -2;
    }

    uint8_t payload[64];
    for (int i = 0; i < 64; i++) payload[i] = (uint8_t)('A' + (i % 26));

    uint8_t pkt[NET_MAX_PACKET];
    icmp_hdr_t* icmp = (icmp_hdr_t*)pkt;
    icmp->type = NET_ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = net_htons(0x1234);
    icmp->seq = net_htons(1);
    for (int i = 0; i < 64; i++) pkt[8 + i] = payload[i];
    icmp->checksum = net_htons(net_checksum(pkt, 8 + 64));

    uint32_t start_ms = 0;
    (void)start_ms;
    /* send the request */
    if (net_send_ipv4(iface, NET_IP_PROTO_ICMP, iface->ip, dst_ip, pkt, 8 + 64) < 0) {
        return -3;
    }

    /* wait for an echo reply on any interface, up to timeout_ms */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        timer_sleep_ms(20);
        elapsed += 20;
        for (int i = 0; i < net_iface_count; i++) {
            if (!net_ifaces[i].up) continue;
            uint8_t rxbuf[NET_MAX_PACKET];
            int len = net_recv(&net_ifaces[i], rxbuf, NET_MAX_PACKET);
            if (len < (int)(20 + 8 + 64)) continue;
            ipv4_hdr_t* iph = (ipv4_hdr_t*)rxbuf;
            if (iph->proto != NET_IP_PROTO_ICMP) continue;
            icmp_hdr_t* ich = (icmp_hdr_t*)(rxbuf + 20);
            if (ich->type != NET_ICMP_ECHO_REPLY) continue;
            if (net_htonl(iph->src) == dst_ip) {
                return 1; /* reply received */
            }
        }
    }
    return 0; /* timeout */
}

/* ----- public API ----- */

void net_init(void) {
    net_iface_count = 0;
    net_rx.head = 0;
    net_rx.tail = 0;
    net_rx.count = 0;
    vnet_tx_len = 0;

    /* Loopback interface: 127.0.0.1 */
    netif_t* lo = &net_ifaces[net_iface_count++];
    for (int i = 0; i < 16; i++) lo->name[i] = 0;
    lo->name[0] = 'l'; lo->name[1] = 'o'; lo->name[2] = 0;
    lo->mac[0] = 0x0A; lo->mac[1] = 0x00; lo->mac[2] = 0x00;
    lo->mac[3] = 0x00; lo->mac[4] = 0x00; lo->mac[5] = 0x01;
    lo->ip = IP_LOOPBACK;
    lo->netmask = 0xFF000000UL;
    lo->gateway = 0;
    lo->up = 1;
    lo->send = loopback_send;
    lo->recv = loopback_recv;
    lo->priv = 0;

    /* Virtual ethernet uplink: 10.0.0.2 */
    netif_t* vn = &net_ifaces[net_iface_count++];
    vn->name[0] = 'v'; vn->name[1] = 'n'; vn->name[2] = 'e'; vn->name[3] = 't'; vn->name[4] = 0;
    vn->mac[0] = 0x52; vn->mac[1] = 0x54; vn->mac[2] = 0x00;
    vn->mac[3] = 0x12; vn->mac[4] = 0x34; vn->mac[5] = 0x56;
    vn->ip = IP_VNET;
    vn->netmask = 0xFFFFFF00UL;
    vn->gateway = 0x0A000001UL;
    vn->up = 1;
    vn->send = vnet_send;
    vn->recv = vnet_recv;
    vn->priv = 0;
}

netif_t* net_iface_by_name(const char* name) {
    for (int i = 0; i < net_iface_count; i++) {
        int match = 1;
        for (int j = 0; name[j]; j++) {
            if (net_ifaces[i].name[j] != name[j]) { match = 0; break; }
        }
        if (match && net_ifaces[i].name[0]) return &net_ifaces[i];
    }
    return 0;
}

netif_t* net_iface_by_ip(uint32_t ip) {
    for (int i = 0; i < net_iface_count; i++) {
        if (net_ifaces[i].ip == ip) return &net_ifaces[i];
    }
    return 0;
}

int net_send(netif_t* iface, uint16_t ethertype, const uint8_t* data, uint32_t len) {
    if (!iface || !iface->send) return -1;
    return iface->send(iface, ethertype, data, len);
}

int net_recv(netif_t* iface, uint8_t* buf, uint32_t max_len) {
    if (!iface || !iface->recv) return -1;
    return iface->recv(iface, buf, max_len);
}

void net_dump(void) {
    vga_puts("\n[ ZeroOS Network Interfaces ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  NAME    MAC ADDRESS       IP ADDRESS      MASK            GATEWAY     STATE\n", COLOR_DARK_GREY);
    for (int i = 0; i < net_iface_count; i++) {
        netif_t* n = &net_ifaces[i];
        vga_puts("  ", COLOR_WHITE);
        vga_puts(n->name, COLOR_LIGHT_GREEN);
        vga_puts("     ", COLOR_WHITE);
        for (int j = 0; j < 6; j++) {
            const char hex[] = "0123456789ABCDEF";
            vga_putc(hex[(n->mac[j] >> 4) & 0xF], COLOR_WHITE);
            vga_putc(hex[n->mac[j] & 0xF], COLOR_WHITE);
            if (j < 5) vga_putc(':', COLOR_WHITE);
        }
        vga_puts("      ", COLOR_WHITE);
        /* print ip */
        uint32_t ip = net_htonl(n->ip);
        for (int j = 0; j < 4; j++) {
            uint8_t b = (uint8_t)((ip >> (24 - j * 8)) & 0xFF);
            /* decimal */
            vga_putc('0' + b / 100, COLOR_WHITE);
            vga_putc('0' + (b / 10) % 10, COLOR_WHITE);
            vga_putc('0' + b % 10, COLOR_WHITE);
            if (j < 3) vga_putc('.', COLOR_WHITE);
        }
        vga_puts("    ", COLOR_WHITE);
        uint32_t mask = net_htonl(n->netmask);
        for (int j = 0; j < 4; j++) {
            uint8_t b = (uint8_t)((mask >> (24 - j * 8)) & 0xFF);
            vga_putc('0' + b / 100, COLOR_WHITE);
            vga_putc('0' + (b / 10) % 10, COLOR_WHITE);
            vga_putc('0' + b % 10, COLOR_WHITE);
            if (j < 3) vga_putc('.', COLOR_WHITE);
        }
        vga_puts("   ", COLOR_WHITE);
        uint32_t gw = net_htonl(n->gateway);
        for (int j = 0; j < 4; j++) {
            uint8_t b = (uint8_t)((gw >> (24 - j * 8)) & 0xFF);
            vga_putc('0' + b / 100, COLOR_WHITE);
            vga_putc('0' + (b / 10) % 10, COLOR_WHITE);
            vga_putc('0' + b % 10, COLOR_WHITE);
            if (j < 3) vga_putc('.', COLOR_WHITE);
        }
        vga_puts("    ", COLOR_WHITE);
        vga_puts(n->up ? "UP" : "DOWN", n->up ? COLOR_LIGHT_GREEN : COLOR_LIGHT_RED);
        vga_putc('\n', COLOR_WHITE);
    }
    vga_putc('\n', COLOR_WHITE);
}
