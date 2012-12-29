
/*
 * Distributed under the terms of the GNU GPL version 2.
 * Copyright (c) 2007, 2008 The Board of Trustees of The Leland 
 * Stanford Junior University
 */

#include "flow.h"
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/llc_pdu.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/llc.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/rcupdate.h>

#include "openflow/openflow.h"
#include "openflow/nicira-ext.h"
#include "compat.h"

struct kmem_cache *flow_cache;

/* Internal function used to compare fields in flow. */
static inline
int flow_fields_match(const struct sw_flow_key *a, const struct sw_flow_key *b,
		      uint32_t w, uint32_t src_mask, uint32_t dst_mask)
{
	return ((w & OFPFW_IN_PORT || a->in_port == b->in_port)
		&& (w & OFPFW_DL_VLAN || a->dl_vlan == b->dl_vlan)
		&& (w & OFPFW_DL_VLAN_PCP || a->dl_vlan_pcp == b->dl_vlan_pcp)
		&& (w & OFPFW_DL_SRC || !memcmp(a->dl_src, b->dl_src, ETH_ALEN))
		&& (w & OFPFW_DL_DST || !memcmp(a->dl_dst, b->dl_dst, ETH_ALEN))
		&& (w & OFPFW_DL_TYPE || a->dl_type == b->dl_type)
		&& (w & OFPFW_NW_TOS || a->nw_tos == b->nw_tos)
		&& (w & OFPFW_NW_PROTO || a->nw_proto == b->nw_proto)
		&& !((a->nw_src ^ b->nw_src) & src_mask)
		&& !((a->nw_dst ^ b->nw_dst) & dst_mask)
		&& (w & OFPFW_TP_SRC || a->tp_src == b->tp_src)
		&& (w & OFPFW_TP_DST || a->tp_dst == b->tp_dst));
}

/* Returns nonzero if 'a' and 'b' match, that is, if their fields are equal
 * modulo wildcards in 'b', zero otherwise. */
int flow_matches_1wild(const struct sw_flow_key *a,
		       const struct sw_flow_key *b)
{
	return flow_fields_match(a, b, b->wildcards,
				 b->nw_src_mask, b->nw_dst_mask);
}
EXPORT_SYMBOL(flow_matches_1wild);

/* Returns nonzero if 'a' and 'b' match, that is, if their fields are equal
 * modulo wildcards in 'a' or 'b', zero otherwise. */
int flow_matches_2wild(const struct sw_flow_key *a,
		       const struct sw_flow_key *b)
{
	return flow_fields_match(a, b,
				 a->wildcards | b->wildcards,
				 a->nw_src_mask & b->nw_src_mask,
				 a->nw_dst_mask & b->nw_dst_mask);
}
EXPORT_SYMBOL(flow_matches_2wild);

/* Returns nonzero if 't' (the table entry's key) and 'd' (the key
 * describing the match) match, that is, if their fields are
 * equal modulo wildcards, zero otherwise.  If 'strict' is nonzero, the
 * wildcards must match in both 't_key' and 'd_key'.  Note that the
 * table's wildcards are ignored unless 'strict' is set. */
int flow_matches_desc(const struct sw_flow_key *t, const struct sw_flow_key *d, 
		int strict)
{
	if (strict && d->wildcards != t->wildcards)
		return 0;
	return flow_matches_1wild(t, d);
}
EXPORT_SYMBOL(flow_matches_desc);

/* Returns nonzero if 't' (the table entry's key) and 'd' (the key
 * describing the match) match, that is, if their fields are
 * equal modulo 't' or 'd' wildcards, zero otherwise.  If 'strict' is nonzero, the
 * wildcards must match in both 't_key' and 'd_key'.  Note that the
 * table's wildcards are ignored unless 'strict' is set. */
int
flow_matches_2desc(const struct sw_flow_key *t, const struct sw_flow_key *d,
        int strict)
{
    if (strict && d->wildcards != t->wildcards) {
        return 0;
    }
    return flow_matches_2wild(t, d);
}
EXPORT_SYMBOL(flow_matches_2desc);

