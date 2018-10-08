#define MMDB_OK 0
#define MMDB_ERROR 1
#define MMDB_BUSY 2
#define MMDB_DONE 3
#define MMDB_NOT_FOUND 100
#define MMDB_CONFLICT 101

#define MMDB_MAX_ID_LENGTH 40
#define MMDB_MAX_REV_LENGTH 48
#define MMDB_MAX_DATA_LENGTH 1024 * 1024

typedef struct mmdb_s {
  int open;
  sqlite3 *db;
} mmdb_t;

typedef struct mmdb_rev_s {
  unsigned int seq;
  unsigned char hash[16];
} mmdb_rev_t;

typedef struct mmdb_doc_s {
  char id[MMDB_MAX_ID_LENGTH];
  mmdb_rev_t rev;
  json_t *fields;
} mmdb_doc_t;

typedef struct mmdb_revs_s {
  int total;
  const char **revs;
} mmdb_revs_t;

typedef struct mmdb_put_options_s {
  int allow_conflict;
} mmdb_put_options_t;

int mmdb_open(const char *filename, mmdb_t **db);
int mmdb_close(mmdb_t *db);
int mmdb_get(mmdb_t *db, mmdb_doc_t *out, const char *id);
int mmdb_get_rev(mmdb_t *db, mmdb_doc_t *out, const char *id, const char *rev);
int mmdb_revs(mmdb_t *db, mmdb_revs_t *out, const char *id);
int mmdb_put(mmdb_t *db, mmdb_rev_t *out_rev, mmdb_doc_t *doc,
             mmdb_put_options_t *opts);

int mmdb_rev_new(mmdb_rev_t *out, const char *str);
void mmdb_rev_clear(mmdb_rev_t *rev);
int mmdb_rev_copy(mmdb_rev_t *dst, mmdb_rev_t *src);
int mmdb_rev_parse(mmdb_rev_t *out, const char *str);
int mmdb_rev_nparse(mmdb_rev_t *out, const char *str, size_t len);
int mmdb_rev_format(char *out, size_t len, mmdb_rev_t *rev);
int mmdb_rev_cmp(mmdb_rev_t *a, mmdb_rev_t *b);
int mmdb_rev_next(mmdb_rev_t *out, mmdb_doc_t *doc);

int mmdb_doc_new(mmdb_doc_t *out, const char *id, const char *rev,
                 const char *fields);
void mmdb_doc_free(mmdb_doc_t *doc);
void mmdb_doc_clear(mmdb_doc_t *doc);
int mmdb_doc_copy(mmdb_doc_t *dst, mmdb_doc_t *src);
int mmdb_doc_set_id(mmdb_doc_t *doc, const char *id);
int mmdb_doc_set_rev(mmdb_doc_t *doc, const char *rev);
int mmdb_doc_set_fields(mmdb_doc_t *doc, json_t *v);
int mmdb_doc_set_fields_new(mmdb_doc_t *doc, json_t *v);
int mmdb_doc_set_fields_str(mmdb_doc_t *doc, const char *str);

int mmdb_revs_new(mmdb_revs_t * revs);
void mmdb_revs_free(mmdb_revs_t *revs);
void mmdb_revs_push(mmdb_revs_t *revs, const char *str);
void mmdb_revs_push_copyn(mmdb_revs_t *revs, const char *str, size_t n);
void mmdb_revs_push_copy(mmdb_revs_t *revs, const char *str);
