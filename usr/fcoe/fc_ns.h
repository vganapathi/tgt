/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _FC_NS_H_
#define	_FC_NS_H_

/*
 * Fibre Channel Services - Name Service (dNS)
 * From T11.org FC-GS-2 Rev 5.3 November 1998.
 */

/*
 * Common-transport sub-type for Name Server.
 */
#define	FC_NS_SUBTYPE	    2	/* fs_ct_hdr.ct_fs_subtype */

/*
 * Name server Requests.
 * Note:  this is an incomplete list, some unused requests are omitted.
 */
enum fc_ns_req {
	FC_NS_GA_NXT =	0x0100,		/* get all next */
	FC_NS_GI_A =	0x0101,		/* get identifiers - scope */
	FC_NS_GPN_ID =	0x0112,		/* get port name by ID */
	FC_NS_GNN_ID =	0x0113,		/* get node name by ID */
	FC_NS_GID_PN =	0x0121,		/* get ID for port name */
	FC_NS_GID_NN =	0x0131,		/* get IDs for node name */
	FC_NS_GID_FT =	0x0171,		/* get IDs by FC4 type */
	FC_NS_GPN_FT =	0x0172,		/* get port names by FC4 type */
	FC_NS_GID_PT =	0x01a1,		/* get IDs by port type */
	FC_NS_RFT_ID =	0x0217,		/* reg FC4 type for ID */
	FC_NS_RPN_ID =	0x0212,		/* reg port name for ID */
	FC_NS_RNN_ID =	0x0213,		/* reg node name for ID */
};

/*
 * Port type values.
 */
enum fc_ns_pt {
	FC_NS_UNID_PORT = 0x00,	/* unidentified */
	FC_NS_N_PORT =	0x01,	/* N port */
	FC_NS_NL_PORT =	0x02,	/* NL port */
	FC_NS_FNL_PORT = 0x03,	/* F/NL port */
	FC_NS_NX_PORT =	0x7f,	/* Nx port */
	FC_NS_F_PORT =	0x81,	/* F port */
	FC_NS_FL_PORT =	0x82,	/* FL port */
	FC_NS_E_PORT =	0x84,	/* E port */
	FC_NS_B_PORT =	0x85,	/* B port */
};

/*
 * Port type object.
 */
struct fc_ns_pt_obj {
	net8_t		pt_type;
};

/*
 * Port ID object
 */
struct fc_ns_fid {
	net8_t		fp_flags;	/* flags for responses only */
	net24_t		fp_fid;
};

/*
 * fp_flags in port ID object, for responses only.
 */
#define	FC_NS_FID_LAST	0x80		/* last object */

/*
 * FC4-types object.
 */
#define	FC_NS_TYPES	256	/* number of possible FC-4 types */
#define	FC_NS_BPW	32	/* bits per word in bitmap */

struct fc_ns_fts {
	net32_t	ff_type_map[FC_NS_TYPES / FC_NS_BPW]; /* bitmap of FC-4 types */
};

/*
 * GID_PT request.
 */
struct fc_ns_gid_pt {
	net8_t		fn_pt_type;
	net8_t		fn_domain_id_scope;
	net8_t		fn_area_id_scope;
	net8_t		fn_resvd;
};

/*
 * GID_FT or GPN_FT request.
 */
struct fc_ns_gid_ft {
	net8_t		fn_resvd;
	net8_t		fn_domain_id_scope;
	net8_t		fn_area_id_scope;
	net8_t		fn_fc4_type;
};

/*
 * GPN_FT response.
 */
struct fc_gpn_ft_resp {
	net8_t		fp_flags;	/* see fp_flags definitions above */
	net24_t		fp_fid;		/* port ID */
	net32_t		fp_resvd;
	net64_t		fp_wwpn;	/* port name */
};

/*
 * GID_PN request
 */
struct fc_ns_gid_pn {
	net64_t     fn_wwpn;    /* port name */
};

/*
 * GID_PN response
 */
struct fc_gid_pn_resp {
	net8_t      fp_resvd;
	net24_t     fp_fid;     /* port ID */
};

/*
 * RFT_ID request - register FC-4 types for ID.
 */
struct fc_ns_rft_id {
	struct fc_ns_fid fr_fid;	/* port ID object */
	struct fc_ns_fts fr_fts;	/* FC-4 types object */
};

/*
 * RPN_ID request - register port name for ID.
 * RNN_ID request - register node name for ID.
 */
struct fc_ns_rn_id {
	struct fc_ns_fid fr_fid;	/* port ID object */
	net64_t		fr_wwn;		/* node name or port name */
};

#endif /* _FC_NS_H_ */
