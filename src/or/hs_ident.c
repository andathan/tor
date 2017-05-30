/* Copyright (c) 2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_ident.c
 * \brief Contains circuit and connection identifier code for the whole HS
 *        subsytem.
 **/

#include "hs_ident.h"

/* Return a newly allocated circuit identifier. The given public key is copied
 * identity_pk into the identifier. */
hs_ident_circuit_t *
hs_ident_circuit_new(const ed25519_public_key_t *identity_pk)
{
  hs_ident_circuit_t *ident = tor_malloc_zero(sizeof(*ident));
  ed25519_pubkey_copy(&ident->identity_pk, identity_pk);
  return ident;
}

/* Free the given circuit identifier. */
void
hs_ident_circuit_free(hs_ident_circuit_t *ident)
{
  if (ident == NULL) {
    return;
  }
  if (ident->auth_key_type == HS_AUTH_KEY_TYPE_LEGACY) {
    crypto_pk_free(ident->intro_key.legacy);
  }
  tor_free(ident);
}

/* For a given circuit identifier src, return a newly allocated copy of it.
 * This can't fail. */
hs_ident_circuit_t *
hs_ident_circuit_dup(const hs_ident_circuit_t *src)
{
  hs_ident_circuit_t *ident = tor_malloc_zero(sizeof(*ident));
  memcpy(ident, src, sizeof(*ident));
  if (ident->auth_key_type == HS_AUTH_KEY_TYPE_LEGACY) {
    ident->intro_key.legacy = crypto_pk_dup_key(src->intro_key.legacy);
  }
  return ident;
}

/* For a given directory connection identifier src, return a newly allocated
 * copy of it. This can't fail. */
hs_ident_dir_conn_t *
hs_ident_dir_conn_dup(const hs_ident_dir_conn_t *src)
{
  hs_ident_dir_conn_t *ident = tor_malloc_zero(sizeof(*ident));
  memcpy(ident, src, sizeof(*ident));
  return ident;
}

/* Free the given directory connection identifier. */
void
hs_ident_dir_conn_free(hs_ident_dir_conn_t *ident)
{
  if (ident == NULL) {
    return;
  }
  tor_free(ident);
}

/* Return a newly allocated edge connection identifier. The given public key
 * identity_pk is copied into the identifier. */
hs_ident_edge_conn_t *
hs_ident_edge_conn_new(const ed25519_public_key_t *identity_pk)
{
  hs_ident_edge_conn_t *ident = tor_malloc_zero(sizeof(*ident));
  ed25519_pubkey_copy(&ident->identity_pk, identity_pk);
  return ident;
}

/* Free the given edge connection identifier. */
void
hs_ident_edge_conn_free(hs_ident_edge_conn_t *ident)
{
  if (ident == NULL) {
    return;
  }
  tor_free(ident);
}

