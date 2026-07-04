#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* Bare-metal, single-threaded lwIP configuration for saramOS/STM32F769I-DISC1 */

#define NO_SYS                      1
#define LWIP_TIMERS                 1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

/* Protocols */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ARP                    1
#define LWIP_ICMP                   1
#define LWIP_IGMP                   0
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    0
#define LWIP_RAW                    0
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0
#define SYS_LIGHTWEIGHT_PROT        0
#define LWIP_DISABLE_TCP_SANITY_CHECKS 1

/* Memory tuning (conservative for 512 KB SRAM) */
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    8192
#define MEMP_NUM_PBUF               16
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_TCP_PCB_LISTEN     2
#define MEMP_NUM_TCP_SEG            16
#define MEMP_NUM_NETBUF             0
#define MEMP_NUM_NETCONN            0
#define MEMP_NUM_TCPIP_MSG_API      0
#define MEMP_NUM_TCPIP_MSG_INPKT    0
#define PBUF_POOL_SIZE              16
#define PBUF_POOL_BUFSIZE           1524

/* TCP tuning */
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (2 * TCP_MSS)
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SNDQUEUELOWAT           (2 * TCP_MSS)
#define TCP_OVERSIZE                TCP_MSS
#define LWIP_TCP_TIMESTAMPS         0
#define LWIP_SO_RCVTIMEO            0
#define LWIP_SO_SNDTIMEO            0

/* ARP */
#define ARP_TABLE_SIZE              10
#define ETHARP_SUPPORT_STATIC_ENTRIES 1
#define ETHARP_TRUST_IP_MAC         0
#define ETHARP_SUPPORT_VLAN         0

/* Checksum: use software for reliability on first bring-up.
 * The STM32F7 MAC can offload IPv4/TCP/UDP checksums; enable later via ETH_MACCR_IPCO. */
#define CHECKSUM_BY_HARDWARE        0

#if CHECKSUM_BY_HARDWARE
  #define CHECKSUM_GEN_IP           0
  #define CHECKSUM_GEN_UDP          0
  #define CHECKSUM_GEN_TCP          0
  #define CHECKSUM_CHECK_IP         0
  #define CHECKSUM_CHECK_UDP        0
  #define CHECKSUM_CHECK_TCP        0
  #define CHECKSUM_GEN_ICMP         1
  #define CHECKSUM_CHECK_ICMP       1
#else
  #define CHECKSUM_GEN_IP           1
  #define CHECKSUM_GEN_UDP          1
  #define CHECKSUM_GEN_TCP          1
  #define CHECKSUM_CHECK_IP         1
  #define CHECKSUM_CHECK_UDP        1
  #define CHECKSUM_CHECK_TCP        1
  #define CHECKSUM_GEN_ICMP         1
  #define CHECKSUM_CHECK_ICMP       1
#endif

/* HTTP server */
#define LWIP_HTTPD                  1
#define LWIP_HTTPD_SSI              0
#define LWIP_HTTPD_CGI              0
#define LWIP_HTTPD_SUPPORT_POST     0
#define LWIP_HTTPD_SUPPORT_EXTSTATUS 0
#define LWIP_HTTPD_SUPPORT_REQUESTLIST 0
#define LWIP_HTTPD_SUPPORT_V09      0
#define LWIP_HTTPD_DYNAMIC_HEADERS  0
#define HTTPD_FSDATA_FILE           "fsdata_custom.c"

/* lwIP stats off to save RAM */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

/* Debug (off by default) */
#define LWIP_DEBUG                  0

#endif /* LWIPOPTS_H */
