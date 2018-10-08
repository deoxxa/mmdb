#include "munit/munit.h"

extern MunitSuite mmdb_put_suite;
extern MunitSuite mmdb_rev_parse_suite;

int main(int argc, char* const argv[]) {
  MunitSuite suites[] = {mmdb_put_suite,
                         mmdb_rev_parse_suite,
                         {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE}};

  MunitSuite suite = {"/mmdb", NULL, suites, 1, MUNIT_SUITE_OPTION_NONE};

  return munit_suite_main(&suite, NULL, argc, argv);
}
