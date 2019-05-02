/* Ringconnect 2.3 (c) 1997 Karl Stenerud (stenerud@lightspeed.bc.ca)
 * 24-Feb-98
 *
 * See readme file and manpage for details.
 *
 * Supported platforms:
 *    Linux.  Anything else will probably fail on the serial routines.
 *
 * Copying / Licence / Warranty:
 *    This software is released as freeware under the GNU Public Licence.
 *    If you do any significant improvements on it, mail me a copy!
 *    
 *    As for warranty, there is none.  Use at your own risk!
 * 
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_ppp.h>
#include <netdb.h>
#include <signal.h>
#include <linux/serial.h>
#include <stdlib.h>
#include "opt.h"
#include "log.h"


#ifdef ASYNC_CALLOUT_ACTIVE
#define LINUX_2_1_X
#else
#define ASYNC_CALLOUT_ACTIVE 0x40000000
#endif

#define OPEN_MODEM(M)       open(M, O_RDONLY | O_NONBLOCK | O_NOCTTY)
#define OPEN_QUERY_SOCKET() socket(AF_INET, SOCK_DGRAM, 0)

void  check_for_ring_pattern(char* modem_device, int num_rings, int ring_time);
void  sleep_until_ring     (char* modem_device);
int   count_rings          (char* modem_device);
int   is_modem_dialing     (char* modem_device);
int   is_net_device_up     (char* net_device);
char* get_net_device_addr  (char* net_device);
int   idle_time            (char* ppp_device);
int   start_ppp            (char* prog, char* modem_device, char* ppp_device, int max_retries);
int   kill_proc            (char* proc_cmdline);
int   is_prg_running       (char* proc_cmdline);
int   get_pid              (char* proc_cmdline);
void  call_prg             (char* cmdline);
void  mail_ip              (char* server, char* recipient, char* my_ip, int timeout);
int   connect_socket       (char* address, short port);
int   dialog               (int fd, char* message, char* expected, char* err_msg, int timeout, int do_or_die);
int   wait_for_input       (int fd, int timeout);
int   contains             (char* buff, char* str);
void  error_callback       (char* msg, int is_fatal);
int   is_os_above_2_0      ();
int   is_modem_dialing_flag_set(int flags);

opt_struct g_opts[] =			/* options */
{
/*option        flag type      default          description*/
{"connect_prg", 'c', a_cmd,    "/usr/sbin/pppd","Program to connect online (full path!)"},
{"device",      'd', a_efile,  "/dev/modem",    "Modem device to attach to"},
{"debug",       'D', a_bool,   "0",             "Run in debug mode"},
{"config_file", 'f', a_defile, "/etc/ringconnectd.conf","Config file to read from"},
{"idle_timeout",'i', a_int,    "0",             "disconnect if idle for num seconds (0=disabled)"},
{"kill_prg",    'k', a_dcmd,   "",              "Use this prog to end connection (full path!)"},
{"mailto",      'm', a_str,    "",              "User to mail IP address to"},
{"nodetach",    'n', a_bool,   "0",             "Run in foreground instead of detaching"},
{"onceonly",    'o', a_bool,   "0",             "Connect once, then quit"},
{"run_prg",     'p', a_dcmd,   "",              "Run this program upon connection"},
{"ppp_device",  'P', a_str,    "ppp0",          "Monitor this PPP device for PPP connections"},
{"redial",      'r', a_int,    "10",            "Times to redial if line is busy"},
{"rings",       'R', a_int,    "0",             "Wait this many rings to connect (0=any)"},
{"smtp_server", 's', a_str,    "localhost",     "Mail server to use to send mail"},
{"timeout",     't', a_int,    "30",            "Timeout for mail server's response (-1=forever)"},
{"ring_time",   'T', a_int,    "7",             "Time between rings"},
{NULL}
};


