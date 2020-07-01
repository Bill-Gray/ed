#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#ifdef __WATCOMC__
#include <dos.h>
#include <direct.h>
#else
#ifdef _WIN32
#include <io.h>
#endif
#include <time.h>
#endif
#include "ed.h"
#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

int adjust_paragraph( EFILE *efile, int x, int y);   /* etools2.c */
int find_word_limits( EFILE *efile, int *start, int *end);   /* etools2.c */

static double letter_atof( const LETTER *lptr)
{
   char buff[80];
   size_t i = 0;

   while( *lptr == ' ')
      lptr++;
   while( *lptr >= '-' && *lptr <= 'e' && i < 79)
      buff[i++] = (char)*lptr++;
   buff[i] = '\0';
   return( atof( buff));
}

int sort_column, sort_order, numeric_sort;

int line_compare( const LINE *a, const LINE *b)
{
   LETTER *aptr, *bptr;
   if( a->size <= sort_column)
      return( ( b->size <= sort_column) ? 0 : -sort_order);

   if( b->size <= sort_column)
      return( sort_order);
   aptr = a->str + sort_column;
   bptr = b->str + sort_column;
   if( numeric_sort == 3)        /* case-sensitive comparison */
      {
      while( *aptr && (*aptr == *bptr))
         {
         aptr++;
         bptr++;
         }
      return( (*aptr - *bptr) * sort_order);
      }
   if( numeric_sort)
      {
      double aval = letter_atof( aptr);
      double bval = letter_atof( bptr);

      if( numeric_sort == 2)
         {
         aval = fabs( aval);
         bval = fabs( bval);
         }
      if( aval > bval)
         return( sort_order);
      else if( aval < bval)
         return( -sort_order);
      else
/*       return( sort_order * strcmp( aptr, bptr));      */
         return( 0);
      }
   while( *aptr && (toupper( *aptr) == toupper( *bptr)))
      {
      aptr++;
      bptr++;
      }
   return( (toupper( *aptr) - toupper( *bptr)) * sort_order);
}

int my_qsort( char *data, const unsigned n_elements, const unsigned element_size,
                     int (*compare)(void const *elem1, void const *elem2));
int my_mergesort( void *data, unsigned n_elements, unsigned elem_size,
                     int (*compare)(void const *elem1, void const *elem2));

int sort_file( EFILE *efile, int start_x, int start_y, int n_lines, int order)
{
   int i, gap;
   LINE *lines;

   sort_order = order;
   if( start_y == -1)
      start_y++;
   if( start_y + n_lines > efile->n_lines)
      n_lines = efile->n_lines - start_y;
   if( n_lines <= 0)
      return( -1);
   lines = efile->lines + start_y;
   for( i = 0; i < n_lines; i++)
      if( lines[i].str)
         lines[i].str[ lines[i].size] = '\0';
   sort_column = start_x;
#ifndef ORIGINAL_SHELL_SORT_CODE
   gap = 4;
   while( gap < n_lines)
      gap = gap * 8 / 3 + 1;
   while( gap)          /* Shell sort the lines */
      {
      int j;
      LINE tline, *tptr1, *tptr2;

      for( i=gap; i<n_lines; i++)
         for( j=i-gap; j>=0 &&
              line_compare(tptr1 = lines + j, tptr2 = lines + j + gap) > 0;
                              j -= gap)
            {
            memcpy( (void *)&tline, (void *)tptr1, sizeof( LINE));
            memcpy( (void *)tptr1, (void *)tptr2, sizeof( LINE));
            memcpy( (void *)tptr2, (void *)&tline, sizeof( LINE));
            }
      gap = gap * 3 / 8;
      }
#else
   my_mergesort( (char *)lines, n_lines, sizeof( LINE), line_compare);
#endif
   efile->dirty = 1;
   numeric_sort = 0;
   return( 0);
}

