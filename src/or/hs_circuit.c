/* Copyright (c) 2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_circuit.c
 **/

#include "or.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "config.h"
#include "relay.h"
#include "rephist.h"
#include "router.h"

#include "hs_cell.h"
#include "hs_circuit.h"
#include "hs_ident.h"
#include "hs_ntor.h"
#include "hs_service.h"

/* Trunnel. */
#include "hs/cell_common.h"
#include "hs/cell_establish_intro.h"

/* A circuit is about to become an e2e rendezvous circuit. Check
 * <b>circ_purpose</b> and ensure that it's properly set. Return true iff
 * circuit purpose is properly set, otherwise return false. */
static int
circuit_purpose_is_correct_for_rend(unsigned int circ_purpose,
                                    int is_service_side)
{
  if (is_service_side) {
    if (circ_purpose != CIRCUIT_PURPOSE_S_CONNECT_REND) {
      log_warn(LD_BUG,
            "HS e2e circuit setup with wrong purpose (%d)", circ_purpose);
      return 0;
    }
  }

  if (!is_service_side) {
    if (circ_purpose != CIRCUIT_PURPOSE_C_REND_READY &&
        circ_purpose != CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED) {
      log_warn(LD_BUG,
            "Client e2e circuit setup with wrong purpose (%d)", circ_purpose);
      return 0;
    }
  }

  return 1;
}

/* Create and return a crypt path for the final hop of a v3 prop224 rendezvous
 * circuit. Initialize the crypt path crypto using the output material from the
 * ntor key exchange at <b>ntor_key_seed</b>.
 *
 * If <b>is_service_side</b> is set, we are the hidden service and the final
 * hop of the rendezvous circuit is the client on the other side. */
static crypt_path_t *
create_rend_cpath(const uint8_t *ntor_key_seed, size_t seed_len,
                  int is_service_side)
{
  uint8_t keys[HS_NTOR_KEY_EXPANSION_KDF_OUT_LEN];
  crypt_path_t *cpath = NULL;

  /* Do the key expansion */
  if (hs_ntor_circuit_key_expansion(ntor_key_seed, seed_len,
                                    keys, sizeof(keys)) < 0) {
    goto err;
  }

  /* Setup the cpath */
  cpath = tor_malloc_zero(sizeof(crypt_path_t));
  cpath->magic = CRYPT_PATH_MAGIC;

  if (circuit_init_cpath_crypto(cpath, (char*)keys, sizeof(keys),
                                is_service_side, 1) < 0) {
    tor_free(cpath);
    goto err;
  }

 err:
  memwipe(keys, 0, sizeof(keys));
  return cpath;
}

/* We are a v2 legacy HS client: Create and return a crypt path for the hidden
 * service on the other side of the rendezvous circuit <b>circ</b>. Initialize
 * the crypt path crypto using the body of the RENDEZVOUS1 cell at
 * <b>rend_cell_body</b> (which must be at least DH_KEY_LEN+DIGEST_LEN bytes).
 */
static crypt_path_t *
create_rend_cpath_legacy(origin_circuit_t *circ, const uint8_t *rend_cell_body)
{
  crypt_path_t *hop = NULL;
  char keys[DIGEST_LEN+CPATH_KEY_MATERIAL_LEN];

  /* first DH_KEY_LEN bytes are g^y from the service. Finish the dh
   * handshake...*/
  tor_assert(circ->build_state);
  tor_assert(circ->build_state->pending_final_cpath);
  hop = circ->build_state->pending_final_cpath;

  tor_assert(hop->rend_dh_handshake_state);
  if (crypto_dh_compute_secret(LOG_PROTOCOL_WARN, hop->rend_dh_handshake_state,
                               (char*)rend_cell_body, DH_KEY_LEN,
                               keys, DIGEST_LEN+CPATH_KEY_MATERIAL_LEN)<0) {
    log_warn(LD_GENERAL, "Couldn't complete DH handshake.");
    goto err;
  }
  /* ... and set up cpath. */
  if (circuit_init_cpath_crypto(hop,
                                keys+DIGEST_LEN, sizeof(keys)-DIGEST_LEN,
                                0, 0) < 0)
    goto err;

  /* Check whether the digest is right... */
  if (tor_memneq(keys, rend_cell_body+DH_KEY_LEN, DIGEST_LEN)) {
    log_warn(LD_PROTOCOL, "Incorrect digest of key material.");
    goto err;
  }

  /* clean up the crypto stuff we just made */
  crypto_dh_free(hop->rend_dh_handshake_state);
  hop->rend_dh_handshake_state = NULL;

  goto done;

 err:
  hop = NULL;

 done:
  memwipe(keys, 0, sizeof(keys));
  return hop;
}