static uint32_t make_nw_mask(int n_wild_bits)
{
	n_wild_bits &= (1u << OFPFW_NW_SRC_BITS) - 1;
	return n_wild_bits < 32 ? htonl(~((1u << n_wild_bits) - 1)) : 0;
}

void flow_extract_match(struct sw_flow_key* to, const struct ofp_match* from)
{
	to->wildcards = ntohl(from->wildcards) & OFPFW_ALL;
	to->dl_vlan_pcp = from->dl_vlan_pcp;
	to->in_port = from->in_port;
	to->dl_vlan = from->dl_vlan;
	memcpy(to->dl_src, from->dl_src, ETH_ALEN);
	memcpy(to->dl_dst, from->dl_dst, ETH_ALEN);
	to->dl_type = from->dl_type;

	to->nw_tos = to->nw_proto = to->nw_src = to->nw_dst = 0;
	to->tp_src = to->tp_dst = 0;
	memset(to->pad, 0, sizeof(to->pad));

#define OFPFW_TP (OFPFW_TP_SRC | OFPFW_TP_DST)
#define OFPFW_NW (OFPFW_NW_TOS | OFPFW_NW_PROTO | OFPFW_NW_SRC_MASK | OFPFW_NW_DST_MASK)
	if (to->wildcards & OFPFW_DL_TYPE) {
		/* Can't sensibly match on network or transport headers if the
		 * data link type is unknown. */
		to->wildcards |= OFPFW_NW | OFPFW_TP;
	} else if (from->dl_type == htons(ETH_P_IP)) {
		to->nw_tos   = from->nw_tos & 0xfc;
		to->nw_proto = from->nw_proto;
		to->nw_src   = from->nw_src;
		to->nw_dst   = from->nw_dst;

		if (to->wildcards & OFPFW_NW_PROTO) {
			/* Can't sensibly match on transport headers if the
			 * network protocol is unknown. */
			to->wildcards |= OFPFW_TP;
		} else if (from->nw_proto == IPPROTO_TCP
				|| from->nw_proto == IPPROTO_UDP
				|| from->nw_proto == IPPROTO_ICMP) {
			to->tp_src = from->tp_src;
			to->tp_dst = from->tp_dst;
		} else {
			/* Transport layer fields are undefined.  Mark them as
			 * exact-match to allow such flows to reside in
			 * table-hash, instead of falling into table-linear. */
			to->wildcards &= ~OFPFW_TP;
		}
	} else {
		/* Network and transport layer fields are undefined.  Mark them
		 * as exact-match to allow such flows to reside in table-hash,
		 * instead of falling into table-linear. */
		to->wildcards &= ~(OFPFW_NW | OFPFW_TP);
	}

	/* We set these late because code above adjusts to->wildcards. */
	to->nw_src_mask = make_nw_mask(to->wildcards >> OFPFW_NW_SRC_SHIFT);
	to->nw_dst_mask = make_nw_mask(to->wildcards >> OFPFW_NW_DST_SHIFT);
}

void flow_fill_match(struct ofp_match* to, const struct sw_flow_key* from)
{
	to->wildcards = htonl(from->wildcards);
	to->in_port   = from->in_port;
	to->dl_vlan   = from->dl_vlan;
	memcpy(to->dl_src, from->dl_src, ETH_ALEN);
	memcpy(to->dl_dst, from->dl_dst, ETH_ALEN);
	to->dl_type   = from->dl_type;
	to->nw_tos    = from->nw_tos;
	to->nw_proto  = from->nw_proto;
	to->nw_src    = from->nw_src;
	to->nw_dst    = from->nw_dst;
	to->tp_src    = from->tp_src;
	to->tp_dst    = from->tp_dst;
	to->dl_vlan_pcp  = from->dl_vlan_pcp;
}

int flow_timeout(struct sw_flow *flow)
{
	if (flow->idle_timeout != OFP_FLOW_PERMANENT
	    && time_after64(get_jiffies_64(), flow->used + flow->idle_timeout * HZ))
		return OFPRR_IDLE_TIMEOUT;
	else if (flow->hard_timeout != OFP_FLOW_PERMANENT
		 && time_after64(get_jiffies_64(),
			       flow->created + flow->hard_timeout * HZ))
		return OFPRR_HARD_TIMEOUT;
	else
		return -1;
}
EXPORT_SYMBOL(flow_timeout);

