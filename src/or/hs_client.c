/* Copyright (c) 2016-2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_client.c
 * \brief Implement next generation hidden service client functionality
 **/

#include "or.h"
#include "hs_circuit.h"
#include "hs_ident.h"
#include "connection_edge.h"
#include "container.h"
#include "rendclient.h"
#include "hs_descriptor.h"
#include "hs_cache.h"
#include "hs_cell.h"
#include "hs_ident.h"
#include "config.h"
#include "directory.h"
#include "hs_client.h"
#include "router.h"
#include "routerset.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "connection.h"
#include "circpathbias.h"
#include "connection.h"

/* Get all connections that are waiting on a circuit and flag them back to
 * waiting for a hidden service descriptor for the given service key
 * service_identity_pk. */
static void
flag_all_conn_wait_desc(const ed25519_public_key_t *service_identity_pk)
{
  tor_assert(service_identity_pk);

  smartlist_t *conns =
    connection_list_by_type_state(CONN_TYPE_AP, AP_CONN_STATE_CIRCUIT_WAIT);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    edge_connection_t *edge_conn;
    if (!CONN_IS_EDGE(conn)) {
      continue;
    }
    edge_conn = TO_EDGE_CONN(conn);
    if (edge_conn->hs_ident &&
        ed25519_pubkey_eq(&edge_conn->hs_ident->identity_pk,
                          service_identity_pk)) {
      connection_ap_mark_as_non_pending_circuit(TO_ENTRY_CONN(conn));
      conn->state = AP_CONN_STATE_RENDDESC_WAIT;
    }
  } SMARTLIST_FOREACH_END(conn);

  smartlist_free(conns);
}

/* A v3 HS circuit successfully connected to the hidden service. Update the
 * stream state at <b>hs_conn_ident</b> appropriately. */
static void
note_connection_attempt_succeeded(const hs_ident_edge_conn_t *hs_conn_ident)
{
  (void) hs_conn_ident;

  /* TODO: When implementing client side */
  return;
}

/* Given the pubkey of a hidden service in <b>onion_identity_pk</b>, fetch its
 * descriptor by launching a dir connection to <b>hsdir</b>. Return 1 on
 * success or -1 on error. */
static int
directory_launch_v3_desc_fetch(const ed25519_public_key_t *onion_identity_pk,
                               const routerstatus_t *hsdir)
{
  uint64_t current_time_period = hs_get_time_period_num(approx_time());
  ed25519_public_key_t blinded_pubkey;
  char base64_blinded_pubkey[ED25519_BASE64_LEN + 1];
  hs_ident_dir_conn_t hs_conn_dir_ident;
  int retval;

  tor_assert(hsdir);
  tor_assert(onion_identity_pk);

  /* Get blinded pubkey */
  hs_build_blinded_pubkey(onion_identity_pk, NULL, 0,
                          current_time_period, &blinded_pubkey);
  /* ...and base64 it. */
  retval = ed25519_public_to_base64(base64_blinded_pubkey, &blinded_pubkey);
  if (BUG(retval < 0)) {
    return -1;
  }

  /* Copy onion pk to a dir_ident so that we attach it to the dir conn */
  ed25519_pubkey_copy(&hs_conn_dir_ident.identity_pk, onion_identity_pk);

  /* Setup directory request */
  directory_request_t *req =
    directory_request_new(DIR_PURPOSE_FETCH_HSDESC);
  directory_request_set_routerstatus(req, hsdir);
  directory_request_set_indirection(req, DIRIND_ANONYMOUS);
  directory_request_set_resource(req, base64_blinded_pubkey);
  directory_request_upload_set_hs_ident(req, &hs_conn_dir_ident);
  directory_initiate_request(req);
  directory_request_free(req);

  log_info(LD_REND, "Descriptor fetch request for service %s with blinded "
                    "key %s to directory %s",
           safe_str_client(ed25519_fmt(onion_identity_pk)),
           safe_str_client(base64_blinded_pubkey),
           safe_str_client(routerstatus_describe(hsdir)));

  return 1;
}

