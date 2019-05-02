#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>

#include "log.h"

// signal catchers
static void  catch_sigquit();
static void  catch_sigterm();
static void  catch_sigint();
void report_and_quit(char* msg);

int  g_mode_syslog = 0;
int  g_mode_debug = 0;
char g_str[10000];

void log_mode_debug()
{
   g_mode_debug = 1;
}

void mode_syslog(char** argv)	// do system level logging
{
   g_mode_syslog = 1;
   signal(SIGQUIT, catch_sigquit);
   signal(SIGTERM, catch_sigterm);
   signal(SIGINT,  catch_sigint);
   openlog(*argv, LOG_PID, LOG_DAEMON); 
}

// basic log entry
void log(char* fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(g_str, fmt, args);
   va_end(args);

   if(g_mode_syslog)
      syslog(LOG_DAEMON|LOG_INFO, g_str);
   else
   {
      write(1, g_str, strlen(g_str));
      write(1, "\n", 1);
   }
}

// debug log entry
void log_debug(char* fmt, ...)
{
   va_list  args;

   if(!g_mode_debug)
      return;

   va_start(args, fmt);
   vsprintf(g_str, fmt, args);
   va_end(args);

   if(g_mode_syslog)
      syslog(LOG_DAEMON|LOG_INFO, g_str);
   else
   {
      write(1, g_str, strlen(g_str));
      write(1, "\n", 1);
   }
}

// fatal error.  we do a perror here
void log_fatal(char* fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(g_str, fmt, args);
   va_end(args);

   if(g_mode_syslog)
   {
      strcat(g_str,": %m\n"); /* emulate perror */
      syslog(LOG_DAEMON|LOG_ERR, g_str);
   }
   else
      perror(g_str);
   exit(-1);
}

// perror without exiting
void log_perror(char* fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(g_str, fmt, args);
   va_end(args);

   if(g_mode_syslog)
   {
      strcat(g_str,": %m\n"); /* emulate perror */
      syslog(LOG_DAEMON|LOG_ERR, g_str);
   }
   else
      perror(g_str);
}

// regular error message
void log_error(char* fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(g_str, fmt, args);
   va_end(args);

   if(g_mode_syslog)
      syslog(LOG_DAEMON|LOG_ERR, g_str);
   else
   {
      write(2, g_str, strlen(g_str));
      write(2, "\n", 1);
   }
}

// error message + quit
void log_quit(char* fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(g_str, fmt, args);
   va_end(args);

   if(g_mode_syslog)
      syslog(LOG_DAEMON|LOG_ERR, g_str);
   else
   {
      write(2, g_str, strlen(g_str));
      write(2, "\n", 1);
   }
   exit(-1);
}

static void  catch_sigquit()
{
   report_and_quit("Caught SIGQUIT, exiting");
}
static void  catch_sigterm()
{
   report_and_quit("Caught SIGTERM, exiting");
}
static void  catch_sigint()
{
   report_and_quit("Caught SIGINT, exiting");
}

void report_and_quit(char* msg)
{
   if(g_mode_syslog)
      syslog(LOG_DAEMON|LOG_WARNING, msg);
   else
      write(1, msg, strlen(msg));
   exit(0);
}
