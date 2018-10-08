#include <jansson.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yder.h>

#include "mmdb.h"

#define MAX_TOKENS 10

void usage(const char *cmd) {
  fprintf(stderr,
          "Usage: %s [-d file.db] [-p port] [-b address] [-l "
          "none|error|warning|info|debug]\n",
          cmd);
}

int main(int argc, char **argv) {
  int opt, rc, i;
  unsigned short port;
  char *bind;
  unsigned long log_level;
  char *db_file;
  mmdb_t *db;
  char line[1000 + MMDB_MAX_DATA_LENGTH];
  char *tokens[MAX_TOKENS+1], *token, *token_ptr;

  opt = 0;
  rc = 0;
  port = 5000;
  bind = "127.0.0.1";
  log_level = Y_LOG_LEVEL_WARNING;
  db_file = NULL;
  db = NULL;
  memset(line, 0, sizeof(line));
  memset(tokens, 0, sizeof(tokens));

  while ((opt = getopt(argc, argv, "l:d:p:b:")) != -1) {
    switch (opt) {
      case 'l':
        if (strcmp(optarg, "none") == 0) {
          log_level = Y_LOG_LEVEL_NONE;
        } else if (strcmp(optarg, "error") == 0) {
          log_level = Y_LOG_LEVEL_ERROR;
        } else if (strcmp(optarg, "warning") == 0) {
          log_level = Y_LOG_LEVEL_WARNING;
        } else if (strcmp(optarg, "info") == 0) {
          log_level = Y_LOG_LEVEL_INFO;
        } else if (strcmp(optarg, "debug") == 0) {
          log_level = Y_LOG_LEVEL_DEBUG;
        } else {
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
      case 'd':
        db_file = strdup(optarg);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'b':
        bind = strdup(optarg);
        break;
      default:
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if ((rc = y_init_logs("mmdb", Y_LOG_MODE_CONSOLE, log_level, NULL, NULL)) ==
      0) {
    return 1;
  }

  y_log_message(Y_LOG_LEVEL_DEBUG, "opening database");
  if ((rc = mmdb_open(db_file, &db)) != MMDB_OK) {
    y_log_message(Y_LOG_LEVEL_ERROR, "couldn't open database: rc=%d", rc);
    return 1;
  }
  y_log_message(Y_LOG_LEVEL_DEBUG, "opened database");

  y_log_message(Y_LOG_LEVEL_DEBUG, "closing database");
  while ((rc = mmdb_close(db)) == MMDB_BUSY) {
    y_log_message(Y_LOG_LEVEL_DEBUG, "database is busy while closing; waiting");
    usleep(1000);
  }
  if (rc != MMDB_OK) {
    y_log_message(Y_LOG_LEVEL_ERROR, "couldn't close database: rc=%d", rc);
    return 1;
  }
  y_log_message(Y_LOG_LEVEL_DEBUG, "closed database");

  while (fgets(line, sizeof(line), stdin) != NULL) {
    printf("got line len=%ld\n", strlen(line));

    for (token = &(line[0]); token != NULL; strsep(&token, " ")) {
      tokens[i++] = token;

      if (i >= MAX_TOKENS) {
        break;
      }
    }
    tokens[i] = NULL;

    printf("token count: %d\n", i);

    for (i = 0; i < MAX_TOKENS; i++) {
      printf("token %d: %s\n", i, tokens[i]);
    }
  }

  return 0;
}
