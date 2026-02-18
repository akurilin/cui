#ifndef FAIL_FAST_H
#define FAIL_FAST_H

/*
 * Abort the process after logging a critical formatted message.
 *
 * Purpose:
 * - Provide a single fail-fast utility for unrecoverable internal errors.
 *
 * Behavior/contract:
 * - Logs using SDL's application logger at critical priority.
 * - Always terminates the process via abort().
 *
 * Parameters:
 * - `format`: printf-style format string (must be non-NULL).
 * - `...`: format arguments consumed according to `format`.
 *
 * Return value:
 * - This function never returns.
 *
 * Ownership/lifecycle:
 * - Stateless utility; no owned resources.
 */
_Noreturn void fail_fast(const char *format, ...);

#endif