/* Returns nonzero if 'flow' contains an output action to 'out_port' or
 * has the value OFPP_NONE. 'out_port' is in network-byte order. */
int flow_has_out_port(struct sw_flow *flow, uint16_t out_port)
{
	struct sw_flow_actions *sf_acts;
	size_t actions_len;
	uint8_t *p;

	if (out_port == htons(OFPP_NONE))
		return 1;

	sf_acts = rcu_dereference(flow->sf_acts);

	actions_len = sf_acts->actions_len;
	p = (uint8_t *)sf_acts->actions;

	while (actions_len > 0) {
		struct ofp_action_header *ah = (struct ofp_action_header *)p;
		size_t len = ntohs(ah->len);

		if (ah->type == htons(OFPAT_OUTPUT)) {
			struct ofp_action_output *oa = (struct ofp_action_output *)p;
			if (oa->port == out_port)
				return 1;
		}

		p += len;
		actions_len -= len;
	}

	return 0;
}
EXPORT_SYMBOL(flow_has_out_port);

/* Allocates and returns a new flow with room for 'actions_len' actions, 
 * using allocation flags 'flags'.  Returns the new flow or a null pointer 
 * on failure. */
struct sw_flow *flow_alloc(size_t actions_len, gfp_t flags)
{
	struct sw_flow_actions *sfa;
	size_t size = sizeof *sfa + actions_len;
	struct sw_flow *flow = kmem_cache_alloc(flow_cache, flags);
	if (unlikely(!flow))
		return NULL;

	sfa = kmalloc(size, flags);
	if (unlikely(!sfa)) {
		kmem_cache_free(flow_cache, flow);
		return NULL;
	}
	sfa->actions_len = actions_len;
	flow->sf_acts = sfa;

	return flow;
}

/* Frees 'flow' immediately. */
void flow_free(struct sw_flow *flow)
{
	if (unlikely(!flow))
		return;
	kfree(flow->sf_acts);
	kmem_cache_free(flow_cache, flow);
}
EXPORT_SYMBOL(flow_free);

/* RCU callback used by flow_deferred_free. */
static void rcu_free_flow_callback(struct rcu_head *rcu)
{
	struct sw_flow *flow = container_of(rcu, struct sw_flow, rcu);
	flow_free(flow);
}

/* Schedules 'flow' to be freed after the next RCU grace period.
 * The caller must hold rcu_read_lock for this to be sensible. */
void flow_deferred_free(struct sw_flow *flow)
{
	call_rcu(&flow->rcu, rcu_free_flow_callback);
}
EXPORT_SYMBOL(flow_deferred_free);

/* RCU callback used by flow_deferred_free_acts. */
static void rcu_free_acts_callback(struct rcu_head *rcu)
{
	struct sw_flow_actions *sf_acts = container_of(rcu, 
			struct sw_flow_actions, rcu);
	kfree(sf_acts);
}

/* Schedules 'sf_acts' to be freed after the next RCU grace period.
 * The caller must hold rcu_read_lock for this to be sensible. */
void flow_deferred_free_acts(struct sw_flow_actions *sf_acts)
{
	call_rcu(&sf_acts->rcu, rcu_free_acts_callback);
}
EXPORT_SYMBOL(flow_deferred_free_acts);

/* Setup the action on the flow, just after it was created with flow_alloc().
 * Jean II */
void flow_setup_actions(struct sw_flow *                       flow,
			const struct ofp_action_header *        actions,
			int                                     actions_len)
{
	/* Basic init of the flow stucture */
	flow->used = flow->created = get_jiffies_64();
	flow->byte_count = 0;
	flow->packet_count = 0;
	spin_lock_init(&flow->lock);

	/* Make sure we don't blow the allocation done earlier */
	if(actions_len > flow->sf_acts->actions_len) {
		printk(KERN_ERR "flow_setup_actions: actions_len is too big (%d > %d)",
		       actions_len, flow->sf_acts->actions_len);
		return;
	}

	/* Setup the actions - No need for RCU at this point ;-) */
	memcpy(flow->sf_acts->actions, actions, actions_len);
}

