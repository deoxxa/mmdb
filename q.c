#include <jansson.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <string.h>
#include <yder.h>

#include "mmdb.h"
#include "q.h"

#ifdef DEBUG_SQL
#define DEBUG_SQL(a, ...) printf(a, __VA_ARGS__)
#else
#define DEBUG_SQL(a, ...)
#endif

int q_exec0(sqlite3 *db, const char *sql, const char *fmt, ...) {
  int failed = 0;
  sqlite3_stmt *stmt = NULL;
  va_list ap;

  DEBUG_SQL("q_exec0: start: %s\n", sql);

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    failed = 1;
    goto cleanup;
  }

  va_start(ap, fmt);
  if (q_bind_va(stmt, fmt, ap) != MMDB_OK) {
    failed = 1;
    goto cleanup;
  }
  va_end(ap);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    failed = 1;
    goto cleanup;
  }

cleanup:
  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    failed = 1;
  }

  DEBUG_SQL("q_exec0: complete (failed=%d)\n", failed);

  return failed ? MMDB_ERROR : MMDB_OK;
}

int q_exec1(sqlite3 *db, const char *sql, void *ptr, q_cb cb, const char *fmt,
            ...) {
  int failed = 0;
  sqlite3_stmt *stmt = NULL;
  va_list ap;

  DEBUG_SQL("q_exec1: start: %s\n", sql);

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    failed = 1;
    goto cleanup;
  }

  va_start(ap, fmt);
  if (q_bind_va(stmt, fmt, ap) != MMDB_OK) {
    failed = 1;
    goto cleanup;
  }
  va_end(ap);

  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      if (cb(stmt, ptr) != MMDB_OK) {
        failed = 1;
        goto cleanup;
      }
      break;
    case SQLITE_DONE:
      if (cb(NULL, ptr) != MMDB_OK) {
        failed = 1;
        goto cleanup;
      }
      break;
  }

cleanup:
  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    failed = 1;
  }

  DEBUG_SQL("q_exec1: complete (failed=%d)\n", failed);

  return failed ? MMDB_ERROR : MMDB_OK;
}

int q_exec2(sqlite3 *db, const char *sql, void *ptr, q_cb cb, const char *fmt,
            ...) {
  int failed = 0;
  sqlite3_stmt *stmt = NULL;
  va_list ap;

  DEBUG_SQL("q_exec2: start: %s\n", sql);

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    failed = 1;
    goto cleanup;
  }

  va_start(ap, fmt);
  if (q_bind_va(stmt, fmt, ap) != MMDB_OK) {
    failed = 1;
    goto cleanup;
  }
  va_end(ap);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (cb(stmt, ptr) != MMDB_OK) {
      failed = 1;
      goto cleanup;
    }
  }

cleanup:
  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    failed = 1;
  }

  DEBUG_SQL("q_exec2: complete (failed=%d)\n", failed);

  return failed ? MMDB_ERROR : MMDB_OK;
}

int q_bind(sqlite3_stmt *stmt, const char *fmt, ...) {
  int rc = 0;
  va_list ap;
  va_start(ap, fmt);
  rc = q_bind_va(stmt, fmt, ap);
  va_end(ap);
  return rc;
}

int q_bind_va(sqlite3_stmt *stmt, const char *fmt, va_list ap) {
  int i = 0;
  const char *in_s = NULL;

  while (*fmt) {
    i++;

    switch (*fmt++) {
      case 's':
        in_s = va_arg(ap, const char *);
        DEBUG_SQL("q_bind: %d string=%s\n", i, in_s);
        if (sqlite3_bind_text(stmt, i, in_s, strlen(in_s), NULL) != SQLITE_OK) {
          return MMDB_ERROR;
        }
        break;
      default:
        return MMDB_ERROR;
    }
  }

  return MMDB_OK;
}

int q_scan(sqlite3_stmt *stmt, const char *fmt, ...) {
  int rc = 0;
  va_list ap;
  va_start(ap, fmt);
  rc = q_scan_va(stmt, fmt, ap);
  va_end(ap);
  return rc;
}

int q_scan_va(sqlite3_stmt *stmt, const char *fmt, va_list ap) {
  int i = 0, len = 0;
  char *out_s = NULL;
  size_t out_len = 0;
  const char *ptr = NULL;

  i = 0;

  while (*fmt) {
    switch (*fmt++) {
      case 's':
        out_s = va_arg(ap, char *);
        out_len = va_arg(ap, size_t);

        ptr = sqlite3_column_text(stmt, i);
        len = sqlite3_column_bytes(stmt, i);

        if (len > out_len - 1) {
          return MMDB_ERROR;
        }

        strncpy(out_s, ptr, len);
        out_s[len] = 0;

        break;
      default:
        return MMDB_ERROR;
    }

    i++;
  }

  return MMDB_OK;
}