/* Append the final <b>hop</b> to the cpath of the rend <b>circ</b>, and mark
 * <b>circ</b> ready for use to transfer HS relay cells. */
static void
finalize_rend_circuit(origin_circuit_t *circ, crypt_path_t *hop,
                      int is_service_side)
{
  tor_assert(circ);
  tor_assert(hop);

  /* Notify the circuit state machine that we are splicing this circuit */
  int new_circ_purpose = is_service_side ?
    CIRCUIT_PURPOSE_S_REND_JOINED : CIRCUIT_PURPOSE_C_REND_JOINED;
  circuit_change_purpose(TO_CIRCUIT(circ), new_circ_purpose);

  /* All is well. Extend the circuit. */
  hop->state = CPATH_STATE_OPEN;
  /* Set the windows to default. */
  hop->package_window = circuit_initial_package_window();
  hop->deliver_window = CIRCWINDOW_START;

  /* Now that this circuit has finished connecting to its destination,
   * make sure circuit_get_open_circ_or_launch is willing to return it
   * so we can actually use it. */
  circ->hs_circ_has_timed_out = 0;

  /* Append the hop to the cpath of this circuit */
  onion_append_to_cpath(&circ->cpath, hop);

  /* In legacy code, 'pending_final_cpath' points to the final hop we just
   * appended to the cpath. We set the original pointer to NULL so that we
   * don't double free it. */
  if (circ->build_state) {
    circ->build_state->pending_final_cpath = NULL;
  }

  /* Finally, mark circuit as ready to be used for client streams */
  if (!is_service_side) {
    circuit_try_attaching_streams(circ);
  }
}

/* For a given circuit and a service introduction point object, register the
 * intro circuit to the circuitmap. This supports legacy intro point. */
static void
register_intro_circ(const hs_service_intro_point_t *ip,
                    origin_circuit_t *circ)
{
  tor_assert(ip);
  tor_assert(circ);

  if (ip->base.is_only_legacy) {
    uint8_t digest[DIGEST_LEN];
    if (BUG(crypto_pk_get_digest(ip->legacy_key, (char *) digest) < 0)) {
      return;
    }
    hs_circuitmap_register_intro_circ_v2_service_side(circ, digest);
  } else {
    hs_circuitmap_register_intro_circ_v3_service_side(circ,
                                         &ip->auth_key_kp.pubkey);
  }
}

/* Return the number of opened introduction circuit for the given circuit that
 * is matching its identity key. */
static unsigned int
count_opened_desc_intro_point_circuits(const hs_service_t *service,
                                       const hs_service_descriptor_t *desc)
{
  unsigned int count = 0;

  tor_assert(service);
  tor_assert(desc);

  DIGEST256MAP_FOREACH(desc->intro_points.map, key,
                       const hs_service_intro_point_t *, ip) {
    circuit_t *circ;
    origin_circuit_t *ocirc;
    if (ip->base.is_only_legacy) {
      uint8_t digest[DIGEST_LEN];
      if (BUG(crypto_pk_get_digest(ip->legacy_key, (char *) digest) < 0)) {
        continue;
      }
      ocirc = hs_circuitmap_get_intro_circ_v2_service_side(digest);
    } else {
      ocirc =
        hs_circuitmap_get_intro_circ_v3_service_side(&ip->auth_key_kp.pubkey);
    }
    if (ocirc == NULL) {
      continue;
    }
    circ = TO_CIRCUIT(ocirc);
    tor_assert(circ->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
               circ->purpose == CIRCUIT_PURPOSE_S_INTRO);
    /* Having a circuit not for the requested service is really bad. */
    tor_assert(ed25519_pubkey_eq(&service->keys.identity_pk,
                                 &ocirc->hs_ident->identity_pk));
    /* Only count opened circuit and skip circuit that will be closed. */
    if (!circ->marked_for_close && circ->state == CIRCUIT_STATE_OPEN) {
      count++;
    }
  } DIGEST256MAP_FOREACH_END;
  return count;
}

