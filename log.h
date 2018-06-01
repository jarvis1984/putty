/**
 * Control of log files
 */

#ifndef PUTTY_LOG_H
#define PUTTY_LOG_H

#define LOG_BUF_SIZE 500

typedef struct {
    int head;
    int tail;
    char space[LOG_BUF_SIZE];
} log_buf;

typedef struct {
    int on_off;
    int fd;
    int filt;
    char *path;
    char *filt_key;
    log_buf buf;
} log_file;

extern int start_log(log_file *);
extern int stop_log(log_file *);
extern int add_log(log_file *, const char *, int);
int init_log_thread(log_file *);
int close_log_thread(void);

#endif