#include <jansson.h>
#include <openssl/md5.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <string.h>

#include "mmdb.h"
#include "q.h"

#define MMDB_MIN(a, b) ((a < b) ? a : b)

int mmdb_put_new(mmdb_t *db, mmdb_rev_t *out_rev, mmdb_doc_t *doc,
                 mmdb_put_options_t *opts);
int mmdb_put_update(mmdb_t *db, mmdb_rev_t *out_rev, mmdb_doc_t *doc,
                    mmdb_rev_t *current_rev, mmdb_put_options_t *opts);

const char query_init[] =
    "create table if not exists meta (rev text not null);"
    "create table if not exists docs (id text not null, rev blob not null);"
    "create table if not exists revs (id text not null, rev blob not null, "
    "doc blob, leaf integer not null default 1, deleted integer not null "
    "default 0);";

const char query_get[] =
    "select r.id, r.rev, r.doc from docs d left join revs r on r.id = d.id and "
    "r.rev = d.rev where d.id = $1";

const char query_get_rev[] =
    "select id, rev, doc from revs where id = $1 and rev = $2";

const char query_revs[] =
    "select rev from revs where id = $1 and leaf = 1 and deleted = 0";

const char query_rev[] = "select rev from docs where id = $1";

const char query_insert_rev[] =
    "insert into revs (id, rev, doc) values ($1, $2, $3);";

const char query_insert_doc[] = "insert into docs (id, rev) values ($1, $2);";

const char query_remove_leaf[] =
    "update revs set leaf = 0 where id = $1 and rev = $2";

const char query_update_doc[] = "update docs set rev = $1 where id = $2";

int mmdb_open(const char *filename, mmdb_t **db) {
  mmdb_t *r = NULL;

  r = malloc(sizeof(mmdb_t));
  memset(r, 0, sizeof(mmdb_t));

  if (sqlite3_open(filename, &r->db) != SQLITE_OK) {
    return MMDB_ERROR;
  }

  if (sqlite3_exec(r->db, query_init, NULL, NULL, NULL) != SQLITE_OK) {
    return MMDB_ERROR;
  }

  *db = r;

  return MMDB_OK;
}

int mmdb_close(mmdb_t *db) {
  if (db == NULL) {
    return MMDB_OK;
  }

  if (db->open == 0) {
    return MMDB_OK;
  }

  switch (sqlite3_close(db->db)) {
    case SQLITE_BUSY:
      return MMDB_BUSY;
    case SQLITE_OK:
      return MMDB_OK;
    default:
      return MMDB_ERROR;
  }
}

int mmdb_get_cb(sqlite3_stmt *stmt, void *ptr) {
  mmdb_doc_t *out = ptr;
  char id[MMDB_MAX_ID_LENGTH], rev[MMDB_MAX_REV_LENGTH],
      fields[MMDB_MAX_DATA_LENGTH];

  if (stmt == NULL) {
    mmdb_doc_clear(out);
    return MMDB_OK;
  }

  if (q_scan(stmt, "sss", id, sizeof(id), rev, sizeof(rev), fields,
             sizeof(fields)) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_doc_new(out, id, rev, fields) != MMDB_OK) {
    mmdb_doc_clear(out);
    return MMDB_ERROR;
  }

  return MMDB_OK;
}

int mmdb_get(mmdb_t *db, mmdb_doc_t *out, const char *id) {
  return q_exec1(db->db, query_get, out, mmdb_get_cb, "s", id);
}

int mmdb_get_rev(mmdb_t *db, mmdb_doc_t *out, const char *id, const char *rev) {
  return q_exec1(db->db, query_get_rev, out, mmdb_get_cb, "ss", id, rev);
}

int mmdb_revs_cb(sqlite3_stmt *stmt, void *ptr) {
  mmdb_revs_t *out = ptr;
  const char rev[MMDB_MAX_REV_LENGTH];

  if (q_scan(stmt, "s", rev, sizeof(rev)) != MMDB_OK) {
    return MMDB_ERROR;
  }

  mmdb_revs_push_copyn(out, rev, sizeof(rev));

  return MMDB_OK;
}