int main(int argc, char** argv)
{
   char* connect_prg;
   char* modem_device;
   char* ppp_device;
   char* smtp_server;
   char* mailto;
   int   redial;
   int   onceonly;
   int   rings;
   int   idle_timeout;
   int   timeout;
   char* kill_prg;
   char* run_prg;
   int   nodetach;
   int   ring_time;
   int   debug_mode;

   signal(SIGCHLD, SIG_IGN);		/* Kills zombies.  Dead. */

   opt_init(g_opts, "ringconnectd [options]", NULL, error_callback);
   parse_args(argc, argv);
   opt_load(opt_get("config_file"));

   /* get our options */
   connect_prg  = opt_get("connect_prg");
   kill_prg     = opt_get("kill_prg");
   modem_device = opt_get("device");
   ppp_device   = opt_get("ppp_device");
   smtp_server  = opt_get("smtp_server");
   mailto       = opt_get("mailto");
   redial       = *((int*) opt_get("redial"));
   rings        = *((int*) opt_get("rings"));
   ring_time    = *((int*) opt_get("ring_time"));
   timeout      = *((int*) opt_get("timeout"));
   onceonly     = *((int*) opt_get("onceonly"));
   idle_timeout = *((int*) opt_get("idle_timeout"));
   run_prg      = opt_get("run_prg");
   nodetach     = *((int*) opt_get("nodetach"));
   debug_mode   = *((int*) opt_get("debug"));

   if(debug_mode)
      log_mode_debug();

   if(!nodetach)
   {
      if(fork())			/* go into daemon mode */
         exit(0);
      if(fork())
         exit(0);

      mode_syslog(argv);		/* go to syslog mode */
   }

   log("Waiting for ring pattern on %s", modem_device);

   for(;;)
   {
      if(is_net_device_up(ppp_device))
      {
         log("Device %s is UP.  Waiting for it to go down", ppp_device);
         while(is_net_device_up(ppp_device))
            sleep(5);
         log("PPP is DOWN");
      }

      if(check_for_ring_pattern(modem_device, rings, ring_time))
      {
         log("Ring pattern established.");
         sleep(10);
         log("Executing %s for connection", connect_prg);
         if(start_ppp(connect_prg, modem_device, ppp_device, redial))
         {
            log("Net device %s is UP", ppp_device);
            if(*mailto != 0)
               mail_ip(smtp_server, mailto, get_net_device_addr(ppp_device), timeout);
            if(*run_prg != '\0')
            {
               log("Executing %s", run_prg);
               call_prg(run_prg);
            }
            while(is_net_device_up(ppp_device))
            {
               sleep(5);
               if(idle_timeout > 0 && idle_time(ppp_device) >= idle_timeout)
               {
                  log("%d seconds of inactivity.", idle_timeout);
                  if(*kill_prg != '\0')
                  {
                     log("Calling %s to kill connection", kill_prg);
                     call_prg(kill_prg);
                  }
                  else
                  {
                     log("Killing %s", connect_prg);
                     kill_proc(connect_prg);
                  }
               }
            }
            log("Net device %s is DOWN", ppp_device);
            if(onceonly)
               break;
         }
         else
            log("Failed to connect.");
         log("Waiting for ring pattern on %s", modem_device);
      }
      sleep(5);
   }
   log("Exiting.");
   exit(0);
}


/****************************************************************************
 * Name:   wait_for_ring_pattern
 * Desc:   Wait for a specified number of rings, then return.
 *         Waits for the EXACT number of rings, no more, no less.
 *         Special case: when num_rings <= 0, it will wait for any
 *         number of rings.
 * Inputs: modem_device - name of modem device to monitor
 *         num_rings    - number of rings to wait (0=any)
 *         ring_time    - seconds expected between rings
 * Output: void
 ****************************************************************************/
int check_for_ring_pattern(char* modem_device, int num_rings, int ring_time)
{
   static int base_rings = count_rings(modem_device);
   int last_rings;
   int new_rings;
   int rc = 0;

   if(count_rings(modem_device) != base_rings)
   {
      log_debug("Got initial ring.");
      if(num_rings > 0)
         log_debug("Will call out after %d rings.", num_rings);
      else
         log_debug("Will call out after any number of rings.");

      for(;;)
      {
         log_debug("Waiting for next ring...");

         last_rings = count_rings(modem_device);
         /* wait for another ring */
         sleep(ring_time);
         new_rings = count_rings(modem_device);

         /* There wasn't a new ring */
         if(new_rings == last_rings)
            break;
      }
      /* This is the number of rings we wanted. */
      if(new_rings == (num_rings + base_rings) || num_rings <= 0)
      {
         log_debug("Target rings reached.");
         rc = 1;
      }
      else
      {
         log_debug(new_rings > (num_rings + base_rings) ? "Too many rings." : "Too few rings.");
         rc = 0;
      }
   }
   base_rings = count_rings(modem_device);
   return rc;
}