/* From a given service and service intro point, create an introduction point
 * circuit identifier. This can't fail. */
static hs_ident_circuit_t *
create_intro_circuit_identifier(const hs_service_t *service,
                                const hs_service_intro_point_t *ip)
{
  hs_ident_circuit_t *ident;

  tor_assert(service);
  tor_assert(ip);

  ident = hs_ident_circuit_new(&service->keys.identity_pk,
                               HS_IDENT_CIRCUIT_INTRO);
  if (ip->base.is_only_legacy) {
    ident->auth_key_type = HS_AUTH_KEY_TYPE_LEGACY;
    ident->auth_rsa_pk = crypto_pk_copy_full(ip->legacy_key);
  } else {
    ident->auth_key_type = HS_AUTH_KEY_TYPE_ED25519;
    ed25519_pubkey_copy(&ident->auth_ed25519_pk,
                        &ip->auth_key_kp.pubkey);
  }

  return ident;
}

/* For a given introduction point and an introduction circuit, send the
 * ESTABLISH_INTRO cell. The service object is used for logging. This can fail
 * and if so, the circuit is closed and the intro point object is flagged
 * that the circuit is not established anymore which is important for the
 * retry mechanism. */
static void
send_establish_intro(const hs_service_t *service,
                     hs_service_intro_point_t *ip, origin_circuit_t *circ)
{
  ssize_t cell_len;
  uint8_t payload[RELAY_PAYLOAD_SIZE];

  tor_assert(service);
  tor_assert(ip);
  tor_assert(circ);

  /* Encode establish intro cell. */
  cell_len = hs_cell_build_establish_intro(circ->cpath->prev->rend_circ_nonce,
                                           ip, payload);
  if (cell_len < 0) {
    log_warn(LD_REND, "Unable to encode ESTABLISH_INTRO cell for service %s "
                      "on circuit %u. Closing circuit.",
             safe_str_client(service->onion_address),
             TO_CIRCUIT(circ)->n_circ_id);
    goto err;
  }

  /* Send the cell on the circuit. */
  if (relay_send_command_from_edge(CONTROL_CELL_ID, TO_CIRCUIT(circ),
                                   RELAY_COMMAND_ESTABLISH_INTRO,
                                   (char *) payload, cell_len,
                                   circ->cpath->prev) < 0) {
    log_info(LD_REND, "Unable to send ESTABLISH_INTRO cell for service %s "
                      "on circuit %u.",
             safe_str_client(service->onion_address),
             TO_CIRCUIT(circ)->n_circ_id);
    /* On error, the circuit has been closed. */
    goto done;
  }

  /* Record the attempt to use this circuit. */
  pathbias_count_use_attempt(circ);
  goto done;

 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
 done:
  memwipe(payload, 0, sizeof(payload));
}

/* ========== */
/* Public API */
/* ========== */

int
hs_circ_launch_rendezvous_point(const hs_service_t *service,
                                const curve25519_public_key_t *onion_key,
                                const uint8_t *rendezvous_cookie)
{
  tor_assert(service);
  tor_assert(onion_key);
  tor_assert(rendezvous_cookie);
  /* XXX: Implement rendezvous launch support. */
  return 0;
}

/* For a given service and a service intro point, launch a circuit to the
 * extend info ei. If the service is a single onion, a one-hop circuit will be
 * requested. Return 0 if the circuit was successfully launched and tagged
 * with the correct identifier. On error, a negative value is returned. */