/** Return the HSDir we should use to fetch the descriptor of the hidden
 *  service with identity key <b>onion_identity_pk</b>. */
static routerstatus_t *
pick_hsdir_v3(const ed25519_public_key_t *onion_identity_pk)
{
  int retval;
  char base64_blinded_pubkey[ED25519_BASE64_LEN + 1];
  uint64_t current_time_period = hs_get_time_period_num(approx_time());
  smartlist_t *responsible_hsdirs;
  ed25519_public_key_t blinded_pubkey;
  routerstatus_t *hsdir_rs = NULL;

  tor_assert(onion_identity_pk);

  responsible_hsdirs = smartlist_new();

  /* Get blinded pubkey of hidden service */
  hs_build_blinded_pubkey(onion_identity_pk, NULL, 0,
                          current_time_period, &blinded_pubkey);
  /* ...and base64 it. */
  retval = ed25519_public_to_base64(base64_blinded_pubkey, &blinded_pubkey);
  if (BUG(retval < 0)) {
    return NULL;
  }

  /* Get responsible hsdirs of service for this time period */
  hs_get_responsible_hsdirs(&blinded_pubkey, current_time_period, 0, 1,
                            responsible_hsdirs);

  log_debug(LD_REND, "Found %d responsible HSDirs and about to pick one.",
           smartlist_len(responsible_hsdirs));

  /* Pick an HSDir from the responsible ones. The ownership of
   * responsible_hsdirs is given to this function so no need to free it. */
  hsdir_rs = hs_pick_hsdir(responsible_hsdirs, base64_blinded_pubkey);

  return hsdir_rs;
}

/** Fetch a v3 descriptor using the given <b>onion_identity_pk</b>.
 *
 * On success, 1 is returned. If no hidden service is left to ask, return 0.
 * On error, -1 is returned. */
static int
fetch_v3_desc(const ed25519_public_key_t *onion_identity_pk)
{
  routerstatus_t *hsdir_rs =NULL;

  tor_assert(onion_identity_pk);

  hsdir_rs = pick_hsdir_v3(onion_identity_pk);
  if (!hsdir_rs) {
    log_warn(LD_GENERAL, "Didn't pick any v3 hsdirs... Failing.");
    return 0;
  }

  return directory_launch_v3_desc_fetch(onion_identity_pk, hsdir_rs);
}

/* Make sure that the given origin circuit circ is a valid correct
 * introduction circuit. This asserts on validation failure. */