/****************************************************************************
 * Name:   sleep_until_ring
 * Desc:   Wait until the modem rings, then return.
 * Inputs: modem_device - modem device to monitor
 * Output: void
 ****************************************************************************/
void sleep_until_ring(char* modem_device)
{
   int fd;
   int badcount = 0;

   if((fd = OPEN_MODEM(modem_device)) < 0)
      log_fatal("sleep_until_ring: error opening %s", modem_device);

   log_debug("Sleeping until %s rings...", modem_device);
   while(ioctl(fd, TIOCMIWAIT, TIOCM_RNG) < 0)
   {
      if(errno != EIO)
         log_fatal("sleep_until_ring: ioctl (TIOCMIWAIT)");
      if(++badcount >= 5)
         log_quit("Can't access modem!");
      close(fd);
      if((fd = OPEN_MODEM(modem_device)) < 0)
         log_fatal("sleep_until_ring: error opening %s", modem_device);
   }
   close(fd);
}

/****************************************************************************
 * Name:   count_rings
 * Desc:   Count the number of times this device has rung
 * Inputs: fd - file descriptor of opened modem device
 * Output: int - number of times this device has rung
 ****************************************************************************/
int count_rings(char* modem_device)
{
   struct serial_icounter_struct c;
   int fd;

   if((fd = OPEN_MODEM(modem_device)) < 0)
      log_fatal("count_rings: error opening %s", modem_device);
   if(ioctl(fd, TIOCGICOUNT, &c) < 0)
      log_fatal("count_rings: ioctl (TIOCGICOUNT)");
   close(fd);

   log_debug("Counted %d rings", c.rng);

   return c.rng;
}

/****************************************************************************
 * Name:   is_modem_dialing
 * Desc:   Check if the modem is dialing
 * Inputs: modem_device - the device to check
 * Output: int (boolean) - true or false
 ****************************************************************************/
int is_modem_dialing(char* modem_device)
{
   struct serial_struct ss;
   int    fd;

   if((fd = OPEN_MODEM(modem_device)) < 0)
      log_fatal("is_modem_dialing: error opening %s", modem_device);
   if(ioctl(fd, TIOCGSERIAL, &ss) < 0)
      log_fatal("is_modem_dialing: ioctl (TIOGSERIAL)");
   close(fd);

   log_debug("%s is %s", modem_device, is_modem_dialing_flag_set(ss.flags) ? "dialing" : "not dialing");

   return is_modem_dialing_flag_set(ss.flags);
}

int is_modem_dialing_flag_set(int flags)
{
   return is_os_above_2_0() ? ((flags & ASYNC_CALLOUT_ACTIVE) != 0) : ((flags & 0x2000000) == 0);
}


// put real runtime check of OS here
int is_os_above_2_0()
{
#ifdef LINUX_2_1_X
   return 1;
#else
   return 0;
}

/****************************************************************************
 * Name:   is_net_device_up
 * Desc:   Check if netwprk device is up
 * Inputs: net_device - the device to check
 * Output: int (boolean) - true or false
 ****************************************************************************/
int is_net_device_up(char* net_device)
{
   struct ifreq ifr;
   int    fd;

   if((fd = OPEN_QUERY_SOCKET()) < 0)
      log_fatal("is_net_device_up: error opening socket");

   /* get device flags from the kernel */
   sprintf(ifr.ifr_name, net_device);
   if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
   {
      if(errno == ENODEV)
      {
         close(fd);
         return 0;
      }
      log_fatal("is_net_device_up: ioctl (SIOCGIFFLAGS)");
   }
   close(fd);

   log_debug("%s is %s", net_device, (ifr.ifr_flags & IFF_UP) ? "up" : "down");

   return (ifr.ifr_flags & IFF_UP);
}

/****************************************************************************
 * Name:   get_net_device_addr
 * Desc:   Get the IP address of the device
 * Inputs: net_device - the device to check
 * Output: char* - pointer to the device address
 ****************************************************************************/
char* get_net_device_addr(char* net_device)
{
   struct ifreq ifr;
   struct sockaddr_in saddr;
   int    fd;

   if((fd = OPEN_QUERY_SOCKET()) < 0)
      log_fatal("get_net_device_addr: error opening socket");

   /* get device address from kernel */
   sprintf(ifr.ifr_name, net_device);
   if(ioctl(fd, SIOCGIFADDR, &ifr) < 0)
   {
      if(errno == ENODEV)
         memset(&ifr, 0, sizeof(ifr));
      else
         log_fatal("get_net_device_addr: ioctl (SIOCGIFADDR)");
   }
   memcpy(&saddr, &(ifr.ifr_dstaddr), sizeof(struct sockaddr));

   close(fd);
   return inet_ntoa(saddr.sin_addr);
}