/* Copies 'actions' into a newly allocated structure for use by 'flow'
 * and safely frees the structure that defined the previous actions. */
void flow_replace_acts(struct sw_flow *flow, 
		const struct ofp_action_header *actions, size_t actions_len)
{
	struct sw_flow_actions *sfa;
	struct sw_flow_actions *orig_sfa = flow->sf_acts;
	size_t size = sizeof *sfa + actions_len;

	sfa = kmalloc(size, GFP_ATOMIC);
	if (unlikely(!sfa))
		return;

	sfa->actions_len = actions_len;
	memcpy(sfa->actions, actions, actions_len);

	rcu_assign_pointer(flow->sf_acts, sfa);
	flow_deferred_free_acts(orig_sfa);

	return;
}
EXPORT_SYMBOL(flow_replace_acts);

/* Prints a representation of 'key' to the kernel log. */
void print_flow(const struct sw_flow_key *key)
{
	printk("wild %08x port %04x vlan-vid %04x vlan-pcp %02x "
	       "src-mac %02x:%02x:%02x:%02x:%02x:%02x "
	       "dst-mac %02x:%02x:%02x:%02x:%02x:%02x "
	       "frm-type %04x ip-tos %02x ip-proto %02x "
	       "src-ip %u.%u.%u.%u dst-ip %u.%u.%u.%u tp-src %d tp-dst %d\n",
			key->wildcards, ntohs(key->in_port), ntohs(key->dl_vlan),
			key->dl_vlan_pcp,
			key->dl_src[0], key->dl_src[1], key->dl_src[2],
			key->dl_src[3], key->dl_src[4], key->dl_src[5],
			key->dl_dst[0], key->dl_dst[1], key->dl_dst[2],
			key->dl_dst[3], key->dl_dst[4], key->dl_dst[5],
			ntohs(key->dl_type),
			key->nw_tos, key->nw_proto,
			((unsigned char *)&key->nw_src)[0],
			((unsigned char *)&key->nw_src)[1],
			((unsigned char *)&key->nw_src)[2],
			((unsigned char *)&key->nw_src)[3],
			((unsigned char *)&key->nw_dst)[0],
			((unsigned char *)&key->nw_dst)[1],
			((unsigned char *)&key->nw_dst)[2],
			((unsigned char *)&key->nw_dst)[3],
			ntohs(key->tp_src), ntohs(key->tp_dst));
}
EXPORT_SYMBOL(print_flow);

#define SNAP_OUI_LEN 3

struct eth_snap_hdr
{
	struct ethhdr eth;
	uint8_t  dsap;  /* Always 0xAA */
	uint8_t  ssap;  /* Always 0xAA */
	uint8_t  ctrl;
	uint8_t  oui[SNAP_OUI_LEN];
	uint16_t ethertype;
} __attribute__ ((packed));

static int is_snap(const struct eth_snap_hdr *esh)
{
	return (esh->dsap == LLC_SAP_SNAP
		&& esh->ssap == LLC_SAP_SNAP
		&& !memcmp(esh->oui, "\0\0\0", 3));
}

/* Parses the Ethernet frame in 'skb', which was received on 'in_port',
 * and initializes 'key' to match.  Returns 1 if 'skb' contains an IP
 * fragment, 0 otherwise. */
int flow_extract(struct sk_buff *skb, uint16_t in_port,
		 struct sw_flow_key *key)
{
	struct ethhdr *eth;
	struct eth_snap_hdr *esh;
	int retval = 0;
	int nh_ofs;

	memset(key, 0, sizeof *key);
	key->dl_vlan = htons(OFP_VLAN_NONE);
	key->in_port = htons(in_port);

	if (skb->len < sizeof *eth)
		return 0;
	if (!pskb_may_pull(skb, skb->len >= 64 ? 64 : skb->len)) {
		return 0;
	}

