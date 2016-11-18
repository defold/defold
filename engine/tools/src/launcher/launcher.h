#ifndef DM_LAUNCHER_H
#define DM_LAUNCHER_H

#ifdef _WIN32

/**
 * Creates a properly quoted CreateProcess lpCommandLine from argv.
 * Exposed for testing purposes.
 */

void QuoteArgv(const char* argv[], char* cmd_line);

#endif

#endif
