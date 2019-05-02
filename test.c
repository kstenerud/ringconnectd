#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <signal.h>
#include <linux/serial.h>
#include <stdlib.h>

void test_modem();

int main(void)
{
   test_modem();
   getchar();
   test_modem();
   getchar();
   test_modem();
   return 0;
}


void test_modem()
{
   struct serial_icounter_struct c;
   struct serial_struct ss;
   int tries = 0;
   int fd;

   if( (fd = open("/dev/modem", O_RDONLY | O_NONBLOCK | O_NOCTTY)) < 0 )
   {perror("open");exit(0);}

   // query serial device
   if(ioctl(fd, TIOCGICOUNT, &c) < 0)
   {perror("ioctl");exit(0);}
   if(ioctl(fd, TIOCGSERIAL, &ss) < 0)
   {perror("ioctl");exit(0);}

   if(!(ss.flags & 000200000000))
      printf("off hook\n");
printf("flags = %012o, rings = %d\n", ss.flags, c.rng);


   close(fd);

}

