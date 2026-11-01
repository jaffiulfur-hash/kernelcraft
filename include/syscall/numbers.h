#ifndef SYSCALL_NUMBERS_H
#define SYSCALL_NUMBERS_H

/* System call numbers, shared by the kernel and by user programs. */
#define SYSCALL_VECTOR 0x80

#define SYS_WRITE      0   /* (const char *buf, size_t len)  -> bytes written */
#define SYS_GETPID     1   /* ()                              -> thread/process id */
#define SYS_YIELD      2   /* ()                              -> 0 */
#define SYS_UPTIME_MS  3   /* ()                              -> milliseconds since boot */
#define SYS_SLEEP_MS   4   /* (uint64_t ms)                   -> 0 */
#define SYS_EXIT       5   /* (int code)                      -> does not return */
#define SYSCALL_COUNT  6

#endif
