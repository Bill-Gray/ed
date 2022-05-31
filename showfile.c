#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "ed.h"

#if defined( _WIN32)
   #define PDC_DLL_BUILD
#endif

#if defined( PDC_WIDE) || defined( __WATCOMC__)
   #include "curses.h"
#else
   #define _XOPEN_SOURCE_EXTENDED 1
   #include <ncursesw/curses.h>
#endif

#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

int color[24];

int xscr = 80, yscr = 25, display_mode = 0, full_refresh = 0;

extern int block_x1, block_y1, block_x2, block_y2, is_block;
extern EFILE *block_file;
extern int insert;

typedef unsigned scrchar_t;

static scrchar_t *old_scr, *new_scr;

#define ATTR_SHIFT 21
#define CHAR_MASK ((1u << ATTR_SHIFT) - 1u)

int correct_efile( EFILE *efile);

static void update_scr( void)
{
   int i = 0, end, j, prev_loc = -1;
   scrchar_t prev_attr = (scrchar_t)-1;

   if( full_refresh)
      {
      if( old_scr)
         memset( old_scr, 0, xscr * yscr * sizeof( scrchar_t));
      full_refresh = 0;
      }

   while( i < xscr * yscr - 1)
      {
      scrchar_t attr;

      while( i < xscr * yscr - 1 && old_scr[i] == new_scr[i])
         i++;              /* skip over areas that didn't change */
      end = i;
      attr = new_scr[i] >> ATTR_SHIFT;
      while( end < xscr * yscr - 1 && old_scr[end] != new_scr[end] &&
           end - i < xscr - 10 && (new_scr[end] >> ATTR_SHIFT) == attr
           && end / xscr == i / xscr)
         end++;
      if( i != prev_loc)
         move( i / xscr, i % xscr);
      prev_loc = end;
      if( attr != prev_attr)
         attrset( COLOR_PAIR( attr & 15));
      prev_attr = attr;
      for( j = 0; j < end - i; j++)
         {
#ifdef PDC_WIDE
         wchar_t c_out = (wchar_t)( new_scr[j + i] & CHAR_MASK);
#else
         char c_out = (char)( new_scr[j + i] & CHAR_MASK);
#endif

         if( !c_out || c_out == 9 || c_out == 13)
            c_out = ' ';
#ifdef PDC_WIDE
         addnwstr( &c_out, 1);
#else
         addnstr( &c_out, 1);
#endif
         }
      i = end;
      }
   memcpy( old_scr, new_scr, xscr * yscr * sizeof( scrchar_t));
   refresh( );
}

int show_date_countdown;

void update_clock( void)
{
#ifdef TEMP_REMOVE
   if( new_scr)
      {
      time_t t = time( NULL);
      char *timebuff = ctime( &t);
      int i, temp_yscr = yscr;

      if( show_date_countdown)
         {
         show_date_countdown--;
         for( i = 0; i < 10; i++)
            new_scr[(64 + i) * 2] = timebuff[i];
         }
      else
         for( i = 0; i < 8; i++)
            new_scr[(64 + i) * 2] = timebuff[11 + i];
      yscr = 1;
      update_scr( );
      yscr = temp_yscr;
      }
#endif
}

int cursor_choice[2] = { 1, 2};

#define SHOW_INSERT_DELETE_VIA_COLOR       1

