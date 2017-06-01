/* Copyright (c) 2017, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_hs_common.c
 * \brief Test hidden service common functionalities.
 */

#define HS_COMMON_PRIVATE

#include "test.h"
#include "test_helpers.h"
#include "log_test_helpers.h"
#include "hs_test_helpers.h"

#include "connection_edge.h"
#include "hs_common.h"

static void
test_validate_address(void *arg)
{
  int ret;

  (void) arg;

  /* Address too short and too long. */
  setup_full_capture_of_logs(LOG_WARN);
  ret = hs_address_is_valid("blah");
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg_containing("has an invalid length");
  teardown_capture_of_logs();

  setup_full_capture_of_logs(LOG_WARN);
  ret = hs_address_is_valid(
           "p3xnclpu4mu22dwaurjtsybyqk4xfjmcfz6z62yl24uwmhjatiwnlnadb");
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg_containing("has an invalid length");
  teardown_capture_of_logs();

  /* Invalid checksum (taken from prop224) */
  setup_full_capture_of_logs(LOG_WARN);
  ret = hs_address_is_valid(
           "l5satjgud6gucryazcyvyvhuxhr74u6ygigiuyixe3a6ysis67ororad");
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg_containing("invalid checksum");
  teardown_capture_of_logs();

  setup_full_capture_of_logs(LOG_WARN);
  ret = hs_address_is_valid(
           "btojiu7nu5y5iwut64eufevogqdw4wmqzugnoluw232r4t3ecsfv37ad");
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg_containing("invalid checksum");
  teardown_capture_of_logs();

  /* Non base32 decodable string. */
  setup_full_capture_of_logs(LOG_WARN);
  ret = hs_address_is_valid(
           "????????????????????????????????????????????????????????");
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg_containing("can't be decoded");
  teardown_capture_of_logs();

  /* Valid address. */
  ret = hs_address_is_valid(
           "p3xnclpu4mu22dwaurjtsybyqk4xfjmcfz6z62yl24uwmhjatiwnlnad");
  tt_int_op(ret, OP_EQ, 1);

 done:
  ;
}

static void
test_build_address(void *arg)
{
  int ret;
  char onion_addr[HS_SERVICE_ADDR_LEN_BASE32 + 1];
  ed25519_public_key_t pubkey;

  (void) arg;

  /* The following has been created with hs_build_address.py script that
   * follows proposal 224 specification to build an onion address. */
  static const char *test_addr =
    "ijbeeqscijbeeqscijbeeqscijbeeqscijbeeqscijbeeqscijbezhid";

  /* Let's try to build the same onion address that the script can do. Key is
   * a long set of very random \x42 :). */
  memset(&pubkey, '\x42', sizeof(pubkey));
  hs_build_address(&pubkey, HS_VERSION_THREE, onion_addr);
  tt_str_op(test_addr, OP_EQ, onion_addr);
  /* Validate that address. */
  ret = hs_address_is_valid(onion_addr);
  tt_int_op(ret, OP_EQ, 1);

 done:
  ;
}

/** Test that our HS time period calculation functions work properly */
static void
test_time_period(void *arg)
{
  (void) arg;
  unsigned int tn;
  int retval;
  time_t fake_time;

  /* Let's do the example in prop224 section [TIME-PERIODS] */
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 11:00:00 UTC",
                              &fake_time);
  tt_int_op(retval, ==, 0);

  /* Check that the time period number is right */
  tn = hs_get_time_period_num(fake_time);
  tt_int_op(tn, ==, 16903);

  /* Increase current time to 11:59:59 UTC and check that the time period
     number is still the same */
  fake_time += 3599;
  tn = hs_get_time_period_num(fake_time);
  tt_int_op(tn, ==, 16903);

  /* Now take time to 12:00:00 UTC and check that the time period rotated */
  fake_time += 1;
  tn = hs_get_time_period_num(fake_time);
  tt_int_op(tn, ==, 16904);

  /* Now also check our hs_get_next_time_period_num() function */
  tn = hs_get_next_time_period_num(fake_time);
  tt_int_op(tn, ==, 16905);

 done:
  ;
}