#ifdef _WIN32
#ifdef __WATCOMC__
EFILE *make_dir_file( const char *path)
{
   struct find_t c_file;
   EFILE *efile;
   int rval, i, line_no = 0;
   char str[80], fullpath[_MAX_PATH];
   LETTER zstr[80];
   char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];

   _splitpath( path, drive, dir, fname, ext);
   if( !*fname)
      strcpy( fname, "*");
   if( !*ext)
      strcpy( ext, ".*");
   _makepath( fullpath, drive, dir, fname, ext);

   rval = _dos_findfirst( fullpath, _A_SUBDIR |
                                   _A_HIDDEN | _A_SYSTEM, &c_file);
   if( rval)      /* no match */
      return( NULL);

   sprintf( str, "^&@.ZX!** %s", fullpath);

   efile = read_efile( str);
   memcpy( efile->filename, "directory", 9);

   while( !rval)
      {
      unsigned date, time, year;

      date = c_file.wr_date;
      time = c_file.wr_time;

      year = (date >> 9) + 1980;
      sprintf( str, "%-16s %10ld     %4d %02d %02d   %2d:%02d:%02d",
                     c_file.name, c_file.size,
                     year, (date >> 5) % 16,
                     date & 0x1f,
                     time >> 11, (time >> 5) & 0x3f,
                     (time & 0x1f) << 1);
      if( c_file.attrib & _A_SUBDIR) memcpy( str + 17, "   <dir>   ", 11);
      for( i = 0; str[i]; i++)
         zstr[i] = (LETTER)str[i];
      zstr[i] = 0;
      change_line( efile, zstr, line_no);
      rval = _dos_findnext( &c_file);
      line_no++;
      }
   sort_file( efile, 0, 0, line_no, 1);
   efile->dirty = 0;
   return( efile);
}
#else       /* non-OpenWATCOM version follows: */
EFILE *make_dir_file( const char *path)
{
   struct _finddata_t c_file;
   EFILE *efile;
   int rval = 0, i, line_no = 0;
   char str[180], fullpath[_MAX_PATH];
   LETTER zstr[180];
   char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
   long hFile;

   _splitpath( path, drive, dir, fname, ext);
   if( !*fname)
      strcpy( fname, "*");
   if( !*ext)
      strcpy( ext, ".*");
   _makepath( fullpath, drive, dir, fname, ext);

   hFile = _findfirst( fullpath, &c_file);
   if( hFile == -1)      /* no match */
      return( NULL);

   sprintf( str, "^&@.ZX!** %s", fullpath);

   efile = read_efile( str);
   memcpy( efile->filename, "directory", 9);

   while( !rval)
      {
      char tbuff[40];

      sprintf( str, "%-16s %10ld    ", c_file.name, c_file.size);
      strcpy( tbuff, ctime( &c_file.time_write));
      strcat( str, tbuff);
      if( c_file.attrib & _A_SUBDIR)
         memcpy( str + 17, "   <dir>   ", 11);
      for( i = 0; str[i]; i++)
         zstr[i] = (LETTER)str[i];
      zstr[i] = 0;
      change_line( efile, zstr, line_no);
      rval = _findnext( hFile, &c_file);
      line_no++;
      }
   _findclose( hFile);
   efile->dirty = 0;
   return( efile);
}
#endif
#endif

void reset_line( LINE *line)
{
   LETTER *tptr = line->str + line->size - 1;

   while( line->size > 0 && (*tptr == ' ' || *tptr == 13 || *tptr == 10))
      {
      line->size--;
      tptr--;
      }
}

int correct_efile( EFILE *efile)
{
   extern int centering;

   if( efile->x < 0)
      efile->x = 0;
   while( efile->left > efile->x)
      efile->left = efile->x - efile->x % (efile->xsize / 2);
   while( efile->left + efile->xsize <= efile->x)
      efile->left += efile->xsize;
   if( efile->y < -1)
      efile->y = -1;
   if( efile->y > efile->n_lines)
      efile->y = efile->n_lines;
   if( efile->top > efile->y - centering)
      efile->top = efile->y - centering;
   if( efile->top < -2)
      efile->top = -2;
   if( efile->top < efile->y - efile->ysize + centering)
      efile->top = efile->y - efile->ysize + centering;
   if( efile->top > efile->n_lines - efile->ysize + 2)
      efile->top = efile->n_lines - efile->ysize + 2;
   return( 0);
}

static const char *whats = "[]{}()<>";

#define COMMENT_START 8
#define COMMENT_END   9

int whatsis_idx( const LINE *line, int x)
{
   int i;
   LETTER *tptr;

   if( x > line->size - 1)
      return( -1);
   tptr = line->str + x;
   line->str[line->size] = '\0';
   if( *tptr == '/' && x && tptr[-1] == '*')
      return( COMMENT_END);
   if( *tptr == '*' && tptr[1] == '/')
      return( COMMENT_END);
   if( *tptr == '*' && x && tptr[-1] == '/')
      return( COMMENT_START);
   if( *tptr == '/' && tptr[1] == '*')
      return( COMMENT_START);
   for( i = 0; whats[i] && whats[i] != (char)*tptr; i++)
      ;
   if( !whats[i])
      i = -1;
   else if( x && x < line->size)
      if( (char)tptr[-1] == (char)tptr[1])
         if( (char)tptr[1] == '\'' || (char)tptr[1] == '"')
            i = -1;
   return( i);
}

int find_occurrence_and_move( EFILE *efile, const char *search,
      const int case_sensitive, const int direction, const int blocking_mode);

#ifdef DOESNT_WORK_VERY_WELL
static int find_matching_html_tag( EFILE *efile)
{
   LINE *line = efile->lines + efile->y;

   if( efile->y >= 0 && efile->y < efile->n_lines && line
            && efile->x + 1 < line->size
            && line->str[efile->x + 1] == '>')
      {
      int x = efile->x, len, dir;
      char tag[20];

      while( x && line->str[x] != '<')
         x--;
      if( line->str[x] == '<' && efile->x - x < 15)
         {
         if( line->str[x + 1] == '/')  /* closing tag */
            {
            len = efile->x - x;
            memcpy( tag, line->str + x, len);
            dir = -1;
            }
         else           /* assume starting tag; */
            {           /* create matching closing tag */
            len = efile->x - x + 1;
            memcpy( tag + 1, line->str + x, len - 1);
            tag[1] = '/';
            dir = 1;
            }
         tag[0] = '<';
         tag[len] = '\0';
         return( find_occurrence_and_move( efile, tag, 0, dir, 0));
         }
      }
   return( 0);
}
#endif