static void
assert_intro_circ_ok(const origin_circuit_t *circ)
{
  tor_assert(circ);
  tor_assert(circ->base_.purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
  tor_assert(circ->hs_ident);
  tor_assert(hs_ident_intro_circ_is_valid(circ->hs_ident));
  assert_circ_anonymity_ok(circ, get_options());
}

/* Find a descriptor intro point object that matches the given ident in the
 * given descriptor desc. Return NULL if not found. */
static const hs_desc_intro_point_t *
find_desc_intro_point_by_ident(const hs_ident_circuit_t *ident,
                               const hs_descriptor_t *desc)
{
  const hs_desc_intro_point_t *intro_point = NULL;

  tor_assert(ident);
  tor_assert(desc);

  SMARTLIST_FOREACH_BEGIN(desc->encrypted_data.intro_points,
                          const hs_desc_intro_point_t *, ip) {
    if (ed25519_pubkey_eq(&ident->intro_auth_pk,
                          &ip->auth_key_cert->signed_key)) {
      intro_point = ip;
      break;
    }
  } SMARTLIST_FOREACH_END(ip);

  return intro_point;
}

/* Send an INTRODUCE1 cell along the intro circuit and populate the rend
 * circuit identifier with the needed key material for the e2e encryption.
 * Return 0 on success, -1 if there is a transient error such that an action
 * has been taken to recover and -2 if there is a permanent error indicating
 * that both circuits were closed. */
static int
send_introduce1(origin_circuit_t *intro_circ,
                origin_circuit_t *rend_circ)
{
  int status;
  char onion_address[HS_SERVICE_ADDR_LEN_BASE32 + 1];
  const ed25519_public_key_t *service_identity_pk = NULL;
  const hs_desc_intro_point_t *ip;

  assert_intro_circ_ok(intro_circ);
  tor_assert(rend_circ);

  service_identity_pk = &intro_circ->hs_ident->identity_pk;
  /* For logging purposes. There will be a time where the hs_ident will have a
   * version number but for now there is none because it's all v3. */
  hs_build_address(service_identity_pk, HS_VERSION_THREE, onion_address);

  log_info(LD_REND, "Sending INTRODUCE1 cell to service %s on circuit %u",
           safe_str_client(onion_address), TO_CIRCUIT(intro_circ)->n_circ_id);

  /* 1) Get descriptor from our cache. */
  const hs_descriptor_t *desc =
    hs_cache_lookup_as_client(service_identity_pk);
  if (desc == NULL || !hs_client_any_intro_points_usable(desc)) {
    log_info(LD_REND, "Request to %s %s. Trying to fetch a new descriptor.",
             safe_str_client(onion_address),
             (desc) ? "didn't have usable intro points" :
             "didn't have a descriptor");
    hs_client_refetch_hsdesc(service_identity_pk);
    /* We just triggered a refetch, make sure every connections are back
     * waiting for that descriptor. */
    flag_all_conn_wait_desc(service_identity_pk);
    /* We just asked for a refetch so this is a transient error. */
    goto tran_err;
  }

  /* We need to find which intro point in the descriptor we are connected to
   * on intro_circ. */
  ip = find_desc_intro_point_by_ident(intro_circ->hs_ident, desc);
  if (BUG(ip == NULL)) {
    /* If we can find a descriptor from this introduction circuit ident, we
     * must have a valid intro point object. Permanent error. */
    goto perm_err;
  }

  /* Send the INTRODUCE1 cell. */
  if (hs_circ_send_introduce1(intro_circ, rend_circ, ip,
                              desc->subcredential) < 0) {
    /* Unable to send the cell, both circuits have been closed, this is a
     * permanent error. */
    goto perm_err;
  }

  /* Cell has been sent successfully. Copy the introduction point
   * authentication and encryption key in the rendezvous circuit identifier so
   * we can compute the ntor keys when we receive the RENDEZVOUS2 cell. */
  memcpy(&rend_circ->hs_ident->intro_enc_pk, &ip->enc_key,
         sizeof(rend_circ->hs_ident->intro_enc_pk));
  ed25519_pubkey_copy(&rend_circ->hs_ident->intro_auth_pk,
                      &intro_circ->hs_ident->intro_auth_pk);

  /* Now, we wait for an ACK or NAK on this circuit. */
  circuit_change_purpose(TO_CIRCUIT(intro_circ),
                         CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT);
  /* Set timestamp_dirty, because circuit_expire_building expects it to
   * specify when a circuit entered the _C_INTRODUCE_ACK_WAIT state. */
  TO_CIRCUIT(intro_circ)->timestamp_dirty = time(NULL);
  pathbias_count_use_attempt(intro_circ);

  /* Success. */
  status = 0;
  goto end;

 perm_err:
  /* Permanent error: it is possible that the intro circuit was closed prior
   * because we weren't able to send the cell. Make sure we don't double close
   * it which would result in a warning. */
  if (!TO_CIRCUIT(intro_circ)->marked_for_close) {
    circuit_mark_for_close(TO_CIRCUIT(intro_circ), END_CIRC_REASON_INTERNAL);
  }
  circuit_mark_for_close(TO_CIRCUIT(rend_circ), END_CIRC_REASON_INTERNAL);
  status = -2;
  goto end;

 tran_err:
  status = -1;

 end:
  memwipe(onion_address, 0, sizeof(onion_address));
  return status;
}

/* Using the introduction circuit circ, setup the authentication key of the
 * intro point this circuit has extended to. */
static void
setup_intro_circ_auth_key(origin_circuit_t *circ)
{
  const hs_descriptor_t *desc;

  tor_assert(circ);

  desc = hs_cache_lookup_as_client(&circ->hs_ident->identity_pk);
  if (BUG(desc == NULL)) {
    /* Opening intro circuit without the descriptor is no good... */
    goto end;
  }

  /* We will go over every intro point and try to find which one is linked to
   * that circuit. Those lists are small so it's not that expensive. */
  SMARTLIST_FOREACH_BEGIN(desc->encrypted_data.intro_points,
                          const hs_desc_intro_point_t *, ip) {
    SMARTLIST_FOREACH_BEGIN(ip->link_specifiers,
                            const hs_desc_link_specifier_t *, lspec) {
      /* Not all tor node have an ed25519 identity key so we still rely on the
       * legacy identity digest. */
      if (lspec->type != LS_LEGACY_ID) {
        continue;
      }
      if (fast_memneq(circ->build_state->chosen_exit->identity_digest,
                      lspec->u.legacy_id, DIGEST_LEN)) {
        break;
      }
      /* We got it, copy its authentication key to the identifier. */
      ed25519_pubkey_copy(&circ->hs_ident->intro_auth_pk,
                          &ip->auth_key_cert->signed_key);
      goto end;
    } SMARTLIST_FOREACH_END(lspec);
  } SMARTLIST_FOREACH_END(ip);

  /* Reaching this point means we didn't find any intro point for this circuit
   * which is not suppose to happen. */
  tor_assert_nonfatal_unreached();

 end:
  return;
}

/* Called when an introduction circuit has opened. */
static void
client_intro_circ_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ);
  tor_assert(TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
  log_info(LD_REND, "Introduction circuit %u has opened. Attaching streams.",
           (unsigned int) TO_CIRCUIT(circ)->n_circ_id);

  /* This is an introduction circuit so we'll attach the correct
   * authentication key to the circuit identifier so it can be identified
   * properly later on. */
  setup_intro_circ_auth_key(circ);

  connection_ap_attach_pending(1);
}