void show_efile( EFILE *efile)
{
   char buf[92], *timebuff;
   const char *s = NULL;
   scrchar_t *display_loc, *screen_start, attr;
   LETTER *lett;
   int i, line, width, cursor_x, cursor_y, size_needed;
   time_t t;
   static int size_alloced = 0;

/* snprintf( buf, sizeof( buf), "be %s", efile->filename);
   PDC_set_title( buf);       */
   size_needed = xscr * yscr + 1;
   if( !efile || size_needed > size_alloced)
      {
      if( new_scr)
         free( new_scr);
      if( old_scr)
         free( old_scr);
      old_scr = new_scr = NULL;
      if( !efile)
         return;
      }
   correct_efile( efile);     /* correct margins, etc */
   if( !new_scr)              /* allow for a "pretty big" screen */
      {
      old_scr = (scrchar_t *)calloc( size_needed, sizeof( scrchar_t));
      new_scr = (scrchar_t *)calloc( size_needed, sizeof( scrchar_t));
      size_alloced = size_needed;
      }

   screen_start = new_scr + xscr * efile->ystart + efile->xstart;
   if( !efile->message)
      {
      char tbuff[40];
#ifndef _WIN32
      const char *home = getenv( "HOME");
      const size_t home_len = strlen( home);

      if( !strncmp( efile->filename, home, home_len))
         {
         *tbuff = '~';
         strncpy( tbuff + 1, efile->filename + home_len, 29);
         }
      else
#endif
         strncpy( tbuff, efile->filename, 30);
      tbuff[30] = '\0';
      snprintf( buf, sizeof( buf), "%-30sLine=%-6dCol=%-6dSize=%-16d",
                     tbuff, efile->y + 1, efile->x + 1, efile->n_lines);
      }
   else
      snprintf( buf, sizeof( buf), "%-80s", efile->message);
   t = time( NULL);
   timebuff = ctime( &t);
   memcpy( buf + 64, timebuff + 11, 8);
   if( efile->ystart < yscr)
      width = xscr;
   else
      width = 80;
   attr = ((scrchar_t)color[efile->message ? 1 : 0] << ATTR_SHIFT);
   for( i = 0; buf[i] && i < efile->xsize; i++)
      screen_start[(i - width)] = (scrchar_t)buf[i] | attr;
   while( i < efile->xsize)
      {
      screen_start[(i - width)] = ' ' | attr;
      i++;
      }
#ifndef TEMP_REMOVE
   screen_start[-2] = (char)(efile->dos_mode ? 'D' : 'U');
   if( efile->next_file != efile)        /* more than one file in ring */
      {
      EFILE *temp;

      for( i = 1, temp = efile; temp->next_file != efile; temp = temp->next_file)
         i++;
      screen_start[-4] = (char)(i + '0');
      }
   screen_start[-6] = (char)(efile->dirty ? 'd' : 'c');
#endif
   for( i = 0; i < efile->ysize; i++)
      {
      int len, highlight_block = 0, mono = 0, j;

      display_loc = new_scr + width  * ( i + efile->ystart) + efile->xstart;
      if( i + efile->ystart >= yscr)         /* mono line */
         mono = 8;
      line = i + efile->top;
      lett = NULL;
      if( line >= 0 && line < efile->n_lines)
         {
         reset_line( efile->lines + line);
         lett = efile->lines[line].str;
         len = efile->lines[line].size;
         }
      else
         len = 0;
      attr = color[mono + 2];       /* normal */
      if( line == -1)
         {
         s = "* * * Top of File * * *";
         len = 23;
         attr = color[mono + 7];
         }
      if( line == efile->n_lines)
         {
         s = "* * * End of File * * *";
         len = 23;
         attr = color[mono + 7];
         }
      if( block_file == efile && line >= block_y1 && line <= block_y2)
         {
         if( !is_block)
            attr = color[mono + 3];
         else
            highlight_block = 1;
         }

      if( line < -1 || line > efile->n_lines)
         for( j = 0; j < efile->xsize; j++)
            *display_loc++ = (scrchar_t)' ';
      else
         {
         int remains;

         len -= efile->left;
         if( len < 0) len = 0;
         if( len > efile->xsize) len = efile->xsize;
         remains = efile->xsize - len;
         if( lett)
            {
            lett += efile->left;
            while( len--)
               *display_loc++ = (scrchar_t)*lett++ | (attr << ATTR_SHIFT);
            }
         else
            {
            s += efile->left;
            while( len--)
               *display_loc++ = (scrchar_t)*s++ | (attr << ATTR_SHIFT);
            }
         while( remains--)
            *display_loc++ = ' ' | (attr << ATTR_SHIFT);
         if( highlight_block)
            {
            display_loc -= efile->xsize;
            for( j = 0; j < efile->xsize; j++)
               if( efile->left + j >= block_x1 && efile->left + j <= block_x2)
                  {
                  display_loc[j] &= CHAR_MASK;  /* remove attribute... */
                  display_loc[j] |= (scrchar_t)color[mono + 3] << ATTR_SHIFT;
                  }                 /* ...and reset it to 'block' attr */
            }
         }
      }     /* so much for the text data...now show dividing line */

   display_loc = new_scr + width * (efile->ystart + efile->ysize) + efile->xstart;
   if( efile->ystart + efile->ysize >= yscr)    /* mono */
      attr = color[12] << ATTR_SHIFT;
   else
      attr = color[4] << ATTR_SHIFT;

   for( i = 0; efile->command[i] && i < efile->xsize; i++)
      display_loc[i] = efile->command[i] | attr;
   while( i < efile->xsize)
      display_loc[i++] = ' ' | attr;

   if( efile->command_loc < 0)     /* cursor is on text,  so show it */
      {
      cursor_x = efile->xstart + efile->x - efile->left;
      cursor_y = efile->ystart + efile->y - efile->top;
#ifdef SHOW_INSERT_DELETE_VIA_COLOR
      display_loc = new_scr + (width * cursor_y + cursor_x);

      *display_loc &= CHAR_MASK;
      *display_loc |= ((scrchar_t)color[5 + insert] << ATTR_SHIFT);
#endif
      }
   else              /* cursor is on command line */
      {
      cursor_x = efile->xstart + efile->command_loc;
      cursor_y = efile->ystart + efile->ysize;
#ifdef SHOW_INSERT_DELETE_VIA_COLOR
      display_loc[efile->command_loc] &= CHAR_MASK;
      display_loc[efile->command_loc] |= ((scrchar_t)color[5 + insert] << ATTR_SHIFT);
#endif
      move( cursor_y, cursor_x);
      }
   update_scr( );
/* curs_set( cursor_choice[insert]);   */
#ifdef SHOW_INSERT_DELETE_VIA_COLOR
   move( 0, 29);
#else
   curs_set( insert ? 2 : 1);
   move( cursor_y, cursor_x);
#endif
}