/** Test that our HS overlap period functions work properly. */
static void
test_desc_overlap_period(void *arg)
{
  (void) arg;
  int retval;
  time_t now = time(NULL);
  networkstatus_t *dummy_consensus = NULL;

  /* First try with a consensus inside the overlap period */
  dummy_consensus = tor_malloc_zero(sizeof(networkstatus_t));
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 10:00:00 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);

  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  /* Now increase the valid_after so that it goes to 11:00:00 UTC. Overlap
     period is still active. */
  dummy_consensus->valid_after += 3600;
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  /* Now increase the valid_after so that it goes to 11:59:59 UTC. Overlap
     period is still active. */
  dummy_consensus->valid_after += 3599;
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  /* Now increase the valid_after so that it drifts to noon, and check that
     overlap mode is not active anymore. */
  dummy_consensus->valid_after += 1;
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 0);

  /* Check that overlap mode is also inactive at 23:59:59 UTC */
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 23:59:59 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 0);

 done:
  tor_free(dummy_consensus);
}

/** Test our HS descriptor request tracker by making various requests and
 *  checking whether they get tracked properly. */
static void
test_hid_serv_request_tracker(void *arg)
{
  (void) arg;
  time_t retval;
  routerstatus_t *hsdir = NULL, *hsdir2 = NULL;
  time_t now = approx_time();

  const char *req_key_str_first =
 "vd4zb6zesaubtrjvdqcr2w7x7lhw2up4Xnw4526ThUNbL5o1go+EdUuEqlKxHkNbnK41pRzizzs";
  const char *req_key_str_second =
 "g53o7iavcd62oihswhr24u6czmqws5kpXnw4526ThUNbL5o1go+EdUuEqlKxHkNbnK41pRzizzs";

  /*************************** basic test *******************************/

  /* Get request tracker and make sure it's empty */
  strmap_t *request_tracker = get_last_hid_serv_requests();
  tt_int_op(strmap_size(request_tracker),OP_EQ, 0);

  /* Let's register a hid serv request */
  hsdir = tor_malloc_zero(sizeof(routerstatus_t));
  memset(hsdir->identity_digest, 'Z', DIGEST_LEN);
  retval = hs_lookup_last_hid_serv_request(hsdir, req_key_str_first,
                                           now, 1);
  tt_int_op(retval, OP_EQ, now);
  tt_int_op(strmap_size(request_tracker),OP_EQ, 1);

  /* Let's lookup a non-existent hidserv request */
  retval = hs_lookup_last_hid_serv_request(hsdir, req_key_str_second,
                                           now+1, 0);
  tt_int_op(retval, OP_EQ, 0);
  tt_int_op(strmap_size(request_tracker),OP_EQ, 1);

  /* Let's lookup a real hidserv request */
  retval = hs_lookup_last_hid_serv_request(hsdir, req_key_str_first,
                                           now+2, 0);
  tt_int_op(retval, OP_EQ, now); /* we got it */
  tt_int_op(strmap_size(request_tracker),OP_EQ, 1);

  /**********************************************************************/

  /* Let's add another request for the same HS but on a different HSDir. */
  hsdir2 = tor_malloc_zero(sizeof(routerstatus_t));
  memset(hsdir->identity_digest, 2, DIGEST_LEN);
  retval = hs_lookup_last_hid_serv_request(hsdir2, req_key_str_first,
                                           now+3, 1);
  tt_int_op(retval, OP_EQ, now+3);
  tt_int_op(strmap_size(request_tracker),OP_EQ, 2);

  /* Check that we can clean the first request based on time */
  hs_clean_last_hid_serv_requests(now+3+REND_HID_SERV_DIR_REQUERY_PERIOD);
  tt_int_op(strmap_size(request_tracker),OP_EQ, 1);
  /* Check that it doesn't exist anymore */
  retval = hs_lookup_last_hid_serv_request(hsdir, req_key_str_first,
                                           now+2, 0);
  tt_int_op(retval, OP_EQ, 0);

  /*************************** deleting entries **************************/

  /* Add another request with very short key */
  retval = hs_lookup_last_hid_serv_request(hsdir, "l",  now, 1);

  /* Try deleting entries with a dummy key. Check that our previous requests
   * are still there */
  hs_purge_hid_serv_from_last_hid_serv_requests("a");
  tt_int_op(strmap_size(request_tracker),OP_EQ, 2);

  /* Try another dummy key. Check that requests are still there */
  {
    char dummy[2000];
    memset(dummy, 'Z', 2000);
    dummy[1999] = '\x00';
    hs_purge_hid_serv_from_last_hid_serv_requests(dummy);
    tt_int_op(strmap_size(request_tracker),OP_EQ, 2);
  }

  /* Another dummy key! */
  hs_purge_hid_serv_from_last_hid_serv_requests(req_key_str_second);
  tt_int_op(strmap_size(request_tracker),OP_EQ, 2);

  /* Now actually delete a request! */
  hs_purge_hid_serv_from_last_hid_serv_requests(req_key_str_first);
  tt_int_op(strmap_size(request_tracker),OP_EQ, 1);

  /* Purge it all! */
  hs_purge_last_hid_serv_requests();
  request_tracker = get_last_hid_serv_requests();
  tt_int_op(strmap_size(request_tracker),OP_EQ, 0);

 done:
  tor_free(hsdir);
  tor_free(hsdir2);
}

