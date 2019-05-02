#ifndef _LOG_H
#define _LOG_H

void mode_syslog(char** argv);
void log_mode_debug();
extern void write_fd(int fd, char* fmt, ...);

extern void log(char* fmt, ...);
extern void log_debug(char* fmt, ...);
extern void log_fatal(char* fmt, ...);
extern void log_perror(char* fmt, ...);
extern void log_error(char* fmt, ...);
extern void log_quit(char* fmt, ...);

#endif
