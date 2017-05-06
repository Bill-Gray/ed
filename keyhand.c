#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef __linux
   #define USE_JOYSTICK
   #define N_JOY_AXES 6
   #define KEY_JOYSTICK 314159
   extern int joy_axes[];
#endif

#if defined( _WIN32)
   #define PDC_DLL_BUILD
#endif

#ifdef __WATCOMC__
   #include "mycurses.h"
   #include "curs_ext.h"
#else
   #include <curses.h>
#endif

#ifndef ALT_A
#include "curs_lin.h"
#endif

#ifndef __GNUC__
#include <dos.h>
#endif
#include <time.h>
#include "ed.h"
#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

#define N_PREV_COMMANDS 20
#define NOT_OVERLAPPED( A, B)  ((A)->ystart != (B)->ystart || (A)->xstart != \
          (B)->xstart || (A)->xsize != (B)->xsize || (A)->ysize != (B)->ysize)

EFILE *block_file;      /* there's only one block,  so info on it is global */
int block_x1, block_y1, block_x2, block_y2, is_block, queue = 0;
static int over_attr = 0;

static char prev_command[N_PREV_COMMANDS][80];

extern int tabs[];      /* defined in COMMHAND.C */
extern int display_mode;      /* in SHOWFILE.C */
extern int full_refresh;      /* in SHOWFILE.C */
extern int n_redefs;
extern char **redefs_from, **redefs_to;
int insert = 0;
void set_video_mode( unsigned mode);      /* in BE.C */

int handle_command( EFILE **curr_file, char *comm);
int realloc_lines( EFILE *efile, int n);
int adjust_paragraph( EFILE *efile, int x, int y);   /* etools2.c */
int find_word_limits( EFILE *efile, int *start, int *end);   /* etools2.c */

static int is_a_c_function( EFILE *efile, int line_no)
{
   LINE *line = efile->lines + line_no;

   if( line[0].size > 0 && line[1].size > 0 &&
            isalpha( line[0].str[0]))
      {
      int j;

      for( j = line_no + 1, line++; j < efile->n_lines; j++, line++)
         if( !line->size)
            return( 0);
         else if( line->str[0] == '{')
            return( 1);
         else if( line->str[0] != ' ')
            return( 0);
      }
   return( 0);
}

