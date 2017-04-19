/* Copyright (c) 2016-2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_common.h
 * \brief Header file containing common data for the whole HS subsytem.
 **/

#ifndef TOR_HS_COMMON_H
#define TOR_HS_COMMON_H

#include "or.h"

/* Protocol version 2. Use this instead of hardcoding "2" in the code base,
 * this adds a clearer semantic to the value when used. */
#define HS_VERSION_TWO 2
/* Version 3 of the protocol (prop224). */
#define HS_VERSION_THREE 3
/* Earliest and latest version we support. */
#define HS_VERSION_MIN HS_VERSION_TWO
#define HS_VERSION_MAX HS_VERSION_THREE

/** Try to maintain this many intro points per service by default. */
#define NUM_INTRO_POINTS_DEFAULT 3
/** Maximum number of intro points per generic and version 2 service. */
#define NUM_INTRO_POINTS_MAX 10
/** Number of extra intro points we launch if our set of intro nodes is empty.
 * See proposal 155, section 4. */
#define NUM_INTRO_POINTS_EXTRA 2

/** If we can't build our intro circuits, don't retry for this long. */
#define INTRO_CIRC_RETRY_PERIOD (60*5)
/** Don't try to build more than this many circuits before giving up for a
 * while.*/
#define MAX_INTRO_CIRCS_PER_PERIOD 10
/** How many times will a hidden service operator attempt to connect to a
 * requested rendezvous point before giving up? */
#define MAX_REND_FAILURES 1
/** How many seconds should we spend trying to connect to a requested
 * rendezvous point before giving up? */
#define MAX_REND_TIMEOUT 30

/* String prefix for the signature of ESTABLISH_INTRO */
#define ESTABLISH_INTRO_SIG_PREFIX "Tor establish-intro cell v1"

/* The default HS time period length */
#define HS_TIME_PERIOD_LENGTH_DEFAULT 1440 /* 1440 minutes == one day */
/* The minimum time period length as seen in prop224 section [TIME-PERIODS] */
#define HS_TIME_PERIOD_LENGTH_MIN 30 /* minutes */
/* The minimum time period length as seen in prop224 section [TIME-PERIODS] */
#define HS_TIME_PERIOD_LENGTH_MAX (60 * 24 * 10) /* 10 days or 14400 minutes */
/* The time period rotation offset as seen in prop224 section [TIME-PERIODS] */
#define HS_TIME_PERIOD_ROTATION_OFFSET (12 * 60) /* minutes */

/* Prefix of the onion address checksum. */
#define HS_SERVICE_ADDR_CHECKSUM_PREFIX ".onion checksum"
/* Length of the checksum prefix minus the NUL terminated byte. */
#define HS_SERVICE_ADDR_CHECKSUM_PREFIX_LEN \
  (sizeof(HS_SERVICE_ADDR_CHECKSUM_PREFIX) - 1)
/* Length of the resulting checksum of the address. The construction of this
 * checksum looks like:
 *   CHECKSUM = ".onion checksum" || PUBKEY || VERSION
 * where VERSION is 1 byte. This is pre-hashing. */
#define HS_SERVICE_ADDR_CHECKSUM_LEN \
  (HS_SERVICE_ADDR_CHECKSUM_PREFIX_LEN + ED25519_PUBKEY_LEN + sizeof(uint8_t))
/* The amount of bytes we use from the address checksum. */
#define HS_SERVICE_ADDR_CHECKSUM_LEN_USED 2
/* Length of the binary encoded service address which is of course before the
 * base32 encoding. Construction is:
 *    PUBKEY || CHECKSUM || VERSION
 * with 1 byte VERSION and 2 bytes CHECKSUM. The following is 35 bytes. */
#define HS_SERVICE_ADDR_LEN \
  (ED25519_PUBKEY_LEN + HS_SERVICE_ADDR_CHECKSUM_LEN_USED + sizeof(uint8_t))
/* Length of 'y' portion of 'y.onion' URL. This is base32 encoded and the
 * length ends up to 56 bytes (not counting the terminated NUL byte.) */
#define HS_SERVICE_ADDR_LEN_BASE32 \
  (CEIL_DIV(HS_SERVICE_ADDR_LEN * 8, 5))

/* The default HS time period length */
#define HS_TIME_PERIOD_LENGTH_DEFAULT 1440 /* 1440 minutes == one day */
/* The minimum time period length as seen in prop224 section [TIME-PERIODS] */
#define HS_TIME_PERIOD_LENGTH_MIN 30 /* minutes */
/* The minimum time period length as seen in prop224 section [TIME-PERIODS] */
#define HS_TIME_PERIOD_LENGTH_MAX (60 * 24 * 10) /* 10 days or 14400 minutes */
/* The time period rotation offset as seen in prop224 section [TIME-PERIODS] */
#define HS_TIME_PERIOD_ROTATION_OFFSET (12 * 60) /* minutes */

/* Keyblinding parameter construction is as follow:
 *    "key-blind" || INT_8(period_num) || INT_8(start_period_sec) */
#define HS_KEYBLIND_NONCE_PREFIX "key-blind"
#define HS_KEYBLIND_NONCE_PREFIX_LEN (sizeof(HS_KEYBLIND_NONCE_PREFIX) - 1)
#define HS_KEYBLIND_NONCE_LEN \
  (HS_KEYBLIND_NONCE_PREFIX_LEN + sizeof(uint64_t) + sizeof(uint64_t))

/* Node hidden service stored at index prefix value. */
#define HS_INDEX_PREFIX "store-at-idx"
#define HS_INDEX_PREFIX_LEN (sizeof(HS_INDEX_PREFIX) - 1)

