#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "opt.h"
#include "log.h"

typedef enum {opt_ok, opt_optnull, opt_argnull, opt_argset, opt_nocmd,
              opt_argnum, opt_argtype, opt_noarg, opt_nofile} opt_rtn;

void        opt_set         (char* opt, char* value);
opt_rtn     opt_struct_set  (opt_struct* opt, char* value);
opt_struct* opt_struct_get  (char* opt);
void        errm            (char* msg, int is_fatal);
void        rem_args        (char** arglist, int num);

opt_struct* g_os = NULL;
char*       g_usage = "";
char*       g_post_usage = NULL;
void(* err_msg)(char* msg, int is_fatal) = errm; // default error msg

// remove a specified number of arguments from an arg list
void rem_args(char** arglist, int num)
{
   int i;

   *arglist = NULL;	// start by assuming the worst

   // find out if we actually have enough args in this list
   for(i=1;i<num;i++)
      if(*(arglist+i) == NULL)
         return;

   // now move the args down
   while( (*arglist=*(arglist+num)) != NULL )
      arglist++;
}

void errm(char* msg, int is_fatal)
{
   // the idiot forgot to initialize first!!!
   write(2, "Programming error: You didn't call init_opt() first!\n", 53);
   exit(-1);
}

void opt_init(opt_struct* os, char* usage, char* post_usage,
              void(* error_callback)(char* msg, int is_fatal))
{
   opt_struct* opts;

   // initialize the globals
   g_os = os;
   g_usage = usage;
   g_post_usage = post_usage;
   err_msg = error_callback;

   // set the default values in the options
   for(opts=os;opts->option != NULL;opts++)
   {
      opt_struct_set(opts, opts->def);
      opts->set = 0;	// they're not "really" set yet
   }
}

void opt_set(char* opt, char* value)
{
   char buff[100];

   if(opt == NULL)
      err_msg("opt_set: got NULL option\n", 1);

   // set the option and interpret the result
   switch(opt_struct_set(opt_struct_get(opt), value))
   {
      case opt_ok:
         break;
      case opt_optnull:
         sprintf(buff, "%s: Unknown option\n", opt);
         err_msg(buff, 1);
      case opt_argnull:
         sprintf(buff, "%s: Missing argument\n", opt);
         err_msg(buff, 1);
      case opt_argset:
         break;
      case opt_argnum:
         sprintf(buff, "%s: %s: Argument not numeric\n", opt, value);
         err_msg(buff, 1);
      case opt_argtype:
         sprintf(buff, "%s: %s: Invalid argument type\n", opt, value);
         err_msg(buff, 1);
         break;
      case opt_noarg:
         sprintf(buff, "%s: Missing argument\n", opt);
         err_msg(buff, 1);
         break;
      case opt_nofile:
         sprintf(buff, "%s: %s: Can't load file\n", opt, value);
         err_msg(buff, 1);
         break;
      case opt_nocmd:
         sprintf(buff, "%s: %s: Unknown command\n", opt, value);
         err_msg(buff, 1);
         break;
      default:
         err_msg("opt_set: got bad return from opt_set_struct\n", 1);
   }
}

opt_rtn opt_struct_set(opt_struct* opt, char* value)
{
   int fd;
   char cmd[100];

   if(opt == NULL)
      return opt_optnull;
   if(value == NULL && opt->type != a_bool)
      return opt_argnull;

   // can't set an option twice
   if(opt->set)
      return opt_argset;

   // handle specific argument types
   switch(opt->type)
   {
      case a_file:	// file (may not exist)
      case a_str:	// character string
         strcpy(opt->data, (char*) value);
         break;
      case a_char:	// single char
         *(opt->data) = *value;
         break;
      case a_efile:	// file (checks if file exists)
         if( (fd=open(value, O_RDONLY)) < 0)
            return opt_nofile;
         close(fd);
         strcpy(opt->data, (char*) value);
         break;
      case a_defile:	// default existing file (no check if it is default)
         if( (fd=open(value, O_RDONLY)) < 0)
            if(!strcmp(opt->def, value) == 0)
               return opt_nofile;
         close(fd);
         strcpy(opt->data, (char*) value);
         break;
      case a_cmd:	// file (checks if file exists)
         copy_arg(cmd, value);
         if( (fd=open(cmd, O_RDONLY)) < 0)
            return opt_nocmd;
         close(fd);
         strcpy(opt->data, (char*) value);
         break;
      case a_dcmd:	// default existing file (no check if it is default)
         copy_arg(cmd, value);
         if( (fd=open(cmd, O_RDONLY)) < 0)
            if(!strcmp(opt->def, value) == 0)
               return opt_nocmd;
         close(fd);
         strcpy(opt->data, (char*) value);
         break;
      case a_bool:	// boolean
         // a null means flip
         if(value != NULL)
           *((int*) opt->data) = atoi(value);
         else // otherwise set it explicitly
            *((int*) opt->data) = !(*((int*) opt->data));
         break;
      case a_int:	// integer
         if(!is_integer(value))
            return opt_argnum;
         *((int*) opt->data) = atoi(value);
         break;
      default:
         return opt_argtype;
   }
   opt->set = 1;
   return opt_ok;
}