/****************************************************************************
 * Name:   idle_time
 * Desc:   check how long the ppp device has been idle since last check.
 * Inputs: ppp_device - the device to check
 * Output: int - idle time in seconds
 ****************************************************************************/
int idle_time(char* ppp_device)
{
   struct ifpppstatsreq req;
   int    fd;
   static int    last_ibytes = 0;
   static int    last_obytes = 0;
   static time_t last_time = 0;

   if((fd = OPEN_QUERY_SOCKET()) < 0)
      log_fatal("idle_time: error opening socket");

   memset (&req, 0, sizeof (req));
   req.stats_ptr = (caddr_t) &req.stats;
   sprintf(req.b.ifr_name, ppp_device);
   if(ioctl(fd, SIOCGPPPSTATS, &req) < 0)
   {
      if(errno == ENODEV)
      {
         close(fd);
         return 0;
      }
      log_fatal("idle_time: ioctl (SIOCGPPPSTATS)");
   }

   if(req.stats.p.ppp_ibytes != last_ibytes ||
      req.stats.p.ppp_obytes != last_obytes)
   {
      last_time = time(NULL);
      last_ibytes = req.stats.p.ppp_ibytes;
      last_obytes = req.stats.p.ppp_obytes;
   }

   log_debug("%s has been idle %d seconds", ppp_device, time(NULL) - last_time);

   close(fd);
   return time(NULL) - last_time;
}

/****************************************************************************
 * Name:   start_ppp
 * Desc:   Start a ppp connection
 * Inputs: prog         - Program to call
 *         modem_device - modem device to monitor
 *         ppp_device   - ppp device to monitor
 *         max_retries  - maximum retry attempts
 * Output: int (boolean) - success / fail
 ****************************************************************************/
int start_ppp(char* prog, char* modem_device, char* ppp_device, int max_retries)
{
   int tries = 0;
   int i;

   log_debug("Calling %s to open %s on %s", prog, ppp_device, modem_device);
   while(!is_net_device_up(ppp_device))	/* keep trying */
   {
      if(!is_prg_running(prog))
      {
         log_debug("Calling %s", prog);
         if(max_retries >= 0 && tries++ > max_retries)
         {
            log_debug("Too many retries.  Giving up");
            return 0;
         }
         call_prg(prog);		/* call the program */
         log_debug("Sleeping a bit...");
         sleep(1);			/* wait for modem to go off hook */
         sleep(3);
         log_debug("Waiting for modem to finish dialing...");
         while(is_modem_dialing(modem_device)) /* wait for it to dial */
            sleep(1);
         log_debug("Waiting for PPP handshake to finish...");
         for(i=0;i<7 && !is_net_device_up(ppp_device);i++)
            sleep(5);			/* wait for ppp handshake */
         if(!is_net_device_up(ppp_device))
            log_debug("Failed to connect.");
      }
      sleep(1);
   }
   log_debug("PPP connection established.");
   return 1;
}

/****************************************************************************
 * Name:   kill_proc
 * Desc:   Kill a process
 * Inputs: proc_cmdline - how the process was invoked (eg /usr/sbin/pppd)
 * Output: int (boolean) - success / fail
 ****************************************************************************/
int kill_proc (char* proc_cmdline)
{
   int pid = get_pid(proc_cmdline);

   log_debug("Killing process %s", proc_cmdline);
   if(pid == -1)
   {
      log_error("Could not kill %s: no such process\n", proc_cmdline);
      return 0;
   }   
   if(kill(pid, SIGKILL) < 0)
   {
      log_perror("Could not kill %s", proc_cmdline);
      return 0;
   }
   return 1;
}

/****************************************************************************
 * Name:   is_prg_running
 * Desc:   Check if a process is running
 * Inputs: proc_cmdline - how the process was invoked (eg /usr/sbin/pppd)
 * Output: int (boolean) - true / false
 ****************************************************************************/
int is_prg_running(char* proc_cmdline)
{
   return get_pid(proc_cmdline) != -1;
}

