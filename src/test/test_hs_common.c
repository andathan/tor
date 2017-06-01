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
#include "config.h"

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

static void
test_start_time_of_next_time_period(void *arg)
{
  (void) arg;
  int retval;
  time_t fake_time;
  char tbuf[ISO_TIME_LEN + 1];
  time_t next_tp_start_time;

  /* Do some basic tests */
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 11:00:00 UTC",
                              &fake_time);
  tt_int_op(retval, ==, 0);
  next_tp_start_time = hs_get_start_time_of_next_time_period(fake_time);
  /* Compare it with the correct result */
  format_iso_time(tbuf, next_tp_start_time);
  tt_str_op("2016-04-13 12:00:00", OP_EQ, tbuf);

  /* Another test with an edge-case time (start of TP) */
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 12:00:00 UTC",
                              &fake_time);
  tt_int_op(retval, ==, 0);
  next_tp_start_time = hs_get_start_time_of_next_time_period(fake_time);
  format_iso_time(tbuf, next_tp_start_time);
  tt_str_op("2016-04-14 12:00:00", OP_EQ, tbuf);

  {
    /* Now pretend we are on a testing network and alter the voting schedule to
       be every 10 seconds. This means that a time period has length 10*24
       seconds (4 minutes). It also means that we apply a rotational offset of
       120 seconds to the time period, so that it starts at 00:02:00 instead of
       00:00:00. */
    or_options_t *options = get_options_mutable();
    options->TestingTorNetwork = 1;
    options->V3AuthVotingInterval = 10;
    options->TestingV3AuthInitialVotingInterval = 10;

    retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:00:00 UTC",
                                &fake_time);
    tt_int_op(retval, ==, 0);
    next_tp_start_time = hs_get_start_time_of_next_time_period(fake_time);
    /* Compare it with the correct result */
    format_iso_time(tbuf, next_tp_start_time);
    tt_str_op("2016-04-13 00:02:00", OP_EQ, tbuf);

    retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:02:00 UTC",
                                &fake_time);
    tt_int_op(retval, ==, 0);
    next_tp_start_time = hs_get_start_time_of_next_time_period(fake_time);
    /* Compare it with the correct result */
    format_iso_time(tbuf, next_tp_start_time);
    tt_str_op("2016-04-13 00:06:00", OP_EQ, tbuf);
  }

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

  /* First try with a consensus just inside the overlap period */
  dummy_consensus = tor_malloc_zero(sizeof(networkstatus_t));
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:00:00 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);

  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  /* Now increase the valid_after so that it goes to 11:00:00 UTC. Overlap
     period is still active. */
  dummy_consensus->valid_after += 3600*11;
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

/* Test the overlap period functions on a testnet with altered voting
 * schedule */
static void
test_desc_overlap_period_testnet(void *arg)
{
  int retval;
  time_t now = approx_time();
  networkstatus_t *dummy_consensus = NULL;
  or_options_t *options = get_options_mutable();

  (void) arg;

  /* Set the testnet option and a 10-second voting interval */
  options->TestingTorNetwork = 1;
  options->V3AuthVotingInterval = 10;
  options->TestingV3AuthInitialVotingInterval = 10;

  dummy_consensus = tor_malloc_zero(sizeof(networkstatus_t));

  /* A 10-second voting interval means that the lengths of an SRV run and of a
   * time period are both 10*24 seconds (4 minutes). The SRV gets published at
   * 00:00:00 and the TP starts at 00:02:00 (rotation offset: 2 mins). Those
   * two minutes between SRV publish and TP start is the overlap period
   * window. Let's test it: */
  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:00:00 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:01:59 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:02:00 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 0);

  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:04:00 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:05:59 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 1);

  retval = parse_rfc1123_time("Wed, 13 Apr 2016 00:06:00 UTC",
                              &dummy_consensus->valid_after);
  tt_int_op(retval, ==, 0);
  retval = hs_overlap_mode_is_active(dummy_consensus, now);
  tt_int_op(retval, ==, 0);

 done:
  tor_free(dummy_consensus);
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
  { "start_time_of_next_time_period", test_start_time_of_next_time_period,
    TT_FORK, NULL, NULL },
  { "desc_overlap_period", test_desc_overlap_period, TT_FORK,
    NULL, NULL },
  { "desc_overlap_period_testnet", test_desc_overlap_period_testnet, TT_FORK,
    NULL, NULL },
  { "parse_extended_hostname", test_parse_extended_hostname, TT_FORK,
    NULL, NULL },

  END_OF_TESTCASES
};

