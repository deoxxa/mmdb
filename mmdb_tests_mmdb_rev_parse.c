#include <jansson.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <string.h>

#include "mmdb.h"
#include "q.h"

#include "munit/munit.h"

static char* mmdb_rev_good_params_input[] = {
    "1-12340000000000000000000000001234",
    "2-12340000000000005555000000001234",
    "3-12340000000066660000000000001234",
    "999-12340000000000000000000000001234",
    "10001-12340000123400001234000000001234",
    "1-ffffeeeeddddcccc0000000000001234",
    "1-0000000000000000000000000000000f",
    "1-000000000000000000000000000000f0",
    "1-00000000000000000000000000000000",
    "1-ffffffffffffffffffffffffffffffff",
    "4294967295-ffffffffffffffffffffffffffffffff",
    NULL};

static MunitParameterEnum mmdb_rev_good_params[] = {
    {"input", mmdb_rev_good_params_input},
    {NULL, NULL},
};

MunitResult test_mmdb_rev_parse_good(const MunitParameter params[], void* p) {
  int rc;
  mmdb_rev_t rev;
  char out[MMDB_MAX_REV_LENGTH];

  rc = mmdb_rev_parse(&rev, params[0].value);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_rev_format(out, sizeof(out), &rev);
  munit_assert_int(rc, ==, MMDB_OK);

  munit_assert_string_equal(params[0].value, out);

  return MUNIT_OK;
}

static char* mmdb_rev_bad_params_input[] = {
    "1-aaa", "asdf-00000000000000000000000000000000", NULL};

static MunitParameterEnum mmdb_rev_bad_params[] = {
    {"input", mmdb_rev_bad_params_input},
    {NULL, NULL},
};

MunitResult test_mmdb_rev_parse_bad(const MunitParameter params[], void* p) {
  int rc;
  mmdb_rev_t rev;

  rc = mmdb_rev_parse(&rev, params[0].value);
  munit_assert_int(rc, ==, MMDB_ERROR);

  return MUNIT_OK;
}

static char* mmdb_rev_weird_params_input[] = {
    "01-12340000000000000000000000001234", NULL};

static MunitParameterEnum mmdb_rev_weird_params[] = {
    {"input", mmdb_rev_weird_params_input},
    {NULL, NULL},
};

MunitResult test_mmdb_rev_parse_weird(const MunitParameter params[], void* p) {
  int rc;
  mmdb_rev_t rev;
  char out[MMDB_MAX_REV_LENGTH];

  rc = mmdb_rev_parse(&rev, params[0].value);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_rev_format(out, sizeof(out), &rev);
  munit_assert_int(rc, ==, MMDB_OK);

  munit_assert_string_not_equal(params[0].value, out);

  return MUNIT_OK;
}

static MunitTest mmdb_rev_parse_tests[] = {
    {"/good", test_mmdb_rev_parse_good, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     mmdb_rev_good_params},
    {"/bad", test_mmdb_rev_parse_bad, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     mmdb_rev_bad_params},
    {"/weird", test_mmdb_rev_parse_weird, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     mmdb_rev_weird_params},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

MunitSuite mmdb_rev_parse_suite = {"/mmdb_rev_parse", mmdb_rev_parse_tests,
                                   NULL, 1, MUNIT_SUITE_OPTION_NONE};