/* Called when a rendezvous circuit has opened. */
static void
client_rendezvous_circ_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ);
  tor_assert(TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND);

  log_info(LD_REND, "Rendezvous circuit has opened to %s.",
           safe_str_client(
                extend_info_describe(circ->build_state->chosen_exit)));

  /* Ignore returned value, nothing we can really do. On failure, the circuit
   * will be marked for close. */
  hs_circ_send_establish_rendezvous(circ);
}

/* This is an helper function that convert a descriptor intro point object ip
 * to a newly allocated extend_info_t object fully initialized. Return NULL if
 * we can't convert it for which chances are that we are missing or malformed
 * link specifiers. */
static extend_info_t *
desc_intro_point_to_extend_info(const hs_desc_intro_point_t *ip)
{
  extend_info_t *ei;
  smartlist_t *lspecs = smartlist_new();

  tor_assert(ip);

  /* We first encode the descriptor link specifiers into the binary
   * representation which is a trunnel object. */
  SMARTLIST_FOREACH_BEGIN(ip->link_specifiers,
                          const hs_desc_link_specifier_t *, desc_lspec) {
    link_specifier_t *lspec = hs_desc_encode_lspec(desc_lspec);
    smartlist_add(lspecs, lspec);
  } SMARTLIST_FOREACH_END(desc_lspec);

  /* Explicitely put the direct connection option to 0 because this is client
   * side and there is no such thing as a non anonymous client. */
  ei = hs_get_extend_info_from_lspecs(lspecs, &ip->onion_key, 0);

  SMARTLIST_FOREACH(lspecs, link_specifier_t *, ls, link_specifier_free(ls));
  smartlist_free(lspecs);
  return ei;
}

/* Using a descriptor desc, return a newly allocated extend_info_t object of a
 * randomly picked introduction point from its list. Return NULL if none are
 * usable. */
