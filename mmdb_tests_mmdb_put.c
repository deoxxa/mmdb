#include <jansson.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <string.h>

#include "mmdb.h"
#include "q.h"

#include "munit/munit.h"

void mem_dump(char* text, void* ptr, size_t len) {
  int i;

  printf("%s\n", text);

  for (i = 0; i < len; i++) {
    printf("%02x", ((unsigned char*)ptr)[i]);
    if (i % 16 == 15) {
      printf("\n");
    } else if (i % 8 == 7) {
      printf("  ");
    } else {
      printf(" ");
    }
  }
  printf("\n");
}

MunitResult test_mmdb_put_new(const MunitParameter params[], void* p) {
  int rc;
  mmdb_t* db;
  mmdb_doc_t doc1, doc1a;
  mmdb_rev_t rev1;

  rc = mmdb_open(NULL, &db);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_doc_new(&doc1, "SpaghettiWithMeatballs", NULL, "{}");
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_put(db, &rev1, &doc1, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_get(db, &doc1a, "SpaghettiWithMeatballs");
  munit_assert_int(rc, ==, MMDB_OK);
  munit_assert_string_equal(doc1.id, doc1a.id);
  munit_assert_memory_equal(sizeof(rev1), &rev1, &doc1a.rev);

  return MUNIT_OK;
}

MunitResult test_mmdb_put_update(const MunitParameter params[], void* p) {
  int rc;
  mmdb_t* db;
  mmdb_doc_t doc1, doc1a, doc2;
  mmdb_rev_t rev1, rev2;

  rc = mmdb_open(NULL, &db);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_doc_new(&doc1, "SpaghettiWithMeatballs", NULL, "{}");
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_put(db, &rev1, &doc1, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_get(db, &doc1a, "SpaghettiWithMeatballs");
  munit_assert_int(rc, ==, MMDB_OK);
  munit_assert_string_equal(doc1.id, doc1a.id);
  munit_assert_memory_equal(sizeof(rev1), &rev1, &doc1a.rev);

  rc = mmdb_put(db, &rev2, &doc1a, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_get(db, &doc2, "SpaghettiWithMeatballs");
  munit_assert_int(rc, ==, MMDB_OK);
  munit_assert_string_equal(doc1a.id, doc2.id);
  munit_assert_memory_equal(sizeof(rev2), &rev2, &doc2.rev);

  return MUNIT_OK;
}

MunitResult test_mmdb_put_conflict_bad(const MunitParameter params[], void* p) {
  int rc;
  mmdb_t* db;
  mmdb_doc_t doc1, doc2a, doc2b;
  mmdb_rev_t rev1, rev2a, rev2b;
  mmdb_put_options_t opts = {.allow_conflict = 0};
  mmdb_revs_t revs;

  rc = mmdb_open(NULL, &db);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_doc_new(&doc1, "SpaghettiWithMeatballs", NULL, "{}");
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_put(db, &rev1, &doc1, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_doc_copy(&doc2a, &doc1);
  munit_assert_int(rc, ==, MMDB_OK);
  doc2a.rev = rev1;
  rc = json_object_set_new(doc2a.fields, "a", json_string("a"));
  munit_assert_int(rc, ==, 0);

  rc = mmdb_doc_copy(&doc2b, &doc1);
  munit_assert_int(rc, ==, MMDB_OK);
  doc2b.rev = rev1;
  rc = json_object_set_new(doc2b.fields, "b", json_string("b"));
  munit_assert_int(rc, ==, 0);

  rc = mmdb_put(db, &rev2a, &doc2a, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_put(db, &rev2b, &doc2b, NULL);
  munit_assert_int(rc, ==, MMDB_CONFLICT);

  rc = mmdb_revs_new(&revs);
  munit_assert_int(rc, ==, MMDB_OK);
  rc = mmdb_revs(db, &revs, "SpaghettiWithMeatballs");
  munit_assert_int(rc, ==, MMDB_OK);
  munit_assert_int(revs.total, ==, 1);

  return MUNIT_OK;
}

MunitResult test_mmdb_put_conflict_good(const MunitParameter params[],
                                        void* p) {
  int rc;
  mmdb_t* db;
  mmdb_doc_t doc1, doc2a, doc2b;
  mmdb_rev_t rev1, rev2a, rev2b;
  mmdb_put_options_t opts = {.allow_conflict = 1};
  mmdb_revs_t revs;

  rc = mmdb_open(NULL, &db);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_doc_new(&doc1, "SpaghettiWithMeatballs", NULL, "{}");
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_put(db, &rev1, &doc1, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_doc_copy(&doc2a, &doc1);
  munit_assert_int(rc, ==, MMDB_OK);
  doc2a.rev = rev1;
  rc = json_object_set_new(doc2a.fields, "a", json_string("a"));
  munit_assert_int(rc, ==, 0);

  rc = mmdb_doc_copy(&doc2b, &doc1);
  munit_assert_int(rc, ==, MMDB_OK);
  doc2b.rev = rev1;
  rc = json_object_set_new(doc2b.fields, "b", json_string("b"));
  munit_assert_int(rc, ==, 0);

  rc = mmdb_put(db, &rev2a, &doc2a, NULL);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_put(db, &rev2b, &doc2b, &opts);
  munit_assert_int(rc, ==, MMDB_OK);

  rc = mmdb_revs_new(&revs);
  munit_assert_int(rc, ==, MMDB_OK);
  rc = mmdb_revs(db, &revs, "SpaghettiWithMeatballs");
  munit_assert_int(rc, ==, MMDB_OK);
  munit_assert_int(revs.total, ==, 2);

  return MUNIT_OK;
}

static MunitTest mmdb_put_tests[] = {
    {"/new", test_mmdb_put_new, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/update", test_mmdb_put_update, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/conflict_bad", test_mmdb_put_conflict_bad, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/conflict_good", test_mmdb_put_conflict_good, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

MunitSuite mmdb_put_suite = {"/mmdb_put", mmdb_put_tests, NULL, 1,
                             MUNIT_SUITE_OPTION_NONE};
