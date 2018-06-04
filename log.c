/**
 * Operation of log files
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "puttymem.h"
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
        log->on_off = 1;
    }

    return 0;
}

int stop_log(log_file *log)
{
    if (log->on_off)
    {
        close(log->fd);
        sfree(log->path);
        sfree(log->filt_key);
        log->on_off = 0;
    }

    return 0;
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

    if (log->buf.head == log->buf.tail)
        return 0;
    else if (log->buf.head < log->buf.tail)
    {
        pthread_mutex_lock(&mutex);
        int size = write(log->fd, &log->buf.space[log->buf.head], log->buf.tail-log->buf.head);

        if (size > 0)
            log->buf.head += size;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;
        
        pthread_mutex_unlock(&mutex);
    }
    else if (log->buf.head > log->buf.tail)
    {
        pthread_mutex_lock(&mutex);
        int size = write(log->fd, &log->buf.space[log->buf.head], LOG_BUF_SIZE-log->buf.head);

        if (size > 0)
            log->buf.head += size;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;
        else
        {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        size = write(log->fd, &log->buf.space[log->buf.head], log->buf.tail-log->buf.head);

        if (size > 0)
            log->buf.head += size;

        if (log->buf.head >= LOG_BUF_SIZE)
            log->buf.head -= LOG_BUF_SIZE;

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
    stop_log(log);
    return pthread_cancel(log_thread);
}