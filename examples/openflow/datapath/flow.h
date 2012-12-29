#ifndef FLOW_H
#define FLOW_H 1

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/rcupdate.h>
#include <linux/gfp.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <net/ip.h>

#include "openflow/openflow.h"

#define VLAN_PCP_SHIFT 13
#define VLAN_PCP_BITMASK 0x0007 /* the least 3-bit is valid */

struct sk_buff;
struct ofp_flow_mod;

/* Identification data for a flow.
 * Network byte order except for the "wildcards" field.
 * Ordered to make bytewise comparisons (e.g. with memcmp()) fail quickly and
 * to keep the amount of padding to a minimum.
 * If you change the ordering of fields here, change flow_keys_equal() to
 * compare the proper fields.
 */
struct sw_flow_key {
	uint32_t nw_src;	/* IP source address. */
	uint32_t nw_dst;	/* IP destination address. */
	uint16_t in_port;	/* Input switch port */
	uint16_t dl_vlan;	/* Input VLAN id. */
	uint16_t dl_type;	/* Ethernet frame type. */
	uint16_t tp_src;	/* TCP/UDP source port. */
	uint16_t tp_dst;	/* TCP/UDP destination port. */
	uint8_t dl_src[ETH_ALEN]; /* Ethernet source address. */
	uint8_t dl_dst[ETH_ALEN]; /* Ethernet destination address. */
	uint8_t dl_vlan_pcp;	/* Input VLAN priority. */
	uint8_t nw_tos;		/* IPv4 DSCP */
	uint8_t nw_proto;	/* IP protocol. */
	uint8_t pad[3];
	uint32_t wildcards;	/* Wildcard fields (host byte order). */
	uint32_t nw_src_mask;	/* 1-bit in each significant nw_src bit. */
	uint32_t nw_dst_mask;	/* 1-bit in each significant nw_dst bit. */
};

/* The match fields for ICMP type and code use the transport source and 
 * destination port fields, respectively. */
#define icmp_type tp_src
#define icmp_code tp_dst

/* Compare two sw_flow_keys and return true if they are the same flow, false
 * otherwise.  Wildcards and netmasks are not considered. */
static inline int flow_keys_equal(const struct sw_flow_key *a,
				   const struct sw_flow_key *b) 
{
	return !memcmp(a, b, offsetof(struct sw_flow_key, wildcards));
}

/* We need to manually make sure that the structure is 32-bit aligned,
 * since we don't want garbage values in compiler-generated pads from
 * messing up hash matches.
 */
static inline void check_key_align(void)
{
	BUILD_BUG_ON(sizeof(struct sw_flow_key) != 48);
}

/* We keep actions as a separate structure because we need to be able to 
 * swap them out atomically when the modify command comes from a Flow
 * Modify message. */
struct sw_flow_actions {
	size_t actions_len;
	struct rcu_head rcu;

	struct ofp_action_header actions[0];
};

/* Locking:
 *
 * - Readers must take rcu_read_lock and hold it the entire time that the flow
 *   must continue to exist.
 *
 * - Writers must hold dp_mutex.
 */
struct sw_flow {
	struct sw_flow_key key;

	uint16_t priority;      /* Only used on entries with wildcards. */
	uint16_t idle_timeout;	/* Idle time before discarding (seconds). */
	uint16_t hard_timeout;  /* Hard expiration time (seconds) */
	uint64_t used;          /* Last used time (in jiffies). */
	uint8_t send_flow_rem;  /* Send a flow removed to the controller */
	uint8_t emerg_flow;     /* Emergency flow indicator */

	struct sw_flow_actions *sf_acts;

	/* For use by table implementation. */
	struct list_head node;
	struct list_head iter_node;
	unsigned long serial;
	void *private;

	spinlock_t lock;         /* Lock this entry...mostly for stat updates */
	uint64_t created;        /* When the flow was created (in jiffies_64). */
	uint64_t packet_count;   /* Number of packets associated with this entry */
	uint64_t byte_count;     /* Number of bytes associated with this entry */

	struct rcu_head rcu;
};

int flow_matches_1wild(const struct sw_flow_key *, const struct sw_flow_key *);
int flow_matches_2wild(const struct sw_flow_key *, const struct sw_flow_key *);
int flow_matches_desc(const struct sw_flow_key *, const struct sw_flow_key *, 
		int);
int flow_matches_2desc(const struct sw_flow_key *, const struct sw_flow_key *,
		int);
int flow_has_out_port(struct sw_flow *, uint16_t);
struct sw_flow *flow_alloc(size_t actions_len, gfp_t flags);
void flow_free(struct sw_flow *);
void flow_deferred_free(struct sw_flow *);
void flow_deferred_free_acts(struct sw_flow_actions *);
void flow_setup_actions(struct sw_flow *, const struct ofp_action_header *,
			int);
void flow_replace_acts(struct sw_flow *, const struct ofp_action_header *, 
		size_t);
int flow_extract(struct sk_buff *, uint16_t in_port, struct sw_flow_key *);
void flow_extract_match(struct sw_flow_key* to, const struct ofp_match* from);
void flow_fill_match(struct ofp_match* to, const struct sw_flow_key* from);
int flow_timeout(struct sw_flow *);

void print_flow(const struct sw_flow_key *);

static inline int iphdr_ok(struct sk_buff *skb)
{
	int nh_ofs = skb_network_offset(skb);
	if (skb->len >= nh_ofs + sizeof(struct iphdr)) {
		int ip_len = ip_hdrlen(skb);
		return (ip_len >= sizeof(struct iphdr)
			&& pskb_may_pull(skb, nh_ofs + ip_len));
	}
	return 0;
}

static inline int tcphdr_ok(struct sk_buff *skb)
{
	int th_ofs = skb_transport_offset(skb);
	if (pskb_may_pull(skb, th_ofs + sizeof(struct tcphdr))) {
		int tcp_len = tcp_hdrlen(skb);
		return (tcp_len >= sizeof(struct tcphdr)
			&& skb->len >= th_ofs + tcp_len);
	}
	return 0;
}

static inline int udphdr_ok(struct sk_buff *skb)
{
	int th_ofs = skb_transport_offset(skb);
	return pskb_may_pull(skb, th_ofs + sizeof(struct udphdr));
}

static inline int icmphdr_ok(struct sk_buff *skb)
{
	int th_ofs = skb_transport_offset(skb);
	return pskb_may_pull(skb, th_ofs + sizeof(struct icmphdr));
}

#define TCP_FLAGS_OFFSET 13
#define TCP_FLAG_MASK 0x3f

static inline struct ofp_tcphdr *ofp_tcp_hdr(const struct sk_buff *skb)
{
	return (struct ofp_tcphdr *)skb_transport_header(skb);
}

static inline void flow_used(struct sw_flow *flow, struct sk_buff *skb) 
{
	unsigned long flags;

	flow->used = get_jiffies_64();

	spin_lock_irqsave(&flow->lock, flags);
	flow->packet_count++;
	flow->byte_count += skb->len;
	spin_unlock_irqrestore(&flow->lock, flags);
}

extern struct kmem_cache *flow_cache;

int flow_init(void);
void flow_exit(void);

#endif /* flow.h */