/****************************************************************************
 * Name:   get_pid
 * Desc:   Get the PID of a process
 * Inputs: proc_cmdline - how the process was invoked (eg /usr/sbin/pppd)
 * Output: int - the process ID or -1 if failed
 ****************************************************************************/
int get_pid(char* proc_cmdline)
{
   DIR*   dir;
   struct dirent* dent;
   char   buff[1000];
   int    fd;
   int    len;

   log_debug("Finding process %s", proc_cmdline);
   /* open the /proc dir and look for pids */
   if( (dir=opendir("/proc")) == NULL) log_fatal("get_pid: error opening /proc");

   while( (dent = readdir(dir)) != NULL)
   {
      if(is_integer(dent->d_name))
      {
         /* for each entry in /proc, try to use it as a directory and read */
         /* the file "cmdline" file from within it.                        */
         sprintf(buff, "/proc/%s/cmdline", dent->d_name);
         if( (fd=open(buff, O_RDONLY)) >0 )
         {
            if( (len=read(fd, buff, 1000)) < 0 ) log_fatal("find_pid: error reading pid entry");
            close(fd);
            buff[len] = 0;
            /* if it matches what we're looking for, return true */
            if(strcmp(buff, proc_cmdline) == 0)
            {
               closedir(dir);
               return atoi(dent->d_name);
            }
         }
      }
   }
   closedir(dir);
   return -1;
}

/****************************************************************************
 * Name:   call_prg
 * Desc:   Execute a program
 * Inputs: cmdline - the command line to execute
 * Output: void
 ****************************************************************************/
void call_prg(char* cmdline)
{
   char  argv[25][100];
   char* argvp[25];
   char* ptr = cmdline;
   int   i;

   if(cmdline == NULL)
      return;

   log_debug("Calling command %s", cmdline);
   for(i=0;i<25;i++)
   {
      argvp[i] = argv[i];
      ptr=copy_arg(argvp[i], ptr);

      if(*ptr == '\0')	/* last arg */
         break;
   }
   if(++i>24)		/* get off last arg */
      i = 24;

   argvp[i] = NULL;	/* null terminate */

   switch(fork())
   {
      case 0:
         if(execv(*argvp, argvp) < 0)
            log_perror("execv");
      case -1:
         log_fatal("call_prg: could not fork");
   }
}

/****************************************************************************
 * Name:   mail_ip
 * Desc:   Mail an IP address to someone
 * Inputs: server    - address of a mail server (smtp)
 *         recipient - email address of recipient
 *         my_ip     - IP address
 *         timeout   - seconds to wait for response from mail server
 * Output: void
 ****************************************************************************/
void mail_ip(char* server, char* recipient, char* my_ip, int timeout)
{
   char  Rcpt[100];
   char  Mesg[100];
   char  ip[16];
   char* Helo = "HELO localhost\r\n";
   char* From = "MAIL FROM:ringconnect@localhost\r\n";
   char* Data = "DATA\r\n";
   char* Quit = "QUIT\r\n";
   int   fd;

   if(server == NULL || recipient == NULL || my_ip == NULL
   || *server == '\0' || *recipient == '\0')
      return;

   strcpy(ip, my_ip);
   sprintf(Rcpt, "RCPT TO:%s\r\n", recipient);
   sprintf(Mesg, "Subject: IP is %s\r\nMy IP is %s\r\n.\r\n", ip, ip);

   log("Mailing IP %s to %s via %s with timeout %d", ip, recipient, server, timeout);

   /* connect to the server */
   if((fd=connect_socket(server, 25)) < 0)
      return;

   /* start a dialog and send the mail */
   if(dialog(fd, NULL, "220", "Failed on CONNECT\n", timeout, 0))
   {
      dialog(fd, Helo, "250", NULL, timeout, 0);
      if(dialog(fd, From, "250", "Failed on MAIL\n", timeout, 0))
         if(dialog(fd, Rcpt, "250", "Failed on RCPT\n", timeout, 0))
            if(dialog(fd, Data, "354", "Failed on DATA\n", timeout, 0))
               if(dialog(fd, Mesg, "250", "Failed on EOF\n", timeout, 0))
                  dialog(fd, Quit, NULL, "", timeout, 0);
   }
   close(fd);
}


