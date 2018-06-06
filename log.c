/**
 * Operation of log files
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "puttymem.h"
#include "putty.h"
#include "log.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int isDir(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

int start_log(log_file *log)
{
    if (!log->on_off)
    {
        if (isDir(log->path))
            return -1;

        log->fd = open(log->path, O_CREAT|O_APPEND|O_WRONLY, S_IWOTH|S_IWGRP);

        if (log->fd < 0)
            return -1;

        log->buf.head = 0;
        log->buf.tail = 0;
        log->line = 0;
        log->on_off = 1;
    }

    return 0;
}

int flush_all(log_file *log)
{
    if (log->fd < 0 || log->buf.head == log->buf.tail)
        return 0;
        
    pthread_mutex_lock(&mutex);

    if (log->buf.head < log->buf.tail)
    {
        int len = log->buf.tail - log->buf.head;
        write(log->fd, &log->buf.space[log->buf.head], len);
        log->buf.head += len;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;
    }
    else if (log->buf.head > log->buf.tail)
    {
        int len = LOG_BUF_SIZE - log->buf.head;
        write(log->fd, &log->buf.space[log->buf.head], len);
        log->buf.head += len;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;

        len = log->buf.tail - log->buf.head;
        write(log->fd, &log->buf.space[log->buf.head], len);
        log->buf.head += len;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;
    }

    pthread_mutex_unlock(&mutex);
    
    return 0;
}

int stop_log(log_file *log)
{
    if (log->on_off)
    {
        flush_all(log);
        close(log->fd);
        sfree(log->path);
        sfree(log->filt_key);
        log->on_off = 0;
    }

    return 0;
}

static inline int str_cmp(char *src, char *mask)
{
    if (*src == '\n')
        return 0;
    else if (*mask == '\0')
        return 1;
    else if (*src == *mask && str_cmp(src+1, mask+1))
        return 1;

    return 0;
}

int if_contain(char *src, char *mask)
{
    if (*src == '\n')
        return 0;
    else if (*mask == '\0')
        return 1;
    else if (*src == *mask && str_cmp(src+1, mask+1))
        return 1;

    return if_contain(src+1, mask);
}

char *get_line(int start, int end, log_file *log)
{
    int len = 0;

    if (end > start)
        len = end -start;
    else
    {
        len = LOG_BUF_SIZE - start;
        len += end;
    }

    int i = 0;
    char *str = snewn(len, char);

    for (i = 0; i < len && start+i < LOG_BUF_SIZE; ++i)
    {
        str[i] = log->buf.space[start+i];
    }

    for (; i < len && start+i >= LOG_BUF_SIZE; ++i)
    {
        str[i] = log->buf.space[start+i-LOG_BUF_SIZE];
    }

    return str;
}

int add_log(log_file *log, const char *data, int len)
{
    int i = 0;

    if (!log->on_off)
        return -1;

    pthread_mutex_lock(&mutex);

    for (i = 0; i < len; ++i)
    {
        if (log->buf.tail+1 == log->buf.head)
            break;
        else if (log->buf.tail+1 == LOG_BUF_SIZE && log->buf.head == 0)
            break;

        log->buf.space[log->buf.tail++] = data[i];

        if (data[i] == '\n')
        {
            if (log->line == 0)
            {
                char *str = get_line(log->buf.head, log->buf.tail, log);
                if ((log->filt == FILTER_INCLUDE && log->key_len > 0 && if_contain(str, log->filt_key)) ||
                    (log->filt == FILTER_EXCLUDE && log->key_len > 0 && !if_contain(str, log->filt_key))) 
                {
                    log->buf.tail = log->buf.head;
                    log->line = 0;
                }
                else
                {
                    log->buf.carriage[log->line] = log->buf.tail;
                    log->line++;
                }
                sfree(str);
            }
            else
            {
                char *str = get_line(log->buf.carriage[log->line-1], log->buf.tail, log);
                if ((log->filt == FILTER_INCLUDE && log->key_len > 0 && if_contain(str, log->filt_key)) ||
                    (log->filt == FILTER_EXCLUDE && log->key_len > 0 && !if_contain(str, log->filt_key)))
                {
                    log->buf.tail = log->buf.carriage[log->line-1];
                }
                else
                {
                    log->buf.carriage[log->line] = log->buf.tail;
                    log->line++;
                }
                sfree(str);
            }
        }

        if (log->buf.tail == LOG_BUF_SIZE)
            log->buf.tail = 0;
    }

    pthread_mutex_unlock(&mutex);

    if (i < len)
        return -1;

    return 0;
}

int consume_log(log_file *log)
{
    if (!log->on_off || log->fd < 0)
        return -1;

    if (log->line == 0)
        return 0;
    else if (log->buf.head < log->buf.carriage[log->line-1])
    {
        pthread_mutex_lock(&mutex);

        int len = log->buf.carriage[log->line-1] - log->buf.head;
        int size = write(log->fd, &log->buf.space[log->buf.head], len);

        if (size > 0)
            log->buf.head += size;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;
        
        log->line = 0;
        pthread_mutex_unlock(&mutex);
    }
    else if (log->buf.head > log->buf.carriage[log->line-1])
    {
        pthread_mutex_lock(&mutex);

        int len = LOG_BUF_SIZE - log->buf.head;
        int size = write(log->fd, &log->buf.space[log->buf.head], len);

        if (size > 0)
            log->buf.head += size;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;

        len = log->buf.carriage[log->line-1] - log->buf.head;
        size = write(log->fd, &log->buf.space[log->buf.head], len);

        if (size > 0)
            log->buf.head += size;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;

        log->line = 0;
        pthread_mutex_unlock(&mutex);
    }

    return 0;
}

void *log_loop(void *arg)
{
    while (1)
    {
        consume_log((log_file *)arg);
        sleep(3);
    }
}

static pthread_t log_thread;

int init_log_thread(log_file *log)
{
    return pthread_create(&log_thread, NULL, log_loop, log);
}

int close_log_thread(log_file *log)
{
    pthread_cancel(log_thread);
    stop_log(log);
    return 0;
}