int find_matching_whatsis( EFILE *efile)
{
   char c1, c;
   LINE *line;
   int i, x, y, count = 1;

   line = efile->lines + efile->y;
   if( efile->y >= 0 && efile->y < efile->n_lines)
      i = whatsis_idx( line, efile->x);
   else
      i = -1;
   if( i == -1)
      {
      if( efile->y >= 0 && efile->y < efile->n_lines && line)
         {
         x = 0;
         while( x < line->size && line->str[x] == ' ')
            x++;
         if( x == efile->x)      /* already there; move to */
            efile->x = line->size;   /* end of the line */
         else
            efile->x = x;
         return( 0);
         }
      efile->message = "Can't match that character";
      return( -1);
      }

   if( i == COMMENT_START)
      return( find_occurrence_and_move( efile, "*/", 0, 1, 0));
   if( i == COMMENT_END)
      return( find_occurrence_and_move( efile, "/*", 0, -1, 0));

   c = whats[i];
   c1 = whats[i ^ 1];
   if( i & 1)     /* search backwards */
      {
      for( y = efile->y; y > -1; y--)
         if( efile->lines[y].str)
            {
            line = efile->lines + y;
            if( y == efile->y)
               x = efile->x;
            else
               x = line->size;
            line->str[line->size] = '\0';
            while( x--)
               if( (char)line->str[x] == c &&
                                   whatsis_idx( line, x) != -1)
                  count++;
               else if( (char)line->str[x] == c1 &&
                                   whatsis_idx( line, x) != -1)
                  {
                  count--;
                  if( !count)
                     {
                     efile->x = x;
                     efile->y = y;
                     return( 0);
                     }
                  }
            }
      }
   else        /* search forward */
      for( y = efile->y; y < efile->n_lines; y++)
         if( efile->lines[y].str)
            {
            line = efile->lines + y;
            if( y == efile->y)
               x = efile->x;
            else
               x = -1;
            line->str[line->size] = '\0';
            while( ++x < line->size)
               if( (char)line->str[x] == c &&
                                   whatsis_idx( line, x) != -1)
                  count++;
               else if( (char)line->str[x] == c1 &&
                                   whatsis_idx( line, x) != -1)
                  {
                  count--;
                  if( !count)
                     {
                     efile->x = x;
                     efile->y = y;
                     return( 0);
                     }
                  }
            }
   efile->message = "No match found";
   return( -2);
}

static int adjust_line( LINE *line)
{
   while( line->size && line->str[line->size - 1] == ' ')
      line->size--;
   return( line->size);
}

int adjust_paragraph( EFILE *efile, int x, int y)
{
   int len1, len2 = 1;
   LINE *line;

   while( y >= 0 && y < efile->n_lines - 1 && len2)
      {
      len1 = adjust_line( efile->lines + y);
      len2 = adjust_line( efile->lines + y + 1);
      if( len2)
         {
         LETTER *tbuff;

         tbuff = (LETTER *)calloc( len1 + len2 + 2,  sizeof( LETTER));
         memcpy( tbuff, efile->lines[y].str, len1 * sizeof( LETTER));
         tbuff[len1] = ' ';
         memcpy( tbuff + len1 + 1, efile->lines[y + 1].str,
                                              len2 * sizeof( LETTER));
         change_line( efile, tbuff, y);
         free( tbuff);
         remove_lines( efile, y + 1, 1);
         }
      }
   if( y >= 0 && y < efile->n_lines)
      while( efile->lines[y].size > x)
         {
         add_lines( efile, y + 1, 1);
         line = efile->lines + y;
         len1 = x;
         while( len1 && line->str[len1] != ' ')
            len1--;
         if( !len1)     /* can't do anything here */
            return( -1);
         line->str[line->size] = (LETTER)0;
         line->str[len1] = (LETTER)0;
         line->size = len1;
         change_line( efile, line->str + len1 + 1, y + 1);
         y++;
         }
   return( 0);
}

int find_word_limits( EFILE *efile, int *start, int *end)
{

   LINE *line;
   int len;

   line = efile->lines + efile->y;
   if( efile->y < 0 || efile->y >= efile->n_lines || line->size <= 0)
      return( -1);

   len = line->size - 1;
   if( len > efile->x)
      len = efile->x;
   *start = *end = len;
   while( *start && line->str[*start - 1] > ' ')
      (*start)--;
   while( *end < line->size - 1 && line->str[*end + 1] > ' ')
      (*end)++;
   return( 0);
}
