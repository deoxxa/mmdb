typedef int (*q_cb)(sqlite3_stmt *stmt, void *ptr);

int q_exec0(sqlite3 *db, const char *sql, const char *fmt, ...);
int q_exec1(sqlite3 *db, const char *sql, void *ptr, q_cb cb, const char *fmt, ...);
int q_exec2(sqlite3 *db, const char *sql, void *ptr, q_cb cb, const char *fmt, ...);

int q_bind(sqlite3_stmt *stmt, const char *fmt, ...);
int q_bind_va(sqlite3_stmt *stmt, const char *fmt, va_list ap);
int q_scan(sqlite3_stmt *stmt, const char *fmt, ...);
int q_scan_va(sqlite3_stmt *stmt, const char *fmt, va_list ap);