int mmdb_revs(mmdb_t *db, mmdb_revs_t *out, const char *id) {
  return q_exec2(db->db, query_revs, out, mmdb_revs_cb, "s", id);
}

int mmdb_insert_doc(mmdb_t *db, const char *id, const char *rev) {
  return q_exec0(db->db, query_insert_doc, "ss", id, rev);
}

int mmdb_insert_rev(mmdb_t *db, const char *id, const char *rev,
                    const char *fields) {
  return q_exec0(db->db, query_insert_rev, "sss", id, rev, fields);
}

int mmdb_remove_leaf(mmdb_t *db, const char *id, const char *rev) {
  return q_exec0(db->db, query_remove_leaf, "ss", id, rev);
}

int mmdb_update_doc(mmdb_t *db, const char *id, const char *rev) {
  return q_exec0(db->db, query_update_doc, "ss", rev, id);
}

int mmdb_put_cb(sqlite3_stmt *stmt, void *ptr) {
  char str[MMDB_MAX_REV_LENGTH];
  mmdb_rev_t *current_rev = ptr;

  if (stmt == NULL) {
    mmdb_rev_clear(current_rev);
    return MMDB_OK;
  }

  if (q_scan(stmt, "s", str, sizeof(str)) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_rev_new(current_rev, str) != MMDB_OK) {
    return MMDB_ERROR;
  }

  return MMDB_OK;
}

int mmdb_put(mmdb_t *db, mmdb_rev_t *out_rev, mmdb_doc_t *doc,
             mmdb_put_options_t *opts) {
  mmdb_rev_t current_rev;

  if (q_exec1(db->db, query_rev, &current_rev, mmdb_put_cb, "s", doc->id) !=
      MMDB_OK) {
    return MMDB_ERROR;
  }

  if (current_rev.seq == 0) {
    return mmdb_put_new(db, out_rev, doc, opts);
  }

  return mmdb_put_update(db, out_rev, doc, &current_rev, opts);
}

