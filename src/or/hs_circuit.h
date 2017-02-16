/* Copyright (c) 2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_circuit.h
 * \brief Header file containing circuit data for the whole HS subsytem.
 **/

#ifndef TOR_HS_CIRCUIT_H
#define TOR_HS_CIRCUIT_H

#include "or.h"
#include "crypto.h"
#include "crypto_ed25519.h"

#include "hs_common.h"
#include "hs_service.h"

/* Client and service side circuit identifier that is used for hidden
 * service connection establishment. Not all fields contain data depending
 * on the circuit purpose. This is attached to a origin_circuit_t. */
typedef struct hs_circ_identifier_t {
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
  uint8_t rendezvous_cookie[REND_COOKIE_LEN];

  /* (Only rendezvous circuit) Number of streams associated with this
   * rendezvous circuit. We track this because there is a check on a maximum
   * value. */
  uint64_t num_rdv_streams;
} hs_circ_identifier_t;

/* Identifier API. */
void hs_circ_identifier_free(hs_circ_identifier_t *ident);

/* Circuit API. */
int hs_circ_launch_intro_point(hs_service_t *service,
                               const hs_service_intro_point_t *ip,
                               extend_info_t *ei, time_t now);

#endif /* TOR_HS_CIRCUIT_H */