static extend_info_t *
client_get_random_intro(const ed25519_public_key_t *service_pk)
{
  extend_info_t *ei = NULL, *ei_excluded = NULL;
  smartlist_t *usable_ips = NULL;
  const hs_descriptor_t *desc;
  const hs_desc_encrypted_data_t *enc_data;
  const or_options_t *options = get_options();

  tor_assert(service_pk);

  desc = hs_cache_lookup_as_client(service_pk);
  if (desc == NULL || !hs_client_any_intro_points_usable(desc)) {
    log_info(LD_REND, "Unable to randomly select an introduction point "
                      "because descriptor %s.",
             (desc) ? "doesn't have usable intro point" : "is missing");
    goto end;
  }

  enc_data = &desc->encrypted_data;
  usable_ips = smartlist_new();
  smartlist_add_all(usable_ips, enc_data->intro_points);
  while (smartlist_len(usable_ips) != 0) {
    int idx;
    const hs_desc_intro_point_t *ip;

    /* Pick a random intro point and immediately remove it from the usable
     * list so we don't pick it again if we have to iterate more. */
    idx = crypto_rand_int(smartlist_len(usable_ips));
    ip = smartlist_get(usable_ips, idx);
    smartlist_del(usable_ips, idx);

    /* Generate an extend info object from the intro point object. */
    ei = desc_intro_point_to_extend_info(ip);
    if (ei == NULL) {
      /* We can get here for instance if the intro point is a private address
       * and we aren't allowed to extend to those. */
      continue;
    }

    /* Test the pick against ExcludeNodes. */
    if (routerset_contains_extendinfo(options->ExcludeNodes, ei)) {
      /* If this pick is in the ExcludeNodes list, we keep its reference so if
       * we ever end up not being able to pick anything else and StrictNodes is
       * unset, we'll use it. */
      ei_excluded = ei;
      continue;
    }
    /* XXX: Intro point can time out or just be unsuable, we need to keep
     * track of this and check against such cache. */

    /* Good pick! Let's go with this. */
    goto end;
  }

  /* Reaching this point means a couple of things. Either we can't use any of
   * the intro point listed because the IP address can't be extended to or it
   * is listed in the ExcludeNodes list. In the later case, if StrictNodes is
   * set, we are forced to not use anything. */
  ei = ei_excluded;
  if (options->StrictNodes) {
    log_warn(LD_REND, "Every introduction points are in the ExcludeNodes set "
             "and StrictNodes is set. We can't connect.");
    ei = NULL;
  }

 end:
  smartlist_free(usable_ips);
  return ei;
}

/* Called when we get an INTRODUCE_ACK success status code. Do the appropriate
 * actions for the rendezvous point and finally close intro_circ. */
static void
handle_introduce_ack_success(origin_circuit_t *intro_circ)
{
  origin_circuit_t *rend_circ = NULL;

  tor_assert(intro_circ);

  log_info(LD_REND, "Received INTRODUCE_ACK ack! Informing rendezvous");

  /* Get the rendezvous circuit matching this intro point circuit.
   * XXX Replace this by our hs circuitmap to support client? */
  rend_circ = circuit_get_ready_rend_by_hs_ident(intro_circ->hs_ident);
  if (rend_circ == NULL) {
    log_warn(LD_REND, "Can't find any rendezvous circuit. Stopping");
    goto end;
  }

  assert_circ_anonymity_ok(rend_circ, get_options());
  circuit_change_purpose(TO_CIRCUIT(rend_circ),
                         CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED);
  /* Set timestamp_dirty, because circuit_expire_building expects it to
   * specify when a circuit entered the
   * CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED state. */
  TO_CIRCUIT(rend_circ)->timestamp_dirty = time(NULL);

 end:
  /* We don't need the intro circuit anymore. It did what it had to do! */
  circuit_change_purpose(TO_CIRCUIT(intro_circ),
                         CIRCUIT_PURPOSE_C_INTRODUCE_ACKED);
  circuit_mark_for_close(TO_CIRCUIT(intro_circ), END_CIRC_REASON_FINISHED);

  /* XXX: Close pending intro circuits we might have in parallel. */
  return;
}

/* Called when we get an INTRODUCE_ACK failure status code. Depending on our
 * failure cache status, either close the circuit or re-extend to a new
 * introduction point. */
