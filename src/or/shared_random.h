/* Copyright (c) 2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_SHARED_RANDOM_H
#define TOR_SHARED_RANDOM_H

/*
 * This file contains ABI/API of the shared random protocol defined in
 * proposal #250. Every public functions and data structure are namespaced
 * with "sr_" which stands for shared random.
 */

#include "or.h"

/* Protocol version */
#define SR_PROTO_VERSION  1
/* Default digest algorithm. */
#define SR_DIGEST_ALG DIGEST_SHA3_256
/* Invariant token in the SRV calculation. */
#define SR_SRV_TOKEN "shared-random"
/* Don't count the NUL terminated byte even though the TOKEN has it. */
#define SR_SRV_TOKEN_LEN (sizeof(SR_SRV_TOKEN) - 1)

/* Length of the random number (in bytes). */
#define SR_RANDOM_NUMBER_LEN 32
/* Size of a decoded commit value in a vote or state. It's a hash and a
 * timestamp. */
#define SR_COMMIT_LEN (DIGEST256_LEN + 8)
/* Size of a decoded reveal value from a vote or state. It's a 64 bit
 * timestamp and the hashed random number. This adds up to 40 bytes. */
#define SR_REVEAL_LEN (8 + DIGEST256_LEN)
/* Size of SRV message length. The construction is has follow:
 *  "shared-random" | INT_8(reveal_num) | INT_8(version) | PREV_SRV */
#define SR_SRV_MSG_LEN \
  (SR_SRV_TOKEN_LEN + 1 + 1 + DIGEST256_LEN)

/* Length of base64 encoded commit NOT including the NULL terminated byte.
 * Formula is taken from base64_encode_size. */
#define SR_COMMIT_BASE64_LEN \
  (((SR_COMMIT_LEN - 1) / 3) * 4 + 4)
/* Length of base64 encoded reveal NOT including the NULL terminated byte.
 * Formula is taken from base64_encode_size. This adds up to 56 bytes. */
#define SR_REVEAL_BASE64_LEN \
  (((SR_REVEAL_LEN - 1) / 3) * 4 + 4)

/* Protocol phase. */
typedef enum {
  /* Commitment phase */
  SR_PHASE_COMMIT  = 1,
  /* Reveal phase */
  SR_PHASE_REVEAL  = 2,
} sr_phase_t;

/* A shared random value (SRV). */
typedef struct sr_srv_t {
  /* The number of reveal values used to derive this SRV. */
  int num_reveals;
  /* The actual value. This is the stored result of SHA3-256. */
  uint8_t value[DIGEST256_LEN];
} sr_srv_t;

/* A commit (either ours or from another authority). */
typedef struct sr_commit_t {
  /* Hashing algorithm used. */
  digest_algorithm_t alg;

  /* Commit owner info */

  /* The RSA identity fingerprint of the authority. */
  char rsa_identity_fpr[FINGERPRINT_LEN + 1];

  /* Commitment information */

  /* Timestamp of reveal. Correspond to TIMESTAMP. */
  time_t reveal_ts;
  /* H(REVEAL) as found in COMMIT message. */
  char hashed_reveal[DIGEST256_LEN];
  /* Base64 encoded COMMIT. We use this to put it in our vote. */
  char encoded_commit[SR_COMMIT_BASE64_LEN + 1];

  /* Reveal information */

  /* H(RN) which is what we used as the random value for this commit. We
   * don't use the raw bytes since those are sent on the network thus
   * avoiding possible information leaks of our PRNG. */
  char random_number[DIGEST256_LEN];
  /* Timestamp of commit. Correspond to TIMESTAMP. */
  time_t commit_ts;
  /* This is the whole reveal message. We use it during verification */
  char encoded_reveal[SR_REVEAL_BASE64_LEN + 1];
} sr_commit_t;

/* API */

/* Public methods: */

int sr_init(int save_to_disk);
void sr_save_and_cleanup(void);
void sr_act_post_consensus(const networkstatus_t *consensus);
void sr_handle_received_commits(smartlist_t *commits,
                                crypto_pk_t *voter_key);
sr_commit_t *sr_parse_commit(const smartlist_t *args);
sr_srv_t *sr_parse_srv(const smartlist_t *args);
char *sr_get_string_for_vote(void);
char *sr_get_string_for_consensus(smartlist_t *votes);
void sr_commit_free(sr_commit_t *commit);

/* Private methods (only used by shared_random_state.c): */

void sr_compute_srv(void);
sr_commit_t *sr_generate_our_commit(time_t timestamp,
                                    authority_cert_t *my_rsa_cert);

#ifdef SHARED_RANDOM_PRIVATE

/* Encode */
STATIC int reveal_encode(sr_commit_t *commit, char *dst, size_t len);
STATIC int commit_encode(sr_commit_t *commit, char *dst, size_t len);
/* Decode. */
STATIC int commit_decode(const char *encoded, sr_commit_t *commit);
STATIC int reveal_decode(const char *encoded, sr_commit_t *commit);

STATIC int commit_has_reveal_value(const sr_commit_t *commit);

STATIC int verify_commit_and_reveal(const sr_commit_t *commit);

STATIC sr_srv_t *get_majority_srv_from_votes(smartlist_t *votes,
                                             unsigned int current);

STATIC void save_commit_to_state(sr_commit_t *commit);
STATIC sr_srv_t *srv_dup(const sr_srv_t *orig);
STATIC int commitments_are_the_same(const sr_commit_t *commit_one,
                                    const sr_commit_t *commit_two);
STATIC int commit_is_authoritative(const sr_commit_t *commit,
                                   const char *voter_key);
STATIC int should_keep_commit(sr_commit_t *commit, const char *voter_key);
STATIC void save_commit_during_reveal_phase(const sr_commit_t *commit);

#endif /* SHARED_RANDOM_PRIVATE */

#endif /* TOR_SHARED_RANDOM_H */