int
hs_circ_launch_intro_point(hs_service_t *service,
                           const hs_service_intro_point_t *ip,
                           extend_info_t *ei, time_t now)
{
  /* Standard flags for introduction circuit. */
  int ret = -1, circ_flags = CIRCLAUNCH_NEED_UPTIME | CIRCLAUNCH_IS_INTERNAL;
  origin_circuit_t *circ;

  tor_assert(service);
  tor_assert(ip);
  tor_assert(ei);

  /* Update circuit flags in case of a single onion service that requires a
   * direct connection. */
  if (service->config.is_single_onion) {
    circ_flags |= CIRCLAUNCH_ONEHOP_TUNNEL;
  }

  log_info(LD_REND, "Launching a circuit to intro point %s for service %s.",
           safe_str_client(extend_info_describe(ei)),
           safe_str_client(service->onion_address));

  /* Note down that we are about to use an internal circuit. */
  rep_hist_note_used_internal(now, circ_flags & CIRCLAUNCH_NEED_UPTIME,
                              circ_flags & CIRCLAUNCH_NEED_CAPACITY);

  /* Note down the launch for the retry period. Even if the circuit fails to
   * be launched, we still want to respect the retry period to avoid stress on
   * the circuit subsystem. */
  service->state.num_intro_circ_launched++;
  circ = circuit_launch_by_extend_info(CIRCUIT_PURPOSE_S_ESTABLISH_INTRO,
                                       ei, circ_flags);
  if (circ == NULL) {
    goto end;
  }

  /* Setup the circuit identifier and attach it to it. */
  circ->hs_ident = create_intro_circuit_identifier(service, ip);
  tor_assert(circ->hs_ident);
  /* Register circuit in the global circuitmap. */
  register_intro_circ(ip, circ);

  /* Success. */
  ret = 0;
 end:
  return ret;
}

/* Called when a service introduction point circuit is done building. Given
 * the service and intro point object, this function will send the
 * ESTABLISH_INTRO cell on the circuit. Return 0 on success. Return 1 if the
 * circuit has been repurposed to General because we already have too many
 * opened. */
int
hs_circ_service_intro_has_opened(hs_service_t *service,
                                 hs_service_intro_point_t *ip,
                                 const hs_service_descriptor_t *desc,
                                 origin_circuit_t *circ)
{
  int ret = 0;
  unsigned int num_intro_circ, num_needed_circ;

  tor_assert(service);
  tor_assert(ip);
  tor_assert(desc);
  tor_assert(circ);

  num_intro_circ = count_opened_desc_intro_point_circuits(service, desc);
  num_needed_circ = service->config.num_intro_points;
  if (num_intro_circ > num_needed_circ) {
    /* There are too many opened valid intro circuit for what the service
     * needs so repurpose this one. */

    /* XXX: Legacy code checks options->ExcludeNodes and if not NULL it just
     * closes the circuit. I have NO idea why it does that so it hasn't been
     * added here. I can only assume in case our ExcludeNodes list changes but
     * in that case, all circuit are flagged unusable (config.c). --dgoulet */

    log_info(LD_CIRC | LD_REND, "Introduction circuit just opened but we "
                                "have enough for service %s. Repurposing "
                                "it to general and leaving internal.",
             safe_str_client(service->onion_address));
    tor_assert(circ->build_state->is_internal);
    /* Remove it from the circuitmap. */
    hs_circuitmap_remove_circuit(TO_CIRCUIT(circ));
    /* Cleaning up the hidden service identifier and repurpose. */
    tor_free(circ->hs_ident);
    circ->hs_ident = NULL;
    circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_GENERAL);
    /* Inform that this circuit just opened for this new purpose. */
    circuit_has_opened(circ);
    /* This return value indicate to the caller that the IP object should be
     * removed from the service because it's corresponding circuit has just
     * been repurposed. */
    ret = 1;
    goto done;
  }

  log_info(LD_REND, "Introduction circuit %u established for service %s.",
           TO_CIRCUIT(circ)->n_circ_id,
           safe_str_client(service->onion_address));
  circuit_log_path(LOG_INFO, LD_REND, circ);

  /* Time to send an ESTABLISH_INTRO cell on this circuit. On error, this call
   * makes sure the circuit gets closed. */
  send_establish_intro(service, ip, circ);

 done:
  return ret;
}

/* Handle an INTRO_ESTABLISHED cell payload of length payload_len arriving on
 * the given introduction circuit circ. The service is only used for logging
 * purposes. Return 0 on success else a negative value. */