void use_key( EFILE **curr_file, int key)
{
   EFILE *efile;
   LINE *line;
   int i;
   static char errbuff[80];

   for( i = 0; i < n_redefs; i++)
      if( redefs_from[i][0] == '#' && atoi( redefs_from[i] + 1) == key)
         {
         handle_command( curr_file, redefs_to[i]);
         return;
         }

   efile = *curr_file;
   line = efile->lines + efile->y;
   switch( key)
      {
#if defined( USE_JOYSTICK)
      case KEY_JOYSTICK:
         if( joy_axes[0] > 0)
            efile->x += 1 + joy_axes[0] / 1000;
         if( joy_axes[0] < 0)
            efile->x -= 1 - joy_axes[0] / 1000;
         if( joy_axes[1] > 0)
            efile->y += 1 + joy_axes[1] / 1000;
         if( joy_axes[1] < 0)
            efile->y -= 1 - joy_axes[1] / 1000;
         break;
#endif

#if defined( KEY_MOUSE)
      case KEY_MOUSE:
         {
#if defined( _WIN32)
         int step;
#endif
         MEVENT mouse_event;

#ifdef __PDCURSES__
         nc_getmouse( &mouse_event);
#else

         getmouse( &mouse_event);
#endif

#if defined( _WIN32)
         step = ((BUTTON_STATUS(1) & BUTTON_SHIFT) ? 15 : 1);
         if( BUTTON_STATUS(1) & BUTTON_CONTROL)
            step *= 4;
         if( MOUSE_WHEEL_UP)
            {
            efile->y -= step;
            efile->top -= step;
            }
         else if( MOUSE_WHEEL_DOWN)
            {
            efile->y += step;
            efile->top += step;
            }
         else
#endif
            {
            if( mouse_event.y >= efile->ystart
                   && mouse_event.y < efile->ystart + efile->ysize
                   && mouse_event.x >= efile->xstart
                   && mouse_event.x < efile->xstart + efile->xsize)
               {
               efile->y = efile->top + mouse_event.y - 1;
               efile->x = efile->left + mouse_event.x;
               efile->command_loc = -1;
               }
            else if( mouse_event.y == efile->ystart + efile->ysize)
               efile->command_loc = strlen( efile->command);
            }
         }
         break;
#endif
      case CTL_LEFT:          /* ctrl + left cursor */
         if( efile->command_loc == -1 && line->size)
            {
            if( line->size <= efile->x)
               efile->x = line->size;
            efile->x--;
            while( efile->x && ((char)line->str[efile->x] == ' ' ||
                                (char)line->str[efile->x - 1] != ' '))
               efile->x--;
            }
         break;
      case CTL_RIGHT:          /* ctrl + right cursor */
         if( efile->command_loc == -1)
            {
            if( efile->x >= line->size)
               efile->x += 40;
            while( efile->x < line->size && (char)line->str[efile->x] == ' ')
               efile->x++;
            while( efile->x < line->size && (char)line->str[efile->x] != ' ')
               efile->x++;
            while( efile->x < line->size && (char)line->str[efile->x] == ' ')
               efile->x++;
            }
         break;
#ifdef ALT_LEFT
      case ALT_LEFT:          /* alt + left cursor */
         if( efile->command_loc == -1)
            efile->x += 40 - ((efile->x + 1) % 40);
         break;
      case ALT_RIGHT:          /* alt + right cursor */
         if( efile->command_loc == -1 && efile->x)
            {
            efile->x -= efile->x % 40;
            efile->x--;
            }
         break;
      case ALT_UP:         /* move up 1% in the file */
         efile->y -= efile->n_lines / 100;
         break;
#ifdef ALT_DOWN
      case ALT_DOWN:       /* move down 1% in the file */
#endif
#ifdef ALT_DN
      case ALT_DN:         /* move down 1% in the file */
#endif
         efile->y += efile->n_lines / 100;
         break;
#endif
#ifdef KEY_B1
      case KEY_B1:              /* left cursor */
#endif
      case KEY_LEFT:
         if( efile->command_loc == -1)
            efile->x--;
         else if( efile->command_loc > 0)
            efile->command_loc--;
         break;
#ifdef KEY_B3
      case KEY_B3:             /* right cursor */
#endif
      case KEY_RIGHT:
         if( efile->command_loc == -1)
            efile->x++;
         else if( efile->command[efile->command_loc])
            efile->command_loc++;
         break;
      case KEY_UP:        /* cursor up */
#ifdef KEY_A2
      case KEY_A2:
#endif
         if( efile->command_loc == -1)
            efile->y--;
         else
            {
            extern int centering;

            efile->x = efile->command_loc;
            efile->command_loc = -1;
            efile->y = efile->top + efile->ysize - centering;
            }
         break;
      case KEY_C3:
      case KEY_NPAGE:
         efile->y += efile->ysize - 1;
         efile->top += efile->ysize - 1;
         break;
      case KEY_PPAGE:
      case KEY_A3:
         efile->y -= efile->ysize - 1;
         efile->top -= efile->ysize - 1;
         break;
      case KEY_DOWN:
#ifdef KEY_C2
      case KEY_C2:
#endif
         if( efile->command_loc == -1)
            efile->y++;
         else
            {
            extern int centering;

            efile->x = efile->command_loc;
            efile->command_loc = -1;
            efile->y = efile->top + centering;
            }
         break;
#ifdef CTL_PGUP
      case CTL_PGUP:       /* top of file */
#endif
      case ALT_PGUP:       /* top of file */
#ifdef CTL_PAD9
#ifndef __WATCOMC__
      case CTL_PAD9:
#endif
#endif
         efile->y = efile->top = 0;
         break;
#ifdef CTL_PGUP
      case CTL_PGDN:       /* end of file */
#endif
      case ALT_PGDN:       /* end of file */
#ifdef CTL_PAD3
#ifndef __WATCOMC__
      case CTL_PAD3:
#endif
#endif
         efile->y = efile->n_lines - 1;
         break;
      case KEY_A1:
      case KEY_HOME:
         if( efile->command_loc == -1)
            efile->command_loc = strlen( efile->command);
         else
            efile->command_loc = -1;
         break;
      case KEY_F(3):                /* quit */
         if( !efile->dirty)
            {
            if( efile->next_file == efile)      /* last file in ring */
               *curr_file = NULL;
            else
               *curr_file = efile->next_file;
            free_efile( efile);
            }
         else
            efile->message = "Changes made, must use qq";
         return;
         break;
      case KEY_F(4):        /* find matching brace, bracket or paren */
         find_matching_whatsis( efile);
         break;
      case KEY_F(5):        /* show ASCII value under cursor */
         if( efile->y >= 0 && efile->y < efile->n_lines)
            if( efile->x < line->size)
               {
               unsigned z = line->str[efile->x];

               sprintf( errbuff, "0x%02x %3u ", z, z);
               for( i = 0; i < 8; i++)
                  errbuff[i + 9] = ((z & (128 >> i)) ? '1' : '0');
               errbuff[17] = '\0';
               efile->message = errbuff;
               }
         break;
#ifdef __WATCOMC__
      case KEY_F(17):        /* Shift-F5: select new text mode */
         {
         int new_mode;

         attrset( COLOR_PAIR( 2));
         new_mode = choose_a_text_mode( );
         if( new_mode >= 0)
            {
            set_text_mode( new_mode);
            if( efile)
               {
               extern int xscr, yscr;

               getmaxyx( stdscr, yscr, xscr);
               efile->xsize = xscr;
               efile->ysize = yscr - 2;
               full_refresh = 1;
               }
            }
         }
         break;
#endif
      case KEY_F(7):        /* find current function */
      case KEY_F(11):       /* go to top of function */
      case KEY_F(11 + 12):  /* Shift-F11:  also go to top of function */
         for( i = efile->y - 1; i > 0; i--)
            if( is_a_c_function( efile, i))
               {
               if( key == KEY_F(7))  /* show which function you're in */
                  {
                  int j, len = efile->lines[i].size;

                  if( len > 79)
                     len = 79;
                  for( j = 0; j < len; j++)
                     errbuff[j] = (char)efile->lines[i].str[j];
                  errbuff[len] = '\0';
                  efile->message = errbuff;
                  i = 0;
                  }
               else
                  {
                  efile->y = i;
                  i = 0;
                  }
               }
         break;
      case KEY_F(47):       /* Alt-F11: go to next function */
      case KEY_F(10 + 12):  /* Shift-F10: also go to next func */
         for( i = efile->y + 1; i < efile->n_lines; i++)
            if( is_a_c_function( efile, i))
               {
               efile->y = i;
               i = efile->n_lines;
               }
         break;
      case KEY_F(12):       /* lower-to-upper or vice versa */
      case KEY_F(24):       /* Cap plus lower: Shift-F12 */
         if( efile->y >= 0 && efile->y < efile->n_lines)
            if( efile->x < line->size)
               {
               char z = (char)line->str[efile->x];

               for( i = (unsigned)efile->x; i && line->str[i-1] != ' '; i--)
                  ;                    /* find start of word */
               if( key == KEY_F(24))
                  i++;
               while( i < line->size && line->str[i] != ' ')
                  {
                  if( isupper( z))
                     line->str[i] = (LETTER)tolower( line->str[i]);
                  if( islower( z))
                     line->str[i] = (LETTER)toupper( line->str[i]);
                  i++;
                  }
               }
         break;
      case ALT_0:
         efile->y = (int)( 10L * (long)efile->n_lines / 11L);
         break;
      case ALT_1: case ALT_2: case ALT_3:
      case ALT_4: case ALT_5: case ALT_6:
      case ALT_7: case ALT_8: case ALT_9:
         efile->y = (int)((long)(key - ALT_1 + 1) *
                          (long)efile->n_lines / 11L);
         break;
      case KEY_IC:         /* Insert key */
#ifdef PAD0
      case PAD0:           /* Insert on keypad */
#endif
         insert ^= 1;
         break;
      case KEY_C1:
      case KEY_END:
         if( efile->command_loc == -1)
            {
            if( efile->y != -1 && efile->y != efile->n_lines)
               efile->x = efile->lines[efile->y].size;
            }
         else
            for( efile->command_loc = 0; efile->command[efile->command_loc];
                              efile->command_loc++)
               ;
         break;
      case ALT_N:              /* nuke remainder of line */
         if( efile->y >= 0 && efile->y < efile->n_lines)
            {
            line = efile->lines + efile->y;
            if( line->size > efile->x)
               {
               line->str[efile->x] = '\0';
               line->size = efile->x;
               }
            }
         break;
      case ALT_R:              /* recover a line */
         {
         LETTER *new_str = pop_string( );

         if( new_str)
            {
            add_lines( efile, efile->y + 1, 1);
            line[1].str = new_str;
            line[1].size = line[1].alloced = line_length( line[1].str);
            }
         else
            efile->message = "No lines left to recover";
         }
         break;
      case ALT_P:              /* paragraphise */
         adjust_paragraph( efile, efile->x, efile->y);   /* etools2.c */
         break;
      case ALT_I:              /* truncate line */
         if( efile->y >= 0 && efile->y < efile->n_lines)
            {
            line = efile->lines + efile->y;
            if( line->size)
               line->str[line->size] = '\0';
            if( efile->x < line->size)
               {
               line->size = efile->x;
               reset_line( line);
               }
            }
         break;
      case ALT_S:              /* split a line */
      case ALT_V:              /* zap to end of line */
         if( efile->y >= 0 && efile->y < efile->n_lines)
            {
            if( key == ALT_S)
               add_lines( efile, efile->y + 1, 1);
            line = efile->lines + efile->y;
            if( line->size)
               line->str[line->size] = '\0';
            if( efile->x < line->size)
               {
               if( key == ALT_S)    /* add remains of current line to */
                  {                 /* next line: */
                  change_line( efile, line->str, efile->y + 1);
                  add_characters( efile, efile->y + 1, 0, -efile->x);
                  }
               line->size = efile->x;
               reset_line( line);
               }
            }
         else
            efile->message = "No line to split";
         break;
      case ALT_T:              /* show curr time */
         {
         time_t t = time( NULL);
         efile->message = ctime( &t);
         efile->message[24] = '\0';
         }
         break;
      case ALT_J:              /* join a line */
         if( efile->y >= 0 && efile->y < efile->n_lines - 1)
            {
            LETTER tbuff[1000];
            int len, len1;

            len = line->size;
            if( len > efile->x)
               len = efile->x;
            if( len)
               memcpy( tbuff, line->str, len * sizeof( LETTER));
            len1 = line[1].size;
            if( len1)
               memcpy( tbuff + len, line[1].str, len1 * sizeof( LETTER));
            tbuff[len + len1] = '\0';
            change_line( efile, tbuff, efile->y);
            remove_lines( efile, efile->y + 1, 1);
            }
         else
            efile->message = "No lines to join";
         break;
      case KEY_F(2):                 /* add in a line */
         add_lines( efile, efile->y + 1, 1);
         efile->y++;
         efile->x = efile->left = 0;
         if( efile->y)
            {
            while( efile->x < line->size && (char)line->str[efile->x] == ' ')
               efile->x++;
            if( efile->x == line->size)
               efile->x = 0;
            }
         break;
      case KEY_F(1):
         if( efile->command_loc > -1)
            {
            efile->x = efile->command_loc;
            efile->command_loc = -1;
            }
         efile->y = efile->top + efile->ysize / 2;
         break;
      case KEY_F(25):       /* Ctrl-F1 */
      case KEY_F(26):       /* Ctrl-F2 */
      case KEY_F(27):       /* Ctrl-F3 */
      case KEY_F(28):       /* Ctrl-F4 */
      case KEY_F(29):       /* Ctrl-F5 */
         if( efile->command_loc > -1)
            {
            efile->x = efile->command_loc;
            efile->command_loc = -1;
            }
         efile->y = efile->top + 2 +
                   (efile->ysize - 4) * (key - KEY_F(25)) / 4;
         break;
      case KEY_F(30):      /* Ctrl-F6 */
      case KEY_F(31):      /* Ctrl-F7 */
      case KEY_F(32):      /* Ctrl-F8 */
      case KEY_F(33):      /* Ctrl-F9 */
      case KEY_F(34):      /* Ctrl-F10 */
      case KEY_F(35):      /* Ctrl-F11 */
      case KEY_F(36):      /* Ctrl-F12 */
         if( efile->command_loc > -1)
            efile->command_loc = -1;
         i = key - (key >= KEY_F(35) ? KEY_F(35) - 5 : KEY_F(30));
         efile->x = efile->left + 2 +
                     (efile->xsize - 4) * i / 6;
         break;
      case KEY_F(10):
         *curr_file = efile->next_file;
         break;
      case ALT_B:
         {
         int y = efile->y;

         if( y < 0)
            y = 0;
         if( y >= efile->n_lines)
            y = efile->n_lines - 1;
         if( block_file && !is_block)
            block_file = NULL;
         is_block = 1;
         if( block_file != efile)
            {
            block_file = efile;
            block_y1 = block_y2 = y;
            block_x1 = block_x2 = efile->x;
            }
         else        /* partly lined out already */
            {
            if( efile->x <= block_x1)
               block_x1 = efile->x;
            else if( efile->x >= block_x2)
               block_x2 = efile->x;

            if( y <= block_y1)
               block_y1 = y;
            else if( y >= block_y2)
               block_y2 = y;
            }
         }
         break;
      case 'D' - 64:       /* Ctrl-D */
         {
         extern int show_date_countdown;

         show_date_countdown = 4;
         }
         break;
      case CTL_DEL:
#ifdef CTL_PADSTOP
      case CTL_PADSTOP:
#endif
         {
         int start, end;

         if( !find_word_limits( efile, &start, &end))
            {
            if( start && line->str[start - 1] == ' ')
               start--;
            add_characters( efile, efile->y, start, start - end - 1);
            }
         }
         break;
      case ALT_E:              /* mark current word */
         {
         int start, end;

         if( !find_word_limits( efile, &start, &end))
            {
            if( block_file && !is_block)
               block_file = NULL;
            is_block = 1;
            block_file = efile;
            block_y1 = block_y2 = efile->y;
            block_x1 = start;
            block_x2 = end;
            }
         }
         break;
      case ALT_COMMA:              /* extend curr word left */
         line = efile->lines + block_y1;
         if( is_block && block_file == efile && block_x1 &&
                      block_x1 < line->size)
            {
            block_x1--;
            while( block_x1 && line->str[block_x1 - 1] > ' ')
               block_x1--;
            }
         break;
      case ALT_STOP:              /* extend curr word right */
         line = efile->lines + block_y1;
         if( is_block && block_file == efile && block_x2 &&
                      block_x2 < line->size)
            {
            block_x2++;
            while( block_x2 < line->size - 1 && line->str[block_x2 + 1] > ' ')
               block_x2++;
            }
         break;
      case ALT_L:
         {
         int y = efile->y;

         if( y < 0)
            y = 0;
         if( y >= efile->n_lines)
            y = efile->n_lines - 1;
         is_block = 0;
         if( block_file != efile)
            {
            block_file = efile;
            block_y1 = block_y2 = y;
            }
         else        /* partly lined out already */
            {
            if( y <= block_y1)
               block_y1 = y;
            else if( y >= block_y2)
               block_y2 = y;
            else
               if( y - block_y1 < block_y2 - y)
                  block_y1 = y;
               else
                  block_y2 = y;
            }
         }
         break;
      case ALT_U:
         if( block_file && block_file != efile)
            if( NOT_OVERLAPPED( block_file, efile))
               show_efile( block_file);
         block_file = NULL;
         break;
      case ALT_F:        /* fill area */
         if( block_file && is_block)
            {
            int c, j;

            efile->message = "Enter fill character";
            show_efile( efile);
            c = extended_getch( );
            for( i = block_y1; i <= block_y2; i++)
               {
               reset_line_size( efile, i, block_x2 + 5);
               for( j = 0; j < block_x2 - block_x1 + 1; j++)
                  efile->lines[i].str[block_x1 + j] = (LETTER)c;
               if( efile->lines[i].size < block_x2 + 1)
                  efile->lines[i].size = block_x2 + 1;
               reset_line( efile->lines + i);
               }
            }
         else
            efile->message = "No block to fill";
         break;
      case ALT_C: case ALT_K: case ALT_M:
         efile->message = "Invalid copy operation";
         if( !block_file)
            efile->message = "No block marked";
         else if( !is_block)     /* line move/copy */
            {
            if( efile->y < block_y1 || efile->y >= block_y2 || efile != block_file)
               {
               EFILE *temp;
               int n_lines;

               efile->message = NULL;
               n_lines = block_y2 - block_y1 + 1;
               realloc_lines( efile, efile->n_lines + n_lines);
               add_lines( efile, efile->y + 1, n_lines);
               for( i = 0; i < n_lines; i++)
                  {
                  line = block_file->lines + i + block_y1;
                  if( line->str)
                     line->str[line->size] = '\0';
                  change_line( efile, line->str, efile->y + i + 1);
                  }
               temp = block_file;
               if( key == ALT_M)
                  remove_lines( block_file, block_y1, n_lines);
               if( key == ALT_M || key == ALT_C)
                  block_file = NULL;
               efile->y++;
               if( temp != efile)      /* show file w/o mark */
                  if( NOT_OVERLAPPED( temp, efile))
                     show_efile( temp);
               }
            }
         else                    /* block move/copy */
            {
            if( efile->y <= block_y1 || efile->y >= block_y2
                  || efile->x >= block_x2 || efile != block_file)
               {
               EFILE *temp;
               int n_lines, n_chars, y;

               efile->message = NULL;
               n_lines = block_y2 - block_y1 + 1;
               n_chars = block_x2 - block_x1 + 1;
               realloc_lines( efile, efile->n_lines + n_lines);
               for( i = 0; i < n_lines; i++)
                  {
                  LINE *line_from;
                  int n_copy, copy_loc = block_x1;

                  add_characters( efile, efile->y + i, efile->x, n_chars);
                  line_from = block_file->lines + i + block_y1;
                  n_copy = line_from->size - block_x1;
                  if( n_copy > n_chars)
                     n_copy = n_chars;
                  line = efile->lines + efile->y + i;
                  if( efile->y == block_y1 &&
                                efile->x <= block_x1 && efile == block_file)
                     copy_loc += n_copy;
                  if( n_copy > 0)
                     memcpy( line->str + efile->x,
                           line_from->str + copy_loc, n_copy * sizeof( LETTER));
                  if( efile->x + n_copy > line->size)
                     line->size = efile->x + n_copy;
                  reset_line( line);
                  }
               if( key == ALT_M)   /* delete prev chars */
                  for( y = block_y1; y <= block_y2; y++)
                     add_characters( block_file, y, block_x1,
                                  block_x1 - block_x2 - 1);
               temp = block_file;
               if( key == ALT_M || key == ALT_C)
                  block_file = NULL;
               if( temp != efile)      /* show file w/o mark */
                  if( NOT_OVERLAPPED( temp, efile))
                     show_efile( temp);
               }
            }
         break;
      case ALT_G:
         if( block_file)
            {
            EFILE *temp;
            int y;

            temp = block_file;
            if( is_block)
               {
               for( y = block_y1; y <= block_y2; y++)
                  add_characters( block_file, y, block_x1,
                              block_x1 - block_x2 - 1);
               }
            else        /* line mode */
               remove_lines( block_file, block_y1, block_y2 - block_y1 + 1);
            block_file = NULL;
            if( temp != efile)
               if( NOT_OVERLAPPED( temp, efile))
                  show_efile( temp);
            }
         else
            efile->message = "No block marked";
         break;
      case ALT_D:
         if( efile->command_loc == -1)
            remove_lines( efile, efile->y, 1);
         break;
      case ALT_X:
      case ALT_Y:
/*    case KEY_F(22):              Shift-F10: get directory */
         {
         int start, end;

         if( !find_word_limits( efile, &start, &end))
            {
            char command[100], *comm_loc = command + 2;
            int dest_line = 0;

            strcpy( command, "k ");
            if( key != KEY_F(22) &&
                                 !memcmp( efile->filename, "directory ", 10))
               {
               char *path = efile->filename + 10;

               for( i = strlen( path); i && path[i-1] != '\\'
                                         && path[i-1] != ':'; i--)
               memcpy( comm_loc, path, i);
               comm_loc += i;
               }
            end++;
            for( i = 0; i < end - start; i++)
               comm_loc[i] = (char)line->str[start + i];
            comm_loc[end - start] = '\0';
            if( key == ALT_X || key == ALT_Y)
               for( i = 0; command[i]; i++)
                  if( command[i] == '('
                          || (command[i] == ':' && isdigit( command[i + 1])))
                     {                                /* ')' */
                     dest_line = atoi( command + i + 1);
                     command[i] = '\0';
                     i--;
                     }
            if( key == KEY_F(22))
               {
               memmove( command + 2, command, strlen( command) + 1);
               memcpy( command, "dir", 3);
               if( !strchr( command, '.'))
                  strcat( command, "*.*");
               }
            if( key == ALT_Y)
               {
               FILE *test_file;
               char filename[80];
               int found_unused = 0;

               for( i = 1; i < 100 && !found_unused; i++)
                  {
                  sprintf( filename, "%s%d.doc", command + 2, i);
                  test_file = fopen( filename, "rb");
                  if( !test_file)
                     {
                     sprintf( command + strlen( command), "%d.doc", i);
                     found_unused = 1;
                     }
                  else
                     fclose( test_file);
                  }
               }
            handle_command( curr_file, command);
            if( dest_line)
               {
               sprintf( command, ":%d", dest_line);
               handle_command( curr_file, command);
               }
            }
         }
         break;
      case KEY_BTAB:       /* shift-tab,  I hope? */
         if( efile->command_loc == -1)
            {
            for( i = 29; i && tabs[i] >= efile->x; i--)
               ;
            if( i < 29)
               efile->x = tabs[i];
            else
               efile->x -= 10;
            }
         else
            {
            for( i = 29; i && tabs[i] >= efile->command_loc; i--)
               ;
            efile->command_loc = tabs[i];
            }
         break;
      case KEY_DC:          /* Delete key */
#ifdef PADSTOP
      case PADSTOP:
#endif
         if( efile->command_loc == -1)
            {
            if( efile->x < line->size)
               {
               add_characters( efile, efile->y, efile->x, -1);
               reset_line( line);
               }
            }
         else if( efile->command[efile->command_loc - 1])
            memmove( efile->command + efile->command_loc,
                     efile->command + efile->command_loc + 1,
                     80 - efile->command_loc);
         break;
#if !defined( _WIN32)
      case 127:                     /* backspace */
      case 263:                     /* also backspace */
#endif
      case 8:                       /* backspace */
         if( efile->command_loc == -1)
            {
            if( efile->x > 0)
               {
               add_characters( efile, efile->y, efile->x - 1, -1);
               efile->x--;
               reset_line( efile->lines + efile->y);
               }
            }
         else if( efile->command_loc)
            {
            memmove( efile->command + efile->command_loc - 1,
                 efile->command + efile->command_loc, 80 - efile->command_loc);
            efile->command_loc--;
            }
         break;
      case 13:
      case 10:
#ifdef PADENTER
      case PADENTER:
#endif
         if( efile->command_loc == -1)
            {
            efile->x = 0;
            efile->y++;
            if( efile->y >= efile->n_lines)
               add_lines( efile, efile->n_lines, 1);
            }
         else if( efile->command[0])
            {
            strcpy( prev_command[queue], efile->command);
            efile->command[0] = '\0';
            efile->command_loc = 0;
            handle_command( curr_file, prev_command[queue]);
            queue = (queue + 1) % N_PREV_COMMANDS;
            }
         break;
      case KEY_F(6):
      case KEY_F(18):           /* Shift-F6 */
         if( prev_command[0][0])
            {
            const int offset = ((key == KEY_F(6)) ? N_PREV_COMMANDS - 1 : 1);

            queue = (queue + offset) % N_PREV_COMMANDS;
            while( !prev_command[queue][0])     /* look for a 'real' command */
               queue = (queue + offset) % N_PREV_COMMANDS;
            strcpy( efile->command, prev_command[queue]);
            efile->command_loc = strlen( efile->command);
            }
         break;
#ifdef __WATCOMC__
      case KEY_F(8):     /* show original screens */
         endwin( );
         extended_getch( );
         refresh( );
         full_refresh = 1;
         break;
#endif
      case KEY_F(9):
         handle_command( curr_file,
                prev_command[ (queue + N_PREV_COMMANDS - 1) % N_PREV_COMMANDS]);
         break;
      case 'W' - 64:       /* Ctrl-W */
         if( efile->command_loc == -1)
            efile->x += 16;
         break;
      case 'X' - 64:       /* Ctrl-X */
         if( efile->command_loc == -1)
            efile->x -= 16;
         if( efile->x < 0)
            efile->x = 0;
         break;
#ifdef KEY_RESIZE
      case KEY_RESIZE:
         {
         EFILE *tptr = efile;
         extern int xscr, yscr;     /* size of screen; default xscr=80, yscr=25 */

         resize_term( 0, 0);
         full_refresh = 1;
         efile->message = errbuff;
         xscr = getmaxx( stdscr);
         yscr = getmaxy( stdscr);
         sprintf( errbuff, "Resize: %d %d", xscr, yscr);
         do
            {
            tptr->xsize = xscr;
            tptr->ysize = yscr - 2;
            tptr = tptr->next_file;
            }
            while( tptr != efile);
         }
         break;
#endif
      case 9:                       /* tab */
         {
         extern int tab_xlate;

         if( tab_xlate)
            {
            if( efile->command_loc == -1)
               {
               for( i = 0; i < 29 && tabs[i] <= efile->x; i++)
                  ;
               if( i < 29)
                  efile->x = tabs[i];
               else
                  efile->x += 10;
               }
            break;
            }
         }
      default:                      /* text entered */
         if( key < 256)
            {
            if( efile->command_loc == -1)
               {
               if( efile->y != -1 && efile->y != efile->n_lines)
                  {
                  int x, y;

                  x = efile->x;
                  y = efile->y;
                  efile->dirty = 1;
                  if( x > efile->lines[y].size)
                     reset_line_size( efile, y, x + 5);
                  else
                     reset_line_size( efile, y, efile->lines[y].size + 5);
                  line = efile->lines + y;
                  if( x > line->size)
                     line->size = x;
                  if( insert)
                     {
                     memmove( line->str + x + 1, line->str + x,
                                     (line->size - x) * sizeof( LETTER));
                     line->str[x] = (LETTER)(key | over_attr);
                     line->size++;
                     }
                  else
                     {
                     line->str[x] = (LETTER)(key | over_attr);
                     if( line->size == x)
                        line->size++;
                     }
                  efile->x++;
                  reset_line( line);
                  }
               }
            else      /* is on command line */
               {
               char *loc;

               loc = efile->command + efile->command_loc;
               if( insert)
                  memmove( loc + 1, loc, strlen( loc) + 1);
               else
                  if( !*loc) loc[1] = '\0';
               *loc = (char)key;
               efile->command_loc++;
               }
            }
         else     /* key >= 256 */
            {
            sprintf( errbuff, "Key %d (%x) not interpreted", key, key);
            efile->message = errbuff;
            }
         break;
      }
}
