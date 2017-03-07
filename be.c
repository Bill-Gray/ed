#include <locale.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#if defined( _WIN32)
   #define PDC_DLL_BUILD
#endif

#ifdef __WATCOMC__
   #include "mycurses.h"
   #include <conio.h>
#else
   #include "curses.h"
#endif
#include "ed.h"

#ifndef ALT_A
#include "curs_lin.h"
#endif

#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

void use_key( EFILE **curr_file, int key);
int read_profile_file( EFILE **curr_file, FILE *ifile);
void update_clock( void);
void set_video_mode( unsigned mode);      /* in BE.C */

char *bitfont;             /* used in VGA modes */
static char profname[80];
#ifdef __WATCOMC__
int min_line_realloc = 15000;
#else
int min_line_realloc = 2000;
#endif
extern FILE *random_file;
extern int xscr, yscr;     /* size of screen; default xscr=80, yscr=25 */
extern int color[], use_mac;
#ifdef USE_MOUSE
int no_mouse;
static MOUSE mouse;
#endif

#define CLOCK_CHAR  *(char far *)((unsigned long) 0x46c)

#ifdef USE_MYCURSES
static int curses_kbhit( )
{
   return( kbhit( ) ? 0: ERR);
}
#else
static int curses_kbhit( )
{
   int c;

   nodelay( stdscr, TRUE);
#ifndef _WIN32
// usleep( 100000);        /* .1 second */
#endif
   c = getch( );
   nodelay( stdscr, FALSE);
   if( c != ERR)     /* no key waiting */
      ungetch( c);
   return( c);
}
#endif


#ifndef __WATCOMC__
         /* for Watcom C,  extended_getch() has been */
         /* moved to curs_ext.cpp */
int extended_getch( void)
{
#ifdef RANDOM_FILE
   static int prev_c;
#endif
   int c = ERR;
   time_t t = time( NULL);

   while( c == ERR)
      {
#ifdef __WATCOMC__
      if( kbhit( ))
         {
         c = getch( );
         if( !c)
            c = getch( ) + 256;
         }
#else
      c = getch( );
#if !defined( _WIN32)
      if( c == 27)
         {
         nodelay( stdscr, TRUE);
         c = getch( );
         nodelay( stdscr, FALSE);
         if( c == ERR)    /* no second key found */
            c = 27;       /* just a plain ol' Escape */
         else
            c += (ALT_A - 'a');
         }
#endif   /* #if defined( __linux)      */
#endif   /* __WATCOMC__                */
      if( t != time( NULL))
         {
         t = time( NULL);
         if( stdscr)
            update_clock( );
         }
      }
   assert( c > 0);
   assert( c != 195);

#ifdef RANDOM_FILE
   if( c != prev_c && random_file)
      {
      char z = CLOCK_CHAR;
      unsigned count = (unsigned)prev_c;
      static char buff[16];
      static unsigned loc;

      while( z == CLOCK_CHAR)
         count++;
      buff[loc & 0x0f] ^= (char)(count ^ (count >> 8));
      loc++;
      if( !(loc & 0x0f))
         fwrite( buff, 16, 1, random_file);
      prev_c = c;
      }
#endif            /* RANDOM_FILE                */
   return( c);
}
#endif            /* MOVED_TO_CURS_EXT_DOT_CPP  */

void set_video_mode( unsigned mode)
{
   extern int full_refresh;      /* in SHOWFILE.C */

#ifndef __WATCOMC__
   mode %= 3;
#endif
   if( mode < 3)
      getmaxyx( stdscr, yscr, xscr);
#ifdef MS_DOS
   else
      {
      switch( mode)
         {
         case 3:
/*          PDC_set_scrn_mode( 38);       80x60 */
            xscr = 80;
            yscr = 60;
            break;
        case 4:
/*          PDC_set_scrn_mode( 42);       100x37 */
            xscr = 100;
            yscr = 37;
            break;
         case 5:
/*          PDC_set_scrn_mode( 35);       132x50 */
            xscr = 132;
            yscr = 50;
            break;
         }
      }
#endif
   resize_term( yscr, xscr);
   full_refresh = 1;
}

