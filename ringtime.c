#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

void sleep_until_ring();
int count_rings();

int g_modemfd;
char* g_device;

int main(int argc, char* argv[])
{
   int last_time;
   char buff[100];

   if(argc != 2)
   {
      printf("Usage: ringtime <tty>\n\n");
      printf("e.g. ringtime /dev/modem\n");
      printf("Remember to add 1 second to the reported time to be sure\n");
      printf("you're well within bounds.\n");
      exit(0);
   }

   g_device = argv[1];

   if( (g_modemfd = open(g_device, O_RDONLY | O_NONBLOCK | O_NOCTTY)) < 0 )
   {
      perror("open");
      exit(-1);
   }

   printf("Monitoring %s\n", g_device);
   for(;;)
   {
      last_time = time(NULL);
      sleep_until_ring();
      sprintf(buff, "Time elapsed since last ring: %d seconds\n",
              (int)(time(NULL) - last_time));
      write(1, buff, strlen(buff));
   }
}

void sleep_until_ring()
{
   int num_rings = count_rings();

   // wait for 1 ring
   while(count_rings() == num_rings)
      ioctl(g_modemfd, TIOCMIWAIT, TIOCM_RNG);
}

int count_rings()
{
   struct serial_icounter_struct c;
   int tries = 0;

   // query serial device
   while(ioctl(g_modemfd, TIOCGICOUNT, &c) < 0)
   {
      if(errno != EIO)
      {
         perror("ioctl");
         exit(-1);
      }
      close(g_modemfd);
      if(tries++ >= 5)		// try 5 times, then abort
      {
         write(1, "Problem with serial port.\n", 26);
         exit(-1);
      }
      if( (g_modemfd = open(g_device, O_RDONLY | O_NONBLOCK | O_NOCTTY)) < 0 )
      {
         perror("count_rings: open");
         exit(-1);
      }
   }
   return c.rng;
}
