/* Copyright (c) 2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_ident.h
 * \brief Header file containing circuit and connection identifier data for
 * the whole HS subsytem.
 *
 **/

#ifndef TOR_HS_IDENT_H
#define TOR_HS_IDENT_H

#include "crypto.h"
#include "crypto_ed25519.h"

#include "hs_common.h"

/* Length of the rendezvous cookie that is used to connect circuits at the
 * rendezvous point. */
#define HS_REND_COOKIE_LEN DIGEST_LEN

/* Client and service side circuit identifier that is used for hidden service
 * circuit establishment. Not all fields contain data, it depends on the
 * circuit purpose. This is attached to an origin_circuit_t. */
typedef struct hs_ident_circuit_t {
  /* (All circuit) The public key used to uniquely identify the service. */
  ed25519_public_key_t identity_pk;

  /* (Only intro point circuit) Which type of authentication key this
   * circuit identifier is using. */
  hs_auth_key_type_t auth_key_type;

  /* (Only intro point circuit) Introduction point authentication key. In
   * legacy mode, we use an RSA key else an ed25519 public key. */
  union {
    /* v2 specific which happens to be the encryption key as well. */
    crypto_pk_t *legacy;
    /* v3 specific */
    ed25519_public_key_t ed25519_pk;
  } intro_key;

  /* (Only rendezvous circuit) Rendezvous cookie sent from the client to the
   * service with an INTRODUCE1 cell and used by the service in an
   * RENDEZVOUS1 cell. */
  uint8_t rendezvous_cookie[HS_REND_COOKIE_LEN];

  /* (Only rendezvous circuit) The HANDSHAKE_INFO needed in the RENDEZVOUS1
   * cell of the service. The construction is as follows:
   *    SERVER_PK   [32 bytes]
   *    AUTH_MAC    [32 bytes]
   */
  uint8_t rendezvous_handshake_info[CURVE25519_PUBKEY_LEN + DIGEST256_LEN];

  /* (Only rendezvous circuit) The NTOR_KEY_SEED needed for key derivation for
   * the e2e encryption with the client on the circuit. */
  uint8_t rendezvous_ntor_key_seed[DIGEST256_LEN];

  /* (Only rendezvous circuit) Number of streams associated with this
   * rendezvous circuit. We track this because there is a check on a maximum
   * value. */
  uint64_t num_rdv_streams;
} hs_ident_circuit_t;

/* Client and service side directory connection identifier used for a
 * directory connection to identify which service is being queried. This is
 * attached to a dir_connection_t. */
typedef struct hs_ident_dir_conn_t {
  /* The public key used to uniquely identify the service. */
  ed25519_public_key_t identity_pk;

  /* XXX: Client authorization. */
} hs_ident_dir_conn_t;

/* Client and service side edge connection identifier used for an edge
 * connection to identify which service is being queried. This is attached to
 * a edge_connection_t. */
typedef struct hs_ident_edge_conn_t {
  /* The public key used to uniquely identify the service. */
  ed25519_public_key_t identity_pk;

  /* XXX: Client authorization. */
} hs_ident_edge_conn_t;

/* Circuit identifier API. */
hs_ident_circuit_t *hs_ident_circuit_new(
                             const ed25519_public_key_t *identity_pk);
void hs_ident_circuit_free(hs_ident_circuit_t *ident);
hs_ident_circuit_t *hs_ident_circuit_dup(const hs_ident_circuit_t *src);

/* Directory connection identifier API. */
hs_ident_dir_conn_t *hs_ident_dir_conn_dup(const hs_ident_dir_conn_t *src);
void hs_ident_dir_conn_free(hs_ident_dir_conn_t *ident);

/* Edge connection identifier API. */
hs_ident_edge_conn_t *hs_ident_edge_conn_new(
                                    const ed25519_public_key_t *identity_pk);
void hs_ident_edge_conn_free(hs_ident_edge_conn_t *ident);

#endif /* TOR_HS_IDENT_H */

