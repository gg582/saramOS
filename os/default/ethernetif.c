#include "ethernetif.h"
#include <hal/hal_eth.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"
#include <hal/stm32f769i-disc1.h>
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
    uint8_t buf[1524];
    size_t len = 0;
    int rc;
    struct pbuf *p;

    rc = hal_eth_rx(buf, sizeof(buf), &len);
    if (rc <= 0)
        return NULL;

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
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(netif->hwaddr, eth_mac_addr, ETHARP_HWADDR_LEN);

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    hal_eth_init(eth_mac_addr);

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
