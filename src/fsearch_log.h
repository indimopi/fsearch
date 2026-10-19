#pragma once

#include <glib.h>

// Writes all of FSearch's own log messages (including debug output) to
// $XDG_STATE_HOME/fsearch/fsearch.log, in addition to the default log output.
void
fsearch_log_init(void);

// Path of the log file, owned by the log module
const char *
fsearch_log_get_path(void);
