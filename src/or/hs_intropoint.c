/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_intropoint.c
 * \brief Implement next generation introductions point functionality
 **/

#define HS_INTROPOINT_PRIVATE

#include "or.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "relay.h"
#include "rendmid.h"
#include "rephist.h"

#include "hs/cell_establish_intro.h"
#include "hs_circuitmap.h"
#include "hs_intropoint.h"
#include "hs_common.h"

/** Extract the authentication key from an ESTABLISH_INTRO <b>cell</b> and
 *  place it in <b>auth_key_out</b>. */
STATIC void
get_auth_key_from_establish_intro_cell(ed25519_public_key_t *auth_key_out,
                                       const hs_cell_establish_intro_t *cell)
{
  tor_assert(auth_key_out);

  const uint8_t *key_array =
    hs_cell_establish_intro_getconstarray_auth_key(cell);
  tor_assert(key_array);

  memcpy(auth_key_out->pubkey, key_array, cell->auth_key_len);
}

/** We received an ESTABLISH_INTRO <b>cell</b>. Verify its signature and MAC,
 *  given <b>circuit_key_material</b>. */
STATIC int
verify_establish_intro_cell(const hs_cell_establish_intro_t *cell,
                            const char *circuit_key_material,
                            size_t circuit_key_material_len)
{
  /* We only reach this function if the first byte of the cell is 0x02 which
     means that auth_key_type is AUTH_KEY_ED25519, hence this assert should
     always pass. See hs_intro_received_establish_intro().  */
  tor_assert(cell->auth_key_type == AUTH_KEY_ED25519);

  const char *msg = (char*) cell->start_cell;

  /* Verify the sig */
  {
    ed25519_signature_t sig_struct;
    const uint8_t *sig_array = hs_cell_establish_intro_getconstarray_sig(cell);
    memcpy(sig_struct.sig, sig_array, cell->sig_len);

    ed25519_public_key_t auth_key;
    get_auth_key_from_establish_intro_cell(&auth_key, cell);

    const size_t sig_msg_len = (char*) (cell->end_sig_fields) - msg;
    int sig_mismatch = ed25519_checksig_prefixed(&sig_struct,
                                                 (uint8_t*) msg, sig_msg_len,
                                                 ESTABLISH_INTRO_SIG_PREFIX,
                                                 &auth_key);
    if (sig_mismatch) {
      log_warn(LD_PROTOCOL, "ESTABLISH_INTRO signature not as expected");
      return -1;
    }
  }

  /* Verify the MAC */
  {
    const size_t auth_msg_len = (char*) (cell->end_mac_fields) - msg;
    char mac[DIGEST256_LEN];
    int mac_errors = crypto_hmac_sha3_256(mac,
                                          circuit_key_material,
                                          circuit_key_material_len,
                                          msg, auth_msg_len);
    if (mac_errors != 0) {
      log_warn(LD_BUG, "Error computing ESTABLISH_INTRO handshake_auth");
      return -1;
    }
    if (tor_memneq(mac, cell->handshake_mac, sizeof(mac))) {
      log_warn(LD_PROTOCOL, "ESTABLISH_INTRO handshake_auth not as expected");
      return -1;
    }
  }

  return 0;
}

/* Send an INTRO_ESTABLISHED cell to <b>circ</b>. */
MOCK_IMPL(int,
hs_intro_send_intro_established_cell,(or_circuit_t *circ))
{
  return relay_send_command_from_edge(0, TO_CIRCUIT(circ),
                                      RELAY_COMMAND_INTRO_ESTABLISHED,
                                      "", 0, NULL);
}

/** We received an ESTABLISH_INTRO <b>parsed_cell</b> on <b>circ</b>. It's
 *  well-formed and passed our verifications. Perform appropriate actions to
 *  establish an intro point. */