int mmdb_put_new(mmdb_t *db, mmdb_rev_t *out_rev, mmdb_doc_t *doc,
                 mmdb_put_options_t *opts) {
  char rev[MMDB_MAX_REV_LENGTH], fields[MMDB_MAX_DATA_LENGTH];
  size_t n;

  if (mmdb_rev_next(out_rev, doc) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_rev_format(rev, sizeof(rev), out_rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if ((n = json_dumpb(doc->fields, fields, sizeof(fields) - 1,
                      JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS)) >
      sizeof(fields) - 1) {
    return MMDB_ERROR;
  }
  fields[n] = 0;

  if (mmdb_insert_rev(db, doc->id, rev, fields) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_insert_doc(db, doc->id, rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  return MMDB_OK;
}

int mmdb_put_update(mmdb_t *db, mmdb_rev_t *out_rev, mmdb_doc_t *doc,
                    mmdb_rev_t *current_rev, mmdb_put_options_t *opts) {
  int cmp = 0;
  char rev[MMDB_MAX_REV_LENGTH], old_rev[MMDB_MAX_REV_LENGTH],
      fields[MMDB_MAX_DATA_LENGTH];
  size_t n;

  cmp = mmdb_rev_cmp(&doc->rev, current_rev);

  if (cmp != 0 && (opts == NULL || opts->allow_conflict == 0)) {
    return MMDB_CONFLICT;
  }

  if (mmdb_rev_next(out_rev, doc) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_rev_format(rev, sizeof(rev), out_rev) != MMDB_OK) {
    return MMDB_ERROR;
  }
  if (mmdb_rev_format(old_rev, sizeof(old_rev), current_rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if ((n = json_dumpb(doc->fields, fields, sizeof(fields) - 1,
                      JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS)) >
      sizeof(fields) - 1) {
    return MMDB_ERROR;
  }
  fields[n] = 0;

  if (mmdb_insert_rev(db, doc->id, rev, fields) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (cmp == 0 && mmdb_remove_leaf(db, doc->id, rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (cmp >= 0 && mmdb_update_doc(db, doc->id, rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  return MMDB_OK;
}

int mmdb_doc_new(mmdb_doc_t *out, const char *id, const char *rev,
                 const char *fields) {
  if (mmdb_doc_set_id(out, id) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_doc_set_rev(out, rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if (mmdb_doc_set_fields_str(out, fields) != MMDB_OK) {
    return MMDB_ERROR;
  }

  return MMDB_OK;
}

void mmdb_doc_free(mmdb_doc_t *doc) {
  if (doc == NULL) return;
  mmdb_doc_clear(doc);
  free(doc);
}

void mmdb_doc_clear(mmdb_doc_t *doc) {
  memset(doc->id, 0, sizeof(doc->id));
  mmdb_rev_clear(&doc->rev);
  json_decref(doc->fields);
  doc->fields = NULL;
}

int mmdb_doc_copy(mmdb_doc_t *dst, mmdb_doc_t *src) {
  mmdb_doc_clear(dst);
  memcpy(dst->id, src->id, sizeof(dst->id));
  mmdb_rev_copy(&dst->rev, &src->rev);
  mmdb_doc_set_fields_new(dst, json_deep_copy(src->fields));
  return MMDB_OK;
}

int mmdb_doc_set_id(mmdb_doc_t *doc, const char *id) {
  if (strlen(id) > MMDB_MAX_ID_LENGTH) return MMDB_ERROR;
  memset(doc->id, 0, sizeof(doc->id));
  strncpy(doc->id, id, MMDB_MAX_ID_LENGTH);
  return MMDB_OK;
}

int mmdb_doc_nset_id(mmdb_doc_t *doc, const char *id, size_t len) {
  if (len > MMDB_MAX_ID_LENGTH) return MMDB_ERROR;
  memset(doc->id, 0, sizeof(doc->id));
  strncpy(doc->id, id, MMDB_MIN(len, MMDB_MAX_ID_LENGTH));
  return MMDB_OK;
}

int mmdb_doc_set_rev(mmdb_doc_t *doc, const char *rev) {
  return mmdb_rev_parse(&doc->rev, rev);
}

int mmdb_doc_nset_rev(mmdb_doc_t *doc, const char *rev, size_t len) {
  return mmdb_rev_nparse(&doc->rev, rev, len);
}

int mmdb_doc_set_fields(mmdb_doc_t *doc, json_t *v) {
  return mmdb_doc_set_fields_new(doc, json_incref(v));
}

int mmdb_doc_set_fields_new(mmdb_doc_t *doc, json_t *v) {
  if (doc->fields != NULL) {
    json_decref(doc->fields);
    doc->fields = NULL;
  }

  doc->fields = v;

  return MMDB_OK;
}

int mmdb_doc_set_fields_str(mmdb_doc_t *doc, const char *str) {
  json_t *v;
  json_error_t err;

  if ((v = json_loads(str, 0, &err)) == NULL) {
    return MMDB_ERROR;
  }

  if (mmdb_doc_set_fields_new(doc, v) != MMDB_OK) {
    json_decref(v);
    v = NULL;

    return MMDB_ERROR;
  }

  return MMDB_OK;
}

int mmdb_revs_new(mmdb_revs_t *revs) {
  memset(revs, 0, sizeof(mmdb_revs_t));
  return MMDB_OK;
}

void mmdb_revs_free(mmdb_revs_t *revs) {
  if (revs == NULL) {
    return;
  }

  while (revs->total > 0) {
    free((char *)(revs->revs[revs->total - 1]));
    revs->total--;
  }

  free(revs->revs);

  free(revs);
}

void mmdb_revs_push(mmdb_revs_t *revs, char const *str) {
  revs->total++;
  revs->revs = realloc(revs->revs, sizeof(char const *) * revs->total);
  revs->revs[revs->total - 1] = str;
}

void mmdb_revs_push_copyn(mmdb_revs_t *revs, char const *str, size_t n) {
  mmdb_revs_push(revs, strndup(str, n));
}

void mmdb_revs_push_copy(mmdb_revs_t *revs, char const *str) {
  mmdb_revs_push(revs, strdup(str));
}

int mmdb_rev_new(mmdb_rev_t *out, const char *str) {
  return mmdb_rev_parse(out, str);
}

void mmdb_rev_clear(mmdb_rev_t *rev) {
  rev->seq = 0;
  memset(rev->hash, 0, sizeof(rev->hash));
}

int mmdb_rev_copy(mmdb_rev_t *dst, mmdb_rev_t *src) {
  dst->seq = src->seq;
  memcpy(dst->hash, src->hash, sizeof(dst->hash));
  return MMDB_OK;
}

int mmdb_rev_parse(mmdb_rev_t *out, const char *str) {
  if (str == NULL || strlen(str) == 0) {
    mmdb_rev_clear(out);
    return MMDB_OK;
  }

  return mmdb_rev_nparse(out, str, strlen(str));
}

int mmdb_rev_nparse(mmdb_rev_t *out, const char *str, size_t len) {
  char *p = NULL;
  int i = 0, n = 0;

  out->seq = strtoul(str, &p, 10);
  if (p == str) {
    return MMDB_ERROR;
  }

  p++;

  if (strlen(p) != sizeof(out->hash) * 2) {
    return MMDB_ERROR;
  }

  for (i = 0; i < 16; i++) {
    sscanf(&(p[i * 2]), "%2hhx", &(out->hash[i]));
  }

  return MMDB_OK;
}

int mmdb_rev_format(char *out, size_t n, mmdb_rev_t *rev) {
  int i, l;

  memset(out, 0, n);

  if (rev->seq == 0) {
    return MMDB_OK;
  }

  l = snprintf(out, n, "%u-", rev->seq);

  if (n < (sizeof(rev->hash) * 2 + l)) {
    return MMDB_ERROR;
  }

  for (i = 0; i < 16; i++) {
    sprintf(&(out[l + i * 2]), "%02x", rev->hash[i]);
  }

  return MMDB_OK;
}

int mmdb_rev_cmp(mmdb_rev_t *a, mmdb_rev_t *b) {
  if (a->seq > b->seq) {
    return -1;
  }

  if (a->seq < b->seq) {
    return 1;
  }

  return memcmp(a->hash, b->hash, sizeof(a->hash));
}

int mmdb_rev_next(mmdb_rev_t *out, mmdb_doc_t *doc) {
  char rev[MMDB_MAX_REV_LENGTH], fields[MMDB_MAX_DATA_LENGTH];
  size_t len;
  MD5_CTX md5;
  unsigned char digest[16];

  if (mmdb_rev_format(rev, sizeof(rev), &doc->rev) != MMDB_OK) {
    return MMDB_ERROR;
  }

  if ((len = json_dumpb(doc->fields, fields, sizeof(fields),
                        JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS)) >
      MMDB_MAX_DATA_LENGTH) {
    return MMDB_ERROR;
  }

  if (MD5_Init(&md5) == 0) {
    return MMDB_ERROR;
  }
  if (MD5_Update(&md5, doc->id, MMDB_MIN(strlen(doc->id), sizeof(doc->id))) ==
      0) {
    return MMDB_ERROR;
  }
  if (MD5_Update(&md5, ",", 1) == 0) {
    return MMDB_ERROR;
  }
  if (MD5_Update(&md5, rev, MMDB_MIN(strlen(rev), sizeof(rev))) == 0) {
    return MMDB_ERROR;
  }
  if (MD5_Update(&md5, ",", 1) == 0) {
    return MMDB_ERROR;
  }
  if (MD5_Update(&md5, fields, len) == 0) {
    return MMDB_ERROR;
  }
  if (MD5_Final(out->hash, &md5) == 0) {
    return MMDB_ERROR;
  }

  out->seq = doc->rev.seq + 1;

  return MMDB_OK;
}
