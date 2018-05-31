/**
 * Operation of log files
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "log.h"

int start_log(log_file *log)
{
    log->fd = open(log->path, O_CREAT|O_APPEND|O_WRONLY, S_IWOTH|S_IWGRP);

    if (log->fd < 0)
        return -1;
    //if ( write(log->fd, "test", 4) <= 0 )
        //return -1;

    log->on_off = 1;
    return 0;
}

int stop_log(log_file *log)
{
    close(log->fd);
    log->on_off = 0;
    return 0;
}