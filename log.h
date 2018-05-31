/**
 * Control of log files
 */

#ifndef PUTTY_LOG_H
#define PUTTY_LOG_H

#define LOG_BUF_SIZE 300

typedef struct {
    int head;
    int tail;
    char rbuf[LOG_BUF_SIZE];
} log_buf;

typedef struct {
    int on_off;
    int fd;
    char *path;
    log_buf buf;
} log_file;

extern int start_log(log_file *);
extern int stop_log(log_file *);

#endif