/* Node hidden service directory index prefix value. */
#define HSDIR_INDEX_PREFIX "node-idx"
#define HSDIR_INDEX_PREFIX_LEN (sizeof(HSDIR_INDEX_PREFIX) - 1)

/* Prefix of the shared random value disaster mode. */
#define HS_SRV_DISASTER_PREFIX "shared-random-disaster"
#define HS_SRV_DISASTER_PREFIX_LEN (sizeof(HS_SRV_DISASTER_PREFIX) - 1)

/* Default value of number of hsdir replicas (hsdir_n_replicas). */
#define HS_DEFAULT_HSDIR_N_REPLICAS 2
/* Default value of hsdir spread store (hsdir_spread_store). */
#define HS_DEFAULT_HSDIR_SPREAD_STORE 3
/* Default value of hsdir spread fetch (hsdir_spread_fetch). */
#define HS_DEFAULT_HSDIR_SPREAD_FETCH 3

/* Type of authentication key used by an introduction point. */
typedef enum {
  HS_AUTH_KEY_TYPE_LEGACY  = 1,
  HS_AUTH_KEY_TYPE_ED25519 = 2,
} hs_auth_key_type_t;

/* Client and service side connection identifier used on a directory and edge
 * connection to identify which service is being queried. This is attached to
 * an edge_connection_t and dir_connection_t. */
typedef struct hs_conn_identifier_t {
  /* The public key used to uniquely identify the service. */
  ed25519_public_key_t identity_pk;

  /* XXX: Client authorization type. */
} hs_conn_identifier_t;

/* Represents the mapping from a virtual port of a rendezvous service to a
 * real port on some IP. */
typedef struct rend_service_port_config_t {
  /* The incoming HS virtual port we're mapping */
  uint16_t virtual_port;
  /* Is this an AF_UNIX port? */
  unsigned int is_unix_addr:1;
  /* The outgoing TCP port to use, if !is_unix_addr */
  uint16_t real_port;
  /* The outgoing IPv4 or IPv6 address to use, if !is_unix_addr */
  tor_addr_t real_addr;
  /* The socket path to connect to, if is_unix_addr */
  char unix_addr[FLEXIBLE_ARRAY_MEMBER];
} rend_service_port_config_t;

/* Hidden service directory index used in a node_t which is set once we set
 * the consensus. */
typedef struct hsdir_index_t {
  /* The hsdir index for the current time period. */
  uint8_t current[DIGEST256_LEN];
  /* The hsdir index for the next time period. */
  uint8_t next[DIGEST256_LEN];
} hsdir_index_t;

void hs_init(void);
void hs_free_all(void);

int hs_check_service_private_dir(const char *username, const char *path,
                                 unsigned int dir_group_readable,
                                 unsigned int create);
char *hs_path_from_filename(const char *directory, const char *filename);

void hs_build_address(const ed25519_public_key_t *key,
                              uint8_t version, char *addr_out);
int hs_address_is_valid(const char *address);

void hs_build_blinded_pubkey(const ed25519_public_key_t *pubkey,
                             const uint8_t *secret, size_t secret_len,
                             uint64_t time_period_num,
                             ed25519_public_key_t *pubkey_out);
void hs_build_blinded_keypair(const ed25519_keypair_t *kp,
                              const uint8_t *secret, size_t secret_len,
                              uint64_t time_period_num,
                              ed25519_keypair_t *kp_out);
int hs_service_requires_uptime_circ(const smartlist_t *ports);

void rend_data_free(rend_data_t *data);
rend_data_t *rend_data_dup(const rend_data_t *data);
rend_data_t *rend_data_client_create(const char *onion_address,
                                     const char *desc_id,
                                     const char *cookie,
                                     rend_auth_type_t auth_type);
rend_data_t *rend_data_service_create(const char *onion_address,
                                      const char *pk_digest,
                                      const uint8_t *cookie,
                                      rend_auth_type_t auth_type);
const char *rend_data_get_address(const rend_data_t *rend_data);
const char *rend_data_get_desc_id(const rend_data_t *rend_data,
                                  uint8_t replica, size_t *len_out);
const uint8_t *rend_data_get_pk_digest(const rend_data_t *rend_data,
                                       size_t *len_out);

uint64_t hs_get_time_period_num(time_t now);
uint64_t hs_get_next_time_period_num(time_t now);

int hs_overlap_mode_is_active(const networkstatus_t *consensus, time_t now);

uint8_t *hs_get_current_srv(uint64_t time_period_num);
uint8_t *hs_get_previous_srv(uint64_t time_period_num);

void hs_build_hsdir_index(const ed25519_public_key_t *identity_pk,
                          const uint8_t *srv, uint64_t period_num,
                          uint8_t *hsdir_index_out);
void hs_build_hs_index(uint64_t replica,
                       const ed25519_public_key_t *blinded_pk,
                       uint64_t period_num, uint8_t *hs_index_out);

int32_t hs_get_hsdir_n_replicas(void);
int32_t hs_get_hsdir_spread_fetch(void);
int32_t hs_get_hsdir_spread_store(void);

void hs_get_responsible_hsdirs(const ed25519_public_key_t *blinded_pk,
                               uint64_t time_period_num, int is_next_period,
                               int is_client, smartlist_t *responsible_dirs);

#ifdef HS_COMMON_PRIVATE

#ifdef TOR_UNIT_TESTS

STATIC uint64_t get_time_period_length(void);

#endif /* TOR_UNIT_TESTS */

#endif /* HS_COMMON_PRIVATE */

#endif /* TOR_HS_COMMON_H */

