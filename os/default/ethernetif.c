#include "ethernetif.h"
#include <hal/hal_eth.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"
#include <hal/board.h>
#include <string.h>

#define IFNAME0 's'
#define IFNAME1 't'

static uint8_t eth_mac_addr[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    uint8_t buf[1524];
    uint32_t framelength = 0;
    int rc;

    (void)netif;

    for (q = p; q != NULL; q = q->next) {
        if (framelength + q->len > sizeof(buf))
            return ERR_MEM;
        memcpy(&buf[framelength], q->payload, q->len);
        framelength += q->len;
    }

    rc = hal_eth_tx(buf, framelength);
    if (rc != 0)
        return ERR_IF;

#if ETHARP_TRUST_IP_MAC
    etharp_request(netif, (struct ip4_addr *)netif_ip4_addr(netif));
#endif

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

static struct pbuf *low_level_input(void)
{
    extern void hal_uart_puts(const char *s);
    uint8_t buf[1524];
    size_t len = 0;
    int rc;
    struct pbuf *p;

    /* hal_eth_rx() returns 0 for "no packet pending" but a negative
     * value for "this one descriptor's frame had an error" (already
     * re-owned/skipped internally by hal_eth_rx() itself). Those are
     * not the same thing: only 0 means the ring is genuinely drained.
     * Treating them the same here used to make ethernetif_input()'s
     * drain loop (below) stop at the very first bad frame in a call,
     * even when a good frame -- potentially the DHCP OFFER everything
     * is waiting on -- was sitting right behind it in the ring. With
     * HAL_ETH_RX_DESC_COUNT only 8 descriptors and any real-world LAN
     * carrying a steady trickle of broadcast/multicast noise (ARP,
     * mDNS, IPv6 neighbor discovery, ...), that could stall DHCP
     * indefinitely if an OFFER never happened to land first in a
     * burst. Skip over errored frames here instead of bailing out. */
    for (int skip_budget = 32; skip_budget > 0; skip_budget--) {
        rc = hal_eth_rx(buf, sizeof(buf), &len);
        if (rc == 0)
            return NULL; /* ring genuinely empty */
        if (rc > 0)
            break; /* got a real frame */
        /* rc < 0: this descriptor's frame errored and was already
         * skipped by hal_eth_rx() -- try the next one, up to the
         * budget above (bounds worst-case time here if incoming
         * frames keep erroring faster than we can drain them). */
    }
    if (rc <= 0)
        return NULL; /* skip budget exhausted without a good frame */

    /* Packet sniffer for diagnosis -- gated on saramos_eth_verbose
     * (defined in hal_eth.c, toggled with "eth on"/"eth off"), same as
     * the RX/TX packet prints in that file. */
    extern volatile int saramos_eth_verbose;
    if (saramos_eth_verbose && len >= 14) {
        uint16_t etype = ((uint16_t)buf[12] << 8) | buf[13];
        if (etype == 0x0806) {
            hal_uart_puts("[ETH] RX ARP\r\n");
        } else if (etype == 0x0800 && len >= 42) {
            uint8_t proto = buf[23];
            if (proto == 17) { /* UDP */
                uint16_t sport = ((uint16_t)buf[34] << 8) | buf[35];
                uint16_t dport = ((uint16_t)buf[36] << 8) | buf[37];
                if (sport == 67 && dport == 68) {
                    char dbg[64];
                    uint8_t msg_type = 0;
                    /* Search for DHCP option 53 (Message Type) in DHCP payload (offset 278) */
                    if (len >= 282 && buf[278] == 0x63 && buf[279] == 0x82 && buf[280] == 0x53 && buf[281] == 0x63) {
                        for (size_t opt = 282; opt + 2 < len && buf[opt] != 255; ) {
                            if (buf[opt] == 0) { opt++; continue; }
                            if (buf[opt] == 53 && buf[opt + 1] == 1) {
                                msg_type = buf[opt + 2];
                                break;
                            }
                            opt += 2 + buf[opt + 1];
                        }
                    }
                    const char *tname = (msg_type == 2) ? "OFFER" : (msg_type == 5) ? "ACK" : (msg_type == 6) ? "NAK" : "OTHER";
                    __builtin_sprintf(dbg, "[ETH] RX DHCP %s (type=%d, len=%u)\r\n", tname, (int)msg_type, (unsigned)len);
                    hal_uart_puts(dbg);
                }
            }
        }
    }

    p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
    if (p != NULL) {
        pbuf_take(p, buf, (u16_t)len);
        LINK_STATS_INC(link.recv);
    } else {
        LINK_STATS_INC(link.memerr);
        LINK_STATS_INC(link.drop);
    }

    return p;
}

void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;

    do {
        p = low_level_input();
        if (p != NULL) {
            if (netif->input(p, netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    } while (p != NULL);
}

err_t low_level_init(struct netif *netif)
{
    extern void hal_uart_puts(const char *s);

    int rc = hal_eth_init(NULL);
    if (rc != 0) {
        char dbg[64];
        __builtin_sprintf(dbg, "[ETH] hal_eth_init failed: %d\r\n", rc);
        hal_uart_puts(dbg);
    } else {
        hal_uart_puts("[ETH] hal_eth_init OK\r\n");
    }

    /* Retrieve unique UID-based MAC from HAL */
    hal_eth_get_mac_addr(eth_mac_addr);

    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(netif->hwaddr, eth_mac_addr, ETHARP_HWADDR_LEN);

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    return ERR_OK;
}

err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    netif->hostname = "saramos";
#endif

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;

    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    return low_level_init(netif);
}