/****************************************************************************
 * Name:   connect_socket
 * Desc:   Open a socket and connect to a host
 * Inputs: address - address of host
 *         port    - port to connect to
 * Output: int - file descriptor of socket or -1 if failed
 ****************************************************************************/
int connect_socket(char* address, short port)
{
   struct sockaddr_in saddr;
   struct hostent* host_info;		/* returned by gethostbyname() */
   int    fd;

   log_debug("Connecting socket to %s, port %d", address, port);

   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family = AF_INET;
   saddr.sin_port = htons(port);

   /* try to decode dotted quad notation */
   if(!inet_aton(address, &saddr.sin_addr))
   {
      /* failing that, look up the name */
      if((host_info = gethostbyname(address)) == NULL)
         log_fatal("connect_socket: gethostbyname");
      memcpy(&saddr.sin_addr, host_info->h_addr, host_info->h_length);
   }
   if((fd=socket(AF_INET, SOCK_STREAM, 0)) < 0) log_fatal("connect_socket: could not open socket");
   if(connect(fd, (struct sockaddr*) &saddr, sizeof(saddr)) < 0)
      log_perror("connect_socket: could not connect");
   return fd;
}

/****************************************************************************
 * Name:   dialog
 * Desc:   Send a message over a file descriptor and compare its result to
 *         what we're expecting.
 * Inputs: fd        - file descriptor for transmission of messages
 *         message   - message to send (NULL = don't send message)
 *         expected  - expected response (NULL = don't expect response)
 *         err_msg   - error message to log if it fails
 *         timeout   - seconds to wait for reply (-1 = forever)
 *         do_or_die - (boolean) quit program if it fails
 * Output: int (boolean) - success / fail
 ****************************************************************************/
int dialog(int fd, char* message, char* expected, char* err_msg, int timeout, int do_or_die)
{
   char buff[1000];
   int  len = 0;

   if(message != NULL)
      if(write(fd, message, strlen(message)) < 0) log_fatal("dialog: write");

   log_debug("Writing message %s", message);

   if(expected != NULL)
   {
      log_debug("Expecting response %s, timeout %d", expected, timeout);
      /* wait for some data then compare it with what we're expecting */
      if(!wait_for_input(fd, timeout))
         log_error("Timed out on read\n");
      else
         if((len=read(fd, buff, 1000)) < 0) log_fatal("dialog: read");

      buff[len] = 0;
      if(!contains(buff, expected))
      {
         if(err_msg != NULL)
         {
            if(do_or_die)
               log_quit(err_msg);
            else
               log_error(err_msg);
         }
         return 0;
      }
   }
   return 1;
}

/****************************************************************************
 * Name:   wait_for_input
 * Desc:   Wait for input on the specified file descriptor
 * Inputs: fd - file descriptor to monitor
 *         timeout - seconds to wait (-1 = forever)
 * Output: int (boolean) - success / fail
 ****************************************************************************/
int wait_for_input(int fd, int timeout)
{
   struct timeval  tval = {timeout, 0};
   struct timeval* wait_val = (timeout >= 0 ? &tval : NULL); /* < 0 = forever */
   fd_set in_set;
   fd_set out_set;
   fd_set exc_set;

   log_debug("Waiting for input from socket, timeout = %d", timeout);

   FD_ZERO(&in_set);
   FD_ZERO(&out_set);
   FD_ZERO(&exc_set);
   FD_SET(fd, &in_set);

   /* use select to wait on this file descriptor */
   switch(select(fd+1, &in_set, &out_set, &exc_set, wait_val))
   {
      case 0:
         return 0;			/* we timed out */
      case 1:
         if(!FD_ISSET(fd, &in_set))	/* big problem */
            log_quit("select returned with no read\n");
         return 1;			/* everything a-ok */
      default:
         log_fatal("wait_for_input: select"); /* error */
   }
   return 0;
}

/****************************************************************************
 * Name:   contains
 * Desc:   Check if a string contains a substring
 * Inputs: buff - string to search
 *         str  - substring to search for
 * Output: int (boolean) - true / false
 ****************************************************************************/
int contains(char* buff, char* str)
{
   int   len = strlen(str);
   char* ptr = buff;

   for(;ptr != NULL;ptr++)
      if(strncmp(buff, str, len) == 0)
         return 1;
   return 0;
}


void error_callback(char* msg, int is_fatal)	/* error callback for opt.c */
{
   if(is_fatal)
      log_quit(msg);
   else
      log_error(msg);
}