int
hs_circ_handle_intro_established(const hs_service_t *service,
                                 const hs_service_intro_point_t *ip,
                                 origin_circuit_t *circ,
                                 const uint8_t *payload, size_t payload_len)
{
  int ret = -1;

  tor_assert(service);
  tor_assert(ip);
  tor_assert(circ);
  tor_assert(payload);

  /* Try to parse the payload into a cell making sure we do actually have a
   * valid cell. For a legacy node, it's an empty payload so as long as we
   * have the cell, we are good. */
  if (!ip->base.is_only_legacy &&
      hs_cell_parse_intro_established(payload, payload_len) < 0) {
    log_warn(LD_REND, "Unable to parse the INTRO_ESTABLISHED cell on "
                      "circuit %u for service %s",
             TO_CIRCUIT(circ)->n_circ_id,
             safe_str_client(service->onion_address));
    goto done;
  }

  /* Switch the purpose to a fully working intro point. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_S_INTRO);
  /* Getting a valid INTRODUCE_ESTABLISHED means we've successfully used the
   * circuit so update our pathbias subsystem. */
  pathbias_mark_use_success(circ);
  /* Success. */
  ret = 0;

 done:
  return ret;
}

/* Handle an INTRODUCE2 unparsed payload of payload_len for the given circuit
 * and service. This cell is associated with the intro point object ip and the
 * subcredential. Return 0 on success else a negative value. */
int
hs_circ_handle_introduce2(const hs_service_t *service,
                          const origin_circuit_t *circ,
                          hs_service_intro_point_t *ip,
                          const uint8_t *subcredential,
                          const uint8_t *payload, size_t payload_len)
{
  int ret = -1;
  hs_cell_introduce2_data_t data =  {0};

  tor_assert(service);
  tor_assert(circ);
  tor_assert(ip);
  tor_assert(subcredential);
  tor_assert(payload);

  /* Populate the data structure with everything we need for the cell to be
   * parsed, decrypted and key material computed correctly. */
  data.auth_pk = &ip->auth_key_kp.pubkey;
  data.enc_kp = &ip->enc_key_kp;
  data.subcredential = subcredential;
  data.payload = payload;
  data.payload_len = payload_len;

  if (hs_cell_parse_introduce2(&data, circ, service) < 0) {
    goto done;
  }

  /* At this point, we just confirmed that the full INTRODUCE2 cell is valid
   * so increment our counter that we've seen one on this intro point. */
  ip->introduce2_count++;

  /* Launch rendezvous circuit with the onion key and rend cookie. */
  ret = hs_circ_launch_rendezvous_point(service, &data.onion_pk,
                                        data.rendezvous_cookie);
  if (ret < 0) {
    goto done;
  }

  /* Success. */
  ret = 0;

 done:
  memwipe(&data, 0, sizeof(data));
  return ret;
}

/* Circuit <b>circ</b> just finished the rend ntor key exchange. Use the key
 * exchange output material at <b>ntor_key_seed</b> and setup <b>circ</b> to
 * serve as a rendezvous end-to-end circuit between the client and the
 * service. If <b>is_service_side</b> is set, then we are the hidden service
 * and the other side is the client.
 *
 * Return 0 if the operation went well; in case of error return -1. */
int
hs_circuit_setup_e2e_rend_circ(origin_circuit_t *circ,
                               const uint8_t *ntor_key_seed, size_t seed_len,
                               int is_service_side)
{
  if (BUG(!circuit_purpose_is_correct_for_rend(TO_CIRCUIT(circ)->purpose,
                                        is_service_side))) {
    return -1;
  }

  crypt_path_t *hop = create_rend_cpath(ntor_key_seed, seed_len,
                                        is_service_side);
  if (!hop) {
    log_warn(LD_REND, "Couldn't get v3 %s cpath!",
             is_service_side ? "service-side" : "client-side");
    return -1;
  }

  finalize_rend_circuit(circ, hop, is_service_side);

  return 0;
}

/* We are a v2 legacy HS client and we just received a RENDEZVOUS1 cell
 * <b>rend_cell_body</b> on <b>circ</b>. Finish up the DH key exchange and then
 * extend the crypt path of <b>circ</b> so that the hidden service is on the
 * other side. */
int
hs_circuit_setup_e2e_rend_circ_legacy_client(origin_circuit_t *circ,
                                             const uint8_t *rend_cell_body)
{

  if (BUG(!circuit_purpose_is_correct_for_rend(
                                      TO_CIRCUIT(circ)->purpose, 0))) {
    return -1;
  }

  crypt_path_t *hop = create_rend_cpath_legacy(circ, rend_cell_body);
  if (!hop) {
    log_warn(LD_GENERAL, "Couldn't get v2 cpath.");
    return -1;
  }

  finalize_rend_circuit(circ, hop, 0);

  return 0;
}

