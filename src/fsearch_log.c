#define G_LOG_DOMAIN "fsearch-log"

#include "fsearch_log.h"

#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Once the log grows beyond this size it's moved to fsearch.log.old and a new one is started
#define FSEARCH_LOG_MAX_SIZE (5 * 1024 * 1024)

static GMutex log_mutex;
static FILE *log_file = NULL;
static char *log_path = NULL;
static char *log_path_old = NULL;

static const char *
level_to_string(GLogLevelFlags level) {
    switch (level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
        return "ERROR";
    case G_LOG_LEVEL_CRITICAL:
        return "CRITICAL";
    case G_LOG_LEVEL_WARNING:
        return "WARNING";
    case G_LOG_LEVEL_MESSAGE:
        return "MESSAGE";
    case G_LOG_LEVEL_INFO:
        return "INFO";
    case G_LOG_LEVEL_DEBUG:
        return "DEBUG";
    default:
        return "LOG";
    }
}

static gboolean
is_fsearch_domain(const char *domain) {
    // FSearch's own code logs to domains named fsearch-*; messages without a domain are kept too
    return !domain || g_str_has_prefix(domain, "fsearch") || g_str_has_prefix(domain, "FSearch");
}

static void
open_log_file(void) {
    log_file = g_fopen(log_path, "a");
    if (log_file) {
        setvbuf(log_file, NULL, _IOLBF, 0);
    }
}

static void
rotate_if_needed(void) {
    if (!log_file || ftell(log_file) < FSEARCH_LOG_MAX_SIZE) {
        return;
    }
    fclose(log_file);
    g_rename(log_path, log_path_old);
    open_log_file();
}

static GLogWriterOutput
log_writer(GLogLevelFlags log_level, const GLogField *fields, gsize n_fields, gpointer user_data) {
    const char *domain = NULL;
    const char *message = NULL;
    for (gsize i = 0; i < n_fields; i++) {
        if (g_strcmp0(fields[i].key, "GLIB_DOMAIN") == 0) {
            domain = fields[i].value;
        }
        else if (g_strcmp0(fields[i].key, "MESSAGE") == 0) {
            message = fields[i].value;
        }
    }

    if (message && is_fsearch_domain(domain)) {
        g_autoptr(GDateTime) now = g_date_time_new_now_local();
        g_autofree char *timestamp = g_date_time_format(now, "%Y-%m-%d %H:%M:%S.%f");
        // %f gives microseconds, milliseconds are enough
        timestamp[strlen(timestamp) - 3] = '\0';

        g_mutex_lock(&log_mutex);
        if (log_file) {
            fprintf(log_file,
                    "%s [%d] %s %s: %s\n",
                    timestamp,
                    (int)getpid(),
                    level_to_string(log_level),
                    domain ? domain : "-",
                    message);
            rotate_if_needed();
        }
        g_mutex_unlock(&log_mutex);
    }

    // Keep the usual behaviour (stderr/journal, G_MESSAGES_DEBUG, fatal warnings, ...)
    return g_log_writer_default(log_level, fields, n_fields, user_data);
}

void
fsearch_log_init(void) {
    g_autofree char *dir = g_build_filename(g_get_user_state_dir(), "fsearch", NULL);
    g_mkdir_with_parents(dir, 0700);
    log_path = g_build_filename(dir, "fsearch.log", NULL);
    log_path_old = g_build_filename(dir, "fsearch.log.old", NULL);

    open_log_file();
    g_log_set_writer_func(log_writer, NULL, NULL);
}

const char *
fsearch_log_get_path(void) {
    return log_path;
}