	skb_reset_mac_header(skb);
	eth = eth_hdr(skb);
	esh = (struct eth_snap_hdr *) eth;
	nh_ofs = sizeof *eth;
	if (likely(ntohs(eth->h_proto) >= OFP_DL_TYPE_ETH2_CUTOFF))
		key->dl_type = eth->h_proto;
	else if (skb->len >= sizeof *esh && is_snap(esh)) {
		key->dl_type = esh->ethertype;
		nh_ofs = sizeof *esh;
	} else {
		key->dl_type = htons(OFP_DL_TYPE_NOT_ETH_TYPE);
		if (skb->len >= nh_ofs + sizeof(struct llc_pdu_un)) {
			nh_ofs += sizeof(struct llc_pdu_un); 
		}
	}

	/* Check for a VLAN tag */
	if (key->dl_type == htons(ETH_P_8021Q) &&
	    skb->len >= nh_ofs + sizeof(struct vlan_hdr)) {
		struct vlan_hdr *vh = (struct vlan_hdr*)(skb->data + nh_ofs);
		key->dl_type = vh->h_vlan_encapsulated_proto;
		key->dl_vlan = vh->h_vlan_TCI & htons(VLAN_VID_MASK);
		key->dl_vlan_pcp = (uint8_t)((ntohs(vh->h_vlan_TCI) >> VLAN_PCP_SHIFT)
		                             & VLAN_PCP_BITMASK);
		nh_ofs += sizeof(struct vlan_hdr);
	}
	memcpy(key->dl_src, eth->h_source, ETH_ALEN);
	memcpy(key->dl_dst, eth->h_dest, ETH_ALEN);
	skb_set_network_header(skb, nh_ofs);

	/* Network layer. */
	if (key->dl_type == htons(ETH_P_IP) && iphdr_ok(skb)) {
		struct iphdr *nh = ip_hdr(skb);
		int th_ofs = nh_ofs + nh->ihl * 4;
		key->nw_tos = nh->tos & 0xfc;
		key->nw_proto = nh->protocol;
		key->nw_src = nh->saddr;
		key->nw_dst = nh->daddr;
		skb_set_transport_header(skb, th_ofs);

		/* Transport layer. */
		if (!(nh->frag_off & htons(IP_MF | IP_OFFSET))) {
			if (key->nw_proto == IPPROTO_TCP) {
				if (tcphdr_ok(skb)) {
					struct tcphdr *tcp = tcp_hdr(skb);
					key->tp_src = tcp->source;
					key->tp_dst = tcp->dest;
				} else {
					/* Avoid tricking other code into
					 * thinking that this packet has an L4
					 * header. */
					key->nw_proto = 0;
				}
			} else if (key->nw_proto == IPPROTO_UDP) {
				if (udphdr_ok(skb)) {
					struct udphdr *udp = udp_hdr(skb);
					key->tp_src = udp->source;
					key->tp_dst = udp->dest;
				} else {
					/* Avoid tricking other code into
					 * thinking that this packet has an L4
					 * header. */
					key->nw_proto = 0;
				}
			} else if (key->nw_proto == IPPROTO_ICMP) {
				if (icmphdr_ok(skb)) {
					struct icmphdr *icmp = icmp_hdr(skb);
					/* The ICMP type and code fields use the 16-bit
					 * transport port fields, so we need to store them
					 * in 16-bit network byte order. */
					key->icmp_type = htons(icmp->type);
					key->icmp_code = htons(icmp->code);
				} else {
					/* Avoid tricking other code into
					 * thinking that this packet has an L4
					 * header. */
					key->nw_proto = 0;
				}
			}
		} else {
			retval = 1;
		}
	} else {
		skb_reset_transport_header(skb);
	}
	return retval;
}

/* Initializes the flow module.
 * Returns zero if successful or a negative error code. */
int flow_init(void)
{
	flow_cache = kmem_cache_create("sw_flow", sizeof(struct sw_flow), 0,
					0, NULL);
	if (flow_cache == NULL)
		return -ENOMEM;

	return 0;
}

/* Uninitializes the flow module. */
void flow_exit(void)
{
	kmem_cache_destroy(flow_cache);
}