char* opt_get(char* opt)
{
   char buff[100];
   opt_struct* os = opt_struct_get(opt);

   if(os == NULL)
   {
      sprintf(buff, "%s: Unknown option\n", opt);
      err_msg(buff, 1);
   }
   return os->data;
}

opt_struct* opt_struct_get(char* opt)
{
   int i;

   if(g_os == NULL)
      err_msg("Opt struct not initialized!\n", 1);

   // opt is in flag form
   if(opt[1] == '\0')
   {
      for(i=0;g_os[i].option != NULL;i++)
         if(g_os[i].flagname == *opt)
            return g_os+i;
   }
   else // opt is in config file form
   {
      for(i=0;g_os[i].option != NULL;i++)
         if(strcmp(g_os[i].option, opt) == 0)
            return g_os+i;
   }
   return NULL;
}

int has_arg(char* opt)
{
   char buff[100];

   opt_struct* os = opt_struct_get(opt);

   if(os == NULL)
   {
      sprintf(buff, "%s: Unknown option\n", opt);
      err_msg(buff, 1);
   }
   return os->type != a_bool;
}

int opt_is_set(char* opt)
{
   char buff[100];

   opt_struct* os = opt_struct_get(opt);

   if(os == NULL)
   {
      sprintf(buff, "%s: Unknown option\n", opt);
      err_msg(buff, 1);
   }
   return os->set;
}

void parse_args(int argc, char** argv)
{
   char** ptr = argv+1;
   char* arg;
   char* opt;

   // go through all args
   while(*ptr != NULL)
   {
      if(**ptr != '-')	// skip non-opts
      {
         ptr++;
         continue;
      }

      opt = (*ptr)+1;

      if(*opt == '?')	// special case. '?' prints usage
         print_usage();

      if(has_arg(opt))
         arg = *(ptr+1);
      else
         arg = NULL;

      opt_set(opt, arg);	// set the option

      // remove opt (and argument, if any) from the arg list
      rem_args(ptr, has_arg(opt) ? 2 : 1);
   }
}

void print_usage()
{
   opt_struct* ptr;

   // usage passed in on opt_init()
   printf("Usage: %s\n", g_usage);
   printf("Opt  default                  meaning\n");

   // step through and display all options
   for(ptr = g_os;ptr->option != NULL;ptr++)
      if(ptr->type == a_bool)
         printf("  %c  %-24s %-50s\n", ptr->flagname, *(ptr->def) == '0' ? "off" : "on", ptr->desc);
      else
         printf("  %c  %-24s %-50s\n", ptr->flagname, ptr->def, ptr->desc);
   printf("NOTE: Boolean operators have no argument.\n");
   if(g_post_usage != NULL)
      printf(g_post_usage);
   exit(1);
}

int opt_load(char* filename)
{
   int fd;
   char buff[1000];
   char option[100];
   char arg[100];
   char* ptr;

   if( (fd=open(filename, O_RDONLY)) < 0)
      return 0;

   // read 1 line at a time and set options
   while(read_line(fd, buff) > 0)
   {
      if( (ptr = copy_arg(option, buff)) == NULL || option[0] == ';' || option[0] == '#' )
         continue;
      copy_arg(arg, ptr);
      opt_set(option, arg);
   }
   close(fd);
   return 1;
}

char* strip_whitespace(char* buff)
{
   if(buff == NULL)
      return NULL;

   for(;(*buff <= ' ' || *buff > '~') && *buff != '\0';buff++);
   return buff;
}

char* copy_arg(char* to, char* from)
{
   int end_quote = 0;

   from = strip_whitespace(from);

   if(to == NULL)
      return NULL;
   if(from == NULL)
   {
      *to = '\0';
      return NULL;
   }

   if(*from == '"')
   {
      // copy until matching quote
      from++;
      while((*to++ = *from++))
      {
         if( (*(to-1) == '"') && (*(to-2) != '\\' || *(to-3) == '\\') )
         {
            *(to-1) = '\0';
            end_quote = 1;
            break;
         }
      }
      if(!end_quote)
         err_msg("Unmatched quotes\n", 1);
   }
   else
   {
      // copy until white space
      while((*to++ = *from++))
      {
         if(*(to-1) <= ' ' || *(to-1) > '~')
         {
            *(to-1) = '\0';
            break;
         }
      }
   }

   if(*(from-1) == '\0')
      from--;
   from = strip_whitespace(from);

   // return the offset we came to
   return from;
}

int read_line(int fd, char* buff)
{
   int i = 0;
   int len;

   for(;;i++)
   {
      if((len=read(fd, buff+i, 1)) < 0)
         {perror("read_line: read");exit(-1);}
      // read until newline
      if(len == 0 || *(buff+i) == '\n')
         break;
   }
   *(buff+i) = '\0';	// null terminate
   return i;
}

int is_integer(char* str)
{
   if(*str == '-')
      str++;

   while(*str >= '0' && *str <= '9') str++;
   return *str == '\0';
}