static void
test_parse_extended_hostname(void *arg)
{
  (void) arg;

  char address1[] = "fooaddress.onion";
  char address2[] = "aaaaaaaaaaaaaaaa.onion";
  char address3[] = "fooaddress.exit";
  char address4[] = "www.torproject.org";
  char address5[] = "foo.abcdefghijklmnop.onion";
  char address6[] = "foo.bar.abcdefghijklmnop.onion";
  char address7[] = ".abcdefghijklmnop.onion";
  char address8[] =
    "www.p3xnclpu4mu22dwaurjtsybyqk4xfjmcfz6z62yl24uwmhjatiwnlnad.onion";

  tt_assert(BAD_HOSTNAME == parse_extended_hostname(address1));
  tt_assert(ONION_V2_HOSTNAME == parse_extended_hostname(address2));
  tt_str_op(address2,OP_EQ, "aaaaaaaaaaaaaaaa");
  tt_assert(EXIT_HOSTNAME == parse_extended_hostname(address3));
  tt_assert(NORMAL_HOSTNAME == parse_extended_hostname(address4));
  tt_assert(ONION_V2_HOSTNAME == parse_extended_hostname(address5));
  tt_str_op(address5,OP_EQ, "abcdefghijklmnop");
  tt_assert(ONION_V2_HOSTNAME == parse_extended_hostname(address6));
  tt_str_op(address6,OP_EQ, "abcdefghijklmnop");
  tt_assert(BAD_HOSTNAME == parse_extended_hostname(address7));
  tt_assert(ONION_V3_HOSTNAME == parse_extended_hostname(address8));
  tt_str_op(address8, OP_EQ,
            "p3xnclpu4mu22dwaurjtsybyqk4xfjmcfz6z62yl24uwmhjatiwnlnad");

 done: ;
}

struct testcase_t hs_common_tests[] = {
  { "build_address", test_build_address, TT_FORK,
    NULL, NULL },
  { "validate_address", test_validate_address, TT_FORK,
    NULL, NULL },
  { "time_period", test_time_period, TT_FORK,
    NULL, NULL },
  { "desc_overlap_period", test_desc_overlap_period, TT_FORK,
    NULL, NULL },
  { "hid_serv_request_tracker", test_hid_serv_request_tracker,
    TT_FORK, NULL, NULL },
  { "parse_extended_hostname", test_parse_extended_hostname, TT_FORK,
    NULL, NULL },

  END_OF_TESTCASES
};