static void
handle_introduce_ack_bad(origin_circuit_t *circ, int status)
{
  tor_assert(circ);

  log_info(LD_REND, "Received INTRODUCE_ACK nack by %s. Reason: %u",
      safe_str_client(extend_info_describe(circ->build_state->chosen_exit)),
      status);

  /* It's a NAK. The introduction point didn't relay our request. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_INTRODUCING);

  /* XXX: Report this failure for the intro point failure cache. Depending on
   * how many times we've tried this intro point, close it or reextend. */
}

/* Called when we get an INTRODUCE_ACK on the intro circuit circ. The encoded
 * cell is in payload of length payload_len. Return 0 on success else a
 * negative value. The circuit is either close or reuse to re-extend to a new
 * introduction point. */
static int
handle_introduce_ack(origin_circuit_t *circ, const uint8_t *payload,
                     size_t payload_len)
{
  int status, ret = -1;

  tor_assert(circ);
  tor_assert(circ->build_state);
  tor_assert(circ->build_state->chosen_exit);
  assert_circ_anonymity_ok(circ, get_options());
  tor_assert(payload);

  status = hs_cell_parse_introduce_ack(payload, payload_len);
  switch (status) {
  case HS_CELL_INTRO_ACK_SUCCESS:
    ret = 0;
    handle_introduce_ack_success(circ);
    break;
  case HS_CELL_INTRO_ACK_FAILURE:
  case HS_CELL_INTRO_ACK_BADFMT:
  case HS_CELL_INTRO_ACK_NORELAY:
    handle_introduce_ack_bad(circ, status);
    break;
  default:
    log_info(LD_PROTOCOL, "Unknown INTRODUCE_ACK status code %u from %s",
        status,
        safe_str_client(extend_info_describe(circ->build_state->chosen_exit)));
    break;
  }

  return ret;
}

/* ========== */
/* Public API */
/* ========== */

/** A circuit just finished connecting to a hidden service that the stream
 *  <b>conn</b> has been waiting for. Let the HS subsystem know about this. */
void
hs_client_note_connection_attempt_succeeded(const edge_connection_t *conn)
{
  tor_assert(connection_edge_is_rendezvous_stream(conn));

  if (BUG(conn->rend_data && conn->hs_ident)) {
    log_warn(LD_BUG, "Stream had both rend_data and hs_ident..."
             "Prioritizing hs_ident");
  }

  if (conn->hs_ident) { /* It's v3: pass it to the prop224 handler */
    note_connection_attempt_succeeded(conn->hs_ident);
    return;
  } else if (conn->rend_data) { /* It's v2: pass it to the legacy handler */
    rend_client_note_connection_attempt_ended(conn->rend_data);
    return;
  }
}

/* With the given encoded descriptor in desc_str and the service key in
 * service_identity_pk, decode the descriptor and set the desc pointer with a
 * newly allocated descriptor object.
 *
 * Return 0 on success else a negative value. */
int
hs_client_decode_descriptor(const char *desc_str,
                            const ed25519_public_key_t *service_identity_pk,
                            hs_descriptor_t **desc)
{
  uint8_t subcredential[DIGEST256_LEN];

  tor_assert(desc_str);
  tor_assert(service_identity_pk);
  tor_assert(desc);

  /* Create subcredential for this HS so that we can decrypt */
  {
    ed25519_public_key_t blinded_pubkey;
    uint64_t current_time_period = hs_get_time_period_num(approx_time());
    hs_build_blinded_pubkey(service_identity_pk, NULL, 0, current_time_period,
                            &blinded_pubkey);
    hs_get_subcredential(service_identity_pk, &blinded_pubkey, subcredential);
  }

  /* Parse descriptor */
  if (hs_desc_decode_descriptor(desc_str, subcredential, desc) < 0) {
    log_warn(LD_GENERAL, "Could not parse received descriptor as client");
    goto err;
  }

  return 0;
 err:
  return -1;
}