#ifdef DEBUG_MEM
static void memory_error( struct _memory_error *err)
{
   static char *status[6] = { "OK", "Re-freed", "Header overwritten",
         "End overwritten", "Null pointer", "Not allocated" };

   printf( "\n%s: Line %u: %u bytes %s", err->file_allocated, err->line_allocated,
            err->allocation_size, status[err->error_no]);
   if( err->error_no && err->error_no != POINTER_NOT_ALLOCATED)
      printf( "\nFound at %s: Line %u", err->file_found, err->line_found);
   extended_getch( );
}
#endif

#ifdef _WIN32
int dummy_main( int argc, char **argv)
#else
int main( int argc, char **argv)
#endif
{
   EFILE *curr_file = NULL, *next_file;
   int i, c, mono = 0, dump_mem = 0;
#ifdef DEBUG_MEM
   int checks_on = 1;
#endif
   FILE *profile;

   setlocale( LC_ALL, "en_US.UTF-8");
/* setvbuf( stdout, NULL, _IONBF, 0);        */
#ifdef DEBUG_MEM
   if( _stricmp( argv[1], "-c"))
      set_memory_error_function( memory_error);
   else
      {
      checks_on = 0;
      set_memory_error_function( NULL);
      }
#endif
#ifdef USE_MOUSE
   mouse.xmin = mouse.xmax = 0;     /* differential mode */
   mouse.sensitivity = 2;
   no_mouse = mouse_initialize( &mouse);
#endif

#if defined( XCURSES)
   resize_term( 50, 98);
   Xinitscr( argc, argv);
   mouse_set(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION);
#else
   initscr( );
#endif
   mousemask( ALL_MOUSE_EVENTS, NULL);
   cbreak( );
   noecho( );
   clear( );
   raw( );
   refresh( );
#ifndef __WATCOMC__
#ifdef MS_DOS
   mouse_set( BUTTON1_RELEASED | BUTTON2_RELEASED | BUTTON3_RELEASED);
#endif
#endif
   start_color( );
/*   if (can_change_color())          set yellow to be more 'orangish'
        init_color( COLOR_YELLOW, 1000, 500, 0);         */
   init_color( COLOR_WHITE, 1000, 1000, 1000);
   init_pair( 0, COLOR_WHITE, COLOR_BLACK);
   init_pair( 1, COLOR_BLACK, COLOR_WHITE);
   init_pair( 2, COLOR_WHITE, COLOR_YELLOW);
   init_pair( 3, COLOR_WHITE, COLOR_BLUE);
   init_pair( 4, COLOR_BLACK, COLOR_GREEN);
   init_pair( 5, COLOR_RED, COLOR_GREEN);         /* excluded obs */
   init_pair( 6, COLOR_WHITE, COLOR_RED);
   init_pair( 7, COLOR_BLACK, COLOR_RED);
   init_pair( 8, COLOR_WHITE, COLOR_CYAN);
   init_pair( 9, COLOR_BLACK, COLOR_YELLOW);
   init_pair( 10, COLOR_RED, COLOR_WHITE);
   init_pair( 11, COLOR_BLACK, COLOR_CYAN);
   init_pair( 12, COLOR_BLUE, COLOR_WHITE);
   init_pair( 13, COLOR_BLACK, COLOR_MAGENTA);
   init_pair( 14, COLOR_WHITE, COLOR_WHITE);
   init_pair( 15, COLOR_RED, COLOR_WHITE);

   keypad( stdscr, 1);
/*   Unfortunately,  enabling the following means one can't link to */
/*   other PDC flavors by simply swapping DLLs:                     */
/* PDC_set_shutdown_key( KEY_F(3));                                 */

   for( i = 1; i < argc; i++)
      if( !strcmp( argv[i], "-mac"))
         use_mac = 1;

   strcpy( profname, argv[0]);
   for( i = strlen( profname); i >= 0 && profname[i] != '\\' && profname[i] != '/'; i--);
   strcpy( profname + i + 1, "profile");
   profile = fopen( profname, "rb");
   if( !profile)
      profile = fopen( "/usr/bin/profile", "rb");
   if( !profile)
      {
      char filename[255];

      strcpy( filename, getenv( "HOME"));
      strcat( filename, "/ed/profile");
      profile = fopen( filename, "rb");
      }
   if( !profile)
      profile = fopen( "c:\\ed\\profile", "rb");
   profname[i + 1] = '\0';
   if( profile)
      {
      read_profile_file( NULL, profile);
      fclose( profile);
      }

   getmaxyx( stdscr, yscr, xscr);
   for( i = 1; i < argc; i++)
      if( argv[i][0] > ' ')
         {
         if( argv[i][0] != '-')
            {
            next_file = read_efile( argv[i]);
            if( next_file)
               {
               next_file->ystart += mono;
               if( !mono)
                  {
                  next_file->xsize = xscr;
                  next_file->ysize = yscr - 2;
                  }
               if( !curr_file)
                  curr_file = next_file;
               else
                  {
                  next_file->prev_file = curr_file->prev_file;
                  next_file->next_file = curr_file;
                  next_file->prev_file->next_file = next_file->next_file->prev_file = next_file;
                  }
               }
            show_efile( next_file);
            }
         else
            switch( argv[i][1])
               {
               case 't': case 'T':
                  {
                  extern int tab_xlate;
                  tab_xlate = atoi( argv[i] + 2);
                  }
                  break;
               case 'x': case 'X':
                  xscr = atoi( argv[i] + 2);
                  break;
               case 'y': case 'Y':
                  yscr = atoi( argv[i] + 2);
                  break;
               case 'r': case 'R':
                  min_line_realloc = atoi( argv[i] + 2);
                  break;
               case 'l': case 'L':
                  setlocale( LC_ALL, argv[i] + 2);
                  break;
               case 'n': case 'N':
                  {
                  extern int max_lines;

                  max_lines = atoi( argv[i] + 2);
                  }
                  break;
               case 's': case 'S':
                  {
                  extern int skipped_bytes;

                  skipped_bytes = atoi( argv[i] + 2);
                  }
                  break;
               case 'd': case 'D':
                  dump_mem = 1;
                  break;
               }
        }
   if( curr_file)
      curr_file->message = "BE Text Editor Version 0.0   Copyright (c) Project Pluto 1993";
   while( curr_file)
      {
//    if( curses_kbhit( ) != ERR)
         show_efile( curr_file);
      c = extended_getch( );
      curr_file->message = NULL;
      use_key( &curr_file, c);
#ifdef DEBUG_MEM
      if( checks_on)
         if( debug_check_all_memory( ))
            debug_dump_memory( );
#endif
      }
   attrset( COLOR_PAIR( 1));
   endwin( );
/* printf( "\n\n\n");         */
#ifdef XCURSES
   XCursesExit();
#endif

#ifdef DEBUG_MEM
   if( debug_check_all_memory( ))
      debug_dump_memory( );
#endif
   if( dump_mem)
      {
      LETTER *str;

      while( (str = pop_string( )) != NULL)
         free( str);
#ifdef DEBUG_MEM
      debug_dump_memory( );
#endif
      }
   return( 0);
}

#ifdef _WIN32
#include <windows.h>

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpszCmdLine, int nCmdShow)
{
   char *argv[30];
   int i, argc = 1;

   argv[0] = "Bill's Editor";
   for( i = 0; lpszCmdLine[i]; i++)
       if( lpszCmdLine[i] != ' ' && (!i || lpszCmdLine[i - 1] == ' '))
          argv[argc++] = lpszCmdLine + i;

   for( i = 0; lpszCmdLine[i]; i++)
       if( lpszCmdLine[i] == ' ')
          lpszCmdLine[i] = '\0';

   return dummy_main( argc, argv);
}
#endif