static int
handle_verified_establish_intro_cell(or_circuit_t *circ,
                               const hs_cell_establish_intro_t *parsed_cell)
{
  /* Get the auth key of this intro point */
  ed25519_public_key_t auth_key;
  get_auth_key_from_establish_intro_cell(&auth_key, parsed_cell);

  /* Close any other intro point circs with the same auth key */
  or_circuit_t *other_circ = NULL;
  while ((other_circ = hs_circuitmap_get_intro_circ_v3(&auth_key))) {
    circuit_mark_for_close(TO_CIRCUIT(other_circ), END_CIRC_REASON_FINISHED);
  }

  /* Then notify the hidden service that the intro point is established by
     sending an INTRO_ESTABLISHED cell */
  if (hs_intro_send_intro_established_cell(circ)) {
    log_warn(LD_BUG, "Couldn't send INTRO_ESTABLISHED cell.");
    return -1;
  }

  /* Associate intro point auth key with this circuit. */
  hs_circuitmap_register_intro_circ_v3(circ, &auth_key);
  /* Repurpose this circuit into an intro circuit. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_INTRO_POINT);

  return 0;
}

/** We just received an ESTABLISH_INTRO cell in <b>circ</b> with payload in
 *  <b>request</b>. Handle it by making <b>circ</b> an intro circuit. Return 0
 *  if everything went well, or -1 if there were errors. */
static int
handle_establish_intro(or_circuit_t *circ, const uint8_t *request,
                       size_t request_len)
{
  int cell_ok, retval = -1;
  hs_cell_establish_intro_t *parsed_cell = NULL;

  tor_assert(circ);
  tor_assert(request);

  log_info(LD_REND, "Received an ESTABLISH_INTRO request on circuit %" PRIu32,
           circ->p_circ_id);

  /* Check that the circuit is in shape to become an intro point */
  if (!hs_intro_circuit_is_suitable(circ)) {
    goto err;
  }

  /* Parse the cell */
  ssize_t parsing_result = hs_cell_establish_intro_parse(&parsed_cell,
                                                         request, request_len);
  if (parsing_result < 0) {
    log_warn(LD_PROTOCOL, "Rejecting %s ESTABLISH_INTRO cell.",
             parsing_result == -1 ? "invalid" : "truncated");
    goto err;
  }

  cell_ok = verify_establish_intro_cell(parsed_cell,
                                       circ->rend_circ_nonce,
                                       sizeof(circ->rend_circ_nonce));
  if (cell_ok < 0) {
    log_warn(LD_PROTOCOL, "Failed to verify ESTABLISH_INTRO cell.");
    goto err;
  }

  /* This cell is legit. Take the appropriate actions. */
  cell_ok = handle_verified_establish_intro_cell(circ, parsed_cell);
  if (cell_ok < 0) {
    goto err;
  }

  log_warn(LD_GENERAL, "Established prop224 intro point on circuit %" PRIu32,
           circ->p_circ_id);

  /* We are done! */
  retval = 0;
  goto done;

 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);

 done:
  hs_cell_establish_intro_free(parsed_cell);
  return retval;
}


/* Return True if circuit is suitable for becoming an intro circuit. */
int
hs_intro_circuit_is_suitable(const or_circuit_t *circ)
{
  /* Basic circuit state sanity checks. */
  if (circ->base_.purpose != CIRCUIT_PURPOSE_OR) {
    log_warn(LD_PROTOCOL, "Rejecting ESTABLISH_INTRO on non-OR circuit.");
    return 0;
  }

  if (circ->base_.n_chan) {
    log_warn(LD_PROTOCOL, "Rejecting ESTABLISH_INTRO on non-edge circuit.");
    return 0;
  }

  return 1;
}

/* We just received an ESTABLISH_INTRO cell in <b>circ</b>. Figure out of it's
 * a legacy or a next gen cell, and pass it to the appropriate handler. */
int
hs_intro_received_establish_intro(or_circuit_t *circ, const uint8_t *request,
                            size_t request_len)
{
  tor_assert(circ);
  tor_assert(request);

  if (request_len == 0) {
    log_warn(LD_PROTOCOL, "Empty ESTABLISH_INTRO cell.");
    goto err;
  }

  /* Using the first byte of the cell, figure out the version of
   * ESTABLISH_INTRO and pass it to the appropriate cell handler */
  const uint8_t first_byte = request[0];
  switch (first_byte) {
    case HS_INTRO_AUTH_KEY_TYPE_LEGACY0:
    case HS_INTRO_AUTH_KEY_TYPE_LEGACY1:
      return rend_mid_establish_intro_legacy(circ, request, request_len);
    case HS_INTRO_AUTH_KEY_TYPE_ED25519:
      return handle_establish_intro(circ, request, request_len);
    default:
      log_warn(LD_PROTOCOL, "Invalid AUTH_KEY_TYPE %u.", first_byte);
      goto err;
  }

 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}