/** Return true if there are any usable intro points in the v3 HS descriptor
 *  <b>desc</b>. */
int
hs_client_any_intro_points_usable(const hs_descriptor_t *desc)
{
  /* XXX stub waiting for more client-side work:
     equivalent to v2 rend_client_any_intro_points_usable() */
  tor_assert(desc);
  return 1;
}

/** Launch a connection to a hidden service directory to fetch a hidden
 * service descriptor using <b>identity_pk</b> to get the necessary keys.
 *
 * On success, 1 is returned. If no hidden service is left to ask, return 0.
 * On error, -1 is returned. (retval is only used by unittests right now) */
int
hs_client_refetch_hsdesc(const ed25519_public_key_t *identity_pk)
{
  tor_assert(identity_pk);

  /* Are we configured to fetch descriptors? */
  if (!get_options()->FetchHidServDescriptors) {
    log_warn(LD_REND, "We received an onion address for a hidden service "
                      "descriptor but we are configured to not fetch.");
    return 0;
  }

  /* Check if fetching a desc for this HS is useful to us right now */
  {
    const hs_descriptor_t *cached_desc = NULL;
    cached_desc = hs_cache_lookup_as_client(identity_pk);
    if (cached_desc && hs_client_any_intro_points_usable(cached_desc)) {
      log_warn(LD_GENERAL, "We would fetch a v3 hidden service descriptor "
                            "but we already have a useable descriprot.");
      return 0;
    }
  }

  return fetch_v3_desc(identity_pk);
}

/* This is called when we are trying to attach an AP connection to these
 * hidden service circuits from connection_ap_handshake_attach_circuit().
 * Return 0 on success, -1 for a transient error that is actions were
 * triggered to recover or -2 for a permenent error where both circuits will
 * marked for close.
 *
 * The following supports every hidden service version. */
int
hs_client_send_introduce1(origin_circuit_t *intro_circ,
                          origin_circuit_t *rend_circ)
{
  return (intro_circ->hs_ident) ? send_introduce1(intro_circ, rend_circ) :
                                  rend_client_send_introduction(intro_circ,
                                                                rend_circ);
}

/* Called when the client circuit circ has been established. It can be either
 * an introduction or rendezvous circuit. This function handles all hidden
 * service versions. */
void
hs_client_circuit_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ);

  /* Handle both version. v2 uses rend_data and v3 uses the hs circuit
   * identifier hs_ident. Can't be both. */
  switch (TO_CIRCUIT(circ)->purpose) {
  case CIRCUIT_PURPOSE_C_INTRODUCING:
    (circ->hs_ident) ? client_intro_circ_has_opened(circ) :
                       rend_client_introcirc_has_opened(circ);
    break;
  case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
    (circ->hs_ident) ? client_rendezvous_circ_has_opened(circ) :
                       rend_client_rendcirc_has_opened(circ);
    break;
  default:
    tor_assert_nonfatal_unreached();
  }
}

/* Called when we receive a RENDEZVOUS_ESTABLISHED cell. Change the state of
 * the circuit to CIRCUIT_PURPOSE_C_REND_READY. Return 0 on success else a
 * negative value and the circuit marked for close. */
int
hs_client_receive_rendezvous_acked(origin_circuit_t *circ,
                                   const uint8_t *payload, size_t payload_len)
{
  tor_assert(circ);
  tor_assert(payload);

  (void) payload_len;

  if (TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_ESTABLISH_REND) {
    log_warn(LD_PROTOCOL, "Got a RENDEZVOUS_ESTABLISHED but we were not "
                          "expecting one. Closing circuit.");
    goto err;
  }

  log_info(LD_REND, "Received an RENDEZVOUS_ESTABLISHED. This circuit is "
                    "now ready for rendezvous.");
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_REND_READY);

  /* Set timestamp_dirty, because circuit_expire_building expects it to
   * specify when a circuit entered the _C_REND_READY state. */
  TO_CIRCUIT(circ)->timestamp_dirty = time(NULL);

  /* From a path bias point of view, this circuit is now successfully used.
   * Waiting any longer opens us up to attacks from malicious hidden services.
   * They could induce the client to attempt to connect to their hidden
   * service and never reply to the client's rend requests */
  pathbias_mark_use_success(circ);

  /* If we already have the introduction circuit built, make sure we send
   * the INTRODUCE cell _now_ */
  connection_ap_attach_pending(1);

  return 0;
 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}

