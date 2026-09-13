#ifndef BASE_CRASHDUMP_H
#define BASE_CRASHDUMP_H

/**
 * @defgroup Crash-Dumping Crash Dumping
 */

/**
 * Initializes the crash dumper and sets the filename to write the crash dump
 * to, if support for crash logging was compiled in. Otherwise does nothing.
 *
 * @ingroup Crash-Dumping
 *
 * @param log_file_path Absolute path to which crash log file should be written.
 */
void crashdump_init_if_available(const char *log_file_path);

/**
 * Records the graphics backend that the current session actually initialized
 * with, so crash reports can attribute the fault to the backend that was
 * really running instead of the one the config asked for.
 *
 * Safe to call again when the backend changes. Does nothing when crash
 * dumping is not compiled in or when the platform has no crash dumper.
 *
 * @ingroup Crash-Dumping
 *
 * @param backend_name Backend name, may be nullptr to clear it.
 */
void crashdump_set_graphics_backend(const char *backend_name);

#endif
