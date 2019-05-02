#ifndef _OPT_H
#define _OPT_H

typedef enum {a_bool,	// boolean value
              a_str,	// null terminated string
              a_char,	// character
              a_int,	// integer
              a_efile,	// existing file (checks if file exists)
              a_defile, // default existing file
                        // (won't fail if default file doesn't exist)
              a_file,   // file (may not exist)
              a_cmd,	// command line
              a_dcmd,	// command line default
             } arg_type;

typedef struct
{
   char*    option;
   char     flagname;
   arg_type type;
   char*    def;
   char*    desc;
   char     data[100];
   int      set;
} opt_struct;

void  opt_init        (opt_struct* os, char* usage, char* post_usage,
                       void(* error_callback)(char* msg, int is_fatal));
void  parse_args      (int argc, char** argv);
int   opt_load        (char* filename);
void  print_usage     ();
char* opt_get         (char* opt);
int   opt_is_set      (char* opt);
int   is_integer      (char* str);
char* strip_whitespace(char* buff);
int   has_arg         (char* opt);
char* copy_arg        (char* to, char* from);
int   read_line       (int fd, char* buff);

#endif