/* This is called when a descriptor has arrived following a fetch request and
 * has been stored in the client cache. Every entry connection that matches
 * the service identity key in the ident will get attached to the hidden
 * service circuit. */
void
hs_client_desc_has_arrived(const hs_ident_dir_conn_t *ident)
{
  time_t now = time(NULL);
  smartlist_t *conns = NULL;

  tor_assert(ident);

  conns = connection_list_by_type_state(CONN_TYPE_AP,
                                        AP_CONN_STATE_RENDDESC_WAIT);
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
    const hs_descriptor_t *desc;
    entry_connection_t *entry_conn = TO_ENTRY_CONN(base_conn);
    const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(entry_conn);

    /* Only consider the entry connections that matches the service for which
     * we just fetched its descriptor. */
    if (!edge_conn->hs_ident ||
        !ed25519_pubkey_eq(&ident->identity_pk,
                           &edge_conn->hs_ident->identity_pk)) {
      continue;
    }
    assert_connection_ok(base_conn, now);

    /* We were just called because we stored the descriptor for this service
     * so not finding a descriptor means we have a bigger problem. */
    desc = hs_cache_lookup_as_client(&ident->identity_pk);
    if (BUG(desc == NULL)) {
      goto end;
    }

    if (!hs_client_any_intro_points_usable(desc)) {
      log_info(LD_REND, "Hidden service descriptor is unusable. "
                        "Closing streams.");
      connection_mark_unattached_ap(entry_conn,
                                    END_STREAM_REASON_RESOLVEFAILED);
      /* XXX: Note the connection attempt. */
      goto end;
    }

    log_info(LD_REND, "Descriptor has arrived. Launching circuits.");

    /* Restart their timeout values, so they get a fair shake at connecting to
     * the hidden service. XXX: Improve comment on why this is needed. */
    base_conn->timestamp_created = now;
    base_conn->timestamp_lastread = now;
    base_conn->timestamp_lastwritten = now;
    /* Change connection's state into waiting for a circuit. */
    base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

    connection_ap_mark_as_pending_circuit(entry_conn);
  } SMARTLIST_FOREACH_END(base_conn);

 end:
  /* We don't have ownership of the objects in this list. */
  smartlist_free(conns);
}

/* Return a newly allocated extend_info_t for a randomly chosen introduction
 * point for the given edge connection identifier ident. Return NULL if we
 * can't pick any usable introduction points. */
extend_info_t *
hs_client_get_random_intro_from_edge(const edge_connection_t *edge_conn)
{
  tor_assert(edge_conn);

  return (edge_conn->hs_ident) ?
    client_get_random_intro(&edge_conn->hs_ident->identity_pk) :
    rend_client_get_random_intro(edge_conn->rend_data);
}
/* Called when get an INTRODUCE_ACK cell on the introduction circuit circ.
 * Return 0 on success else a negative value is returned. The circuit will be
 * closed or reuse to extend again to another intro point. */
int
hs_client_receive_introduce_ack(origin_circuit_t *circ,
                                const uint8_t *payload, size_t payload_len)
{
  int ret = -1;

  tor_assert(circ);
  tor_assert(payload);

  if (TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) {
    log_warn(LD_PROTOCOL, "Unexpected INTRODUCE_ACK on circuit %u.",
             (unsigned int) TO_CIRCUIT(circ)->n_circ_id);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    goto end;
  }

  ret = (circ->hs_ident) ? handle_introduce_ack(circ, payload, payload_len) :
                           rend_client_introduction_acked(circ, payload,
                                                          payload_len);
  /* For path bias: This circuit was used successfully. NACK or ACK counts. */
  pathbias_mark_use_success(circ);

 end:
  return ret;
}

