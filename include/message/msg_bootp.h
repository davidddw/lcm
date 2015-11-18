#ifndef _MESSAGE_MSG_BOOTP_H
#define _MESSAGE_MSG_BOOTP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* DHCP message types. */
#define DHCPDISCOVER    1
#define DHCPOFFER       2
#define DHCPREQUEST     3
#define DHCPDECLINE     4
#define DHCPACK         5
#define DHCPNAK         6
#define DHCPRELEASE     7
#define DHCPINFORM      8
#define DHCPLEASEQUERY      10
#define DHCPLEASEUNASSIGNED 11
#define DHCPLEASEUNKNOWN    12
#define DHCPLEASEACTIVE     13


#define IP_HDR_SIZE         20
#define UDP_HDR_SIZE        8
#define DHCP_UDP_OVERHEAD   (IP_HDR_SIZE + UDP_HDR_SIZE)   
                            /* IP header + UDP header */
#define DHCP_SNAME_LEN      64
#define DHCP_FILE_LEN       128
#define DHCP_FIXED_NON_UDP  236
#define DHCP_FIXED_LEN      (DHCP_FIXED_NON_UDP + DHCP_UDP_OVERHEAD)
                            /* Everything but options. */
//#define BOOTP_MIN_LEN       300
#define BOOTP_MIN_LEN       120

#define DHCP_MTU_MAX        1500
#define DHCP_MTU_MIN        576

#define DHCP_MAX_OPTION_LEN	(DHCP_MTU_MAX - DHCP_FIXED_LEN)
#define DHCP_MIN_OPTION_LEN (DHCP_MTU_MIN - DHCP_FIXED_LEN)

typedef struct dhcp_packet_s {
    u_int8_t  op;              /* 0: Message opcode/type */
	u_int8_t  htype;           /* 1: Hardware addr type (net/if_types.h) */
	u_int8_t  hlen;            /* 2: Hardware addr length */
	u_int8_t  hops;            /* 3: Number of relay agent hops from client */
	u_int32_t xid;             /* 4: Transaction ID */
	u_int16_t secs;            /* 8: Seconds since client started looking */
	u_int16_t flags;           /* 10: Flag bits */
	struct in_addr ciaddr;     /* 12: Client IP address (if already in use) */
	struct in_addr yiaddr;     /* 16: Client IP address */
	struct in_addr siaddr;     /* 18: IP address of next server to talk to */
	struct in_addr giaddr;     /* 20: DHCP relay agent IP address */
	unsigned char chaddr [16]; /* 24: Client hardware address */
	char sname [DHCP_SNAME_LEN];	/* 40: Server name */
	char file [DHCP_FILE_LEN]; /* 104: Boot filename */
	unsigned char options [DHCP_MAX_OPTION_LEN];
                               /* 
                                * 212: Optional parameters 
                                * (actual length dependent on MTU). 
                                * */
} dhcp_packet_t;


typedef struct udphdr_s {
	u_int16_t uh_sport;		/* source port */
	u_int16_t uh_dport;		/* destination port */
	u_int16_t uh_ulen;		/* udp length */
	u_int16_t uh_sum;		/* udp checksum */
} udphdr_t;

#define	ETHER_ADDR_LEN	6

typedef struct ether_header_s {
	u_int8_t  ether_dhost[ETHER_ADDR_LEN];
	u_int8_t  ether_shost[ETHER_ADDR_LEN];
	u_int16_t ether_type;
} ether_header_t;

#define ETHER_HDR_SIZE (ETHER_ADDR_LEN * 2 + sizeof (u_int16_t))

#endif /* _MESSAGE_MSG_BOOTP_H */
