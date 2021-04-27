#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#if defined( _WIN32)
   #define PDC_DLL_BUILD
#endif

#include "curses.h"
#ifdef _WIN32
 #include <io.h>
 #include <dos.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "ed.h"
#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

EFILE *make_dir_file( const char *path);
void set_video_mode( unsigned mode);      /* in BE.C */
int sort_file( EFILE *efile, int start_x, int start_y, int n_lines, int order);
void use_key( EFILE **curr_file, int key);
int realloc_lines( EFILE *efile, int n);
int xlate_key_string( const char *string, int *len);     /* keyhand.c */

extern int display_mode;      /* in SHOWFILE.C */
extern int full_refresh;      /* in SHOWFILE.C */
extern int color[];
extern int max_line_length;   /* in ETOOLS.C */
int default_dos_mode, tabs[60], centering = 2;
int n_redefs = 0;
int tab_xlate = 3;
char **redefs_from, **redefs_to;
extern int xscr, yscr;     /* size of screen; default xscr=80, yscr=25 */

static int find_in_string( const LETTER *s, int max, const char *search,
                   const int case_sensitive, const int direction)
{
   int j, loc;
   const int len = strlen( search);

   if( !s)
      return( -1);
   max -= len - 1;
   if( max <= 0)
      return( -1);
   loc = ((direction == 1) ? 0 : max - 1);
   if( !case_sensitive)
      {
      while( max--)
         {
         assert( loc >= 0);
         if( tolower( (char)s[loc]) == tolower( *search))
            {
            for( j = 0; j < len && tolower( (char)s[loc + j]) ==
                                   tolower( search[j]); j++)
               ;
            if( j == len)
               return( loc);
            }
         loc += direction;
         }
      }
   else
      {
      while( max--)
         {
         assert( loc >= 0);
         if( (char)s[loc] == *search)
            {
            for( j = 0; j < len && (char)s[loc + j] == search[j]; j++)
               ;
            if( j == len)
               return( loc);
            }
         loc += direction;
         }
      }
   return( -1);
}

#if !defined( _WIN32)
int _memicmp( const char *s1, const char *s2, int n)
{
   int c1, c2;

   while( n--)
      {
      if( (c1 = tolower( *s1++)) != (c2 = tolower( *s2++)))
         return( c1 - c2);
      }
   return( 0);
}

int _stricmp( const char *s1, const char *s2)
{
   int c1, c2;

   while( *s1 || *s2)
      {
      if( (c1 = tolower( *s1++)) != (c2 = tolower( *s2++)))
         return( c1 - c2);
      }
   return( 0);
}
#endif

static int in_block( const EFILE *efile, const int x, const int y)
{
   extern int block_x1, block_y1, block_x2, block_y2, is_block;
   extern EFILE *block_file;
   int rval = 1;

   if( block_file == efile)
      {
      if( y < block_y1 || y > block_y2)
         rval = 0;
      else if( is_block && (x < block_x1 || x > block_x2))
         rval = 0;
      }
   return( rval);
}

static int letter_atoi( const LETTER *lptr)
{
   int rval = 0;

   while( *lptr >= '0' && *lptr <= '9')
      rval = rval * 10 + (*lptr++ - '0');
   return( rval);
}

static int find_occurrence( const EFILE *efile, const char *search, int *x, int *y,
        const int case_sensitive, const int direction, const int blocking_mode)
{
   int x1, y1;
   LINE *line;

   assert( efile);
   if( *y < 0)
      (*y)++;
   if( *y >= efile->n_lines)
/*    return( -1);                  end of file */
      *y = efile->n_lines - 1;
   line = efile->lines + *y;
   if( direction == 1)
      {
      *x += 1;
      if( line->str &&
                 (x1 = find_in_string( line->str + *x, line->size - *x,
                              search, case_sensitive, 1)) > -1)
         if( !blocking_mode || in_block( efile, *x + x1, *y))
            {
            *x = *x + x1;
            return( 0);             /* success */
            }
      for( y1 = *y + 1; y1 < efile->n_lines; y1++)
         {
         x1 = find_in_string( efile->lines[y1].str, efile->lines[y1].size,
                                            search, case_sensitive, 1);
         if( x1 > -1)
            if( !blocking_mode || in_block( efile, x1, y1))
               {
               *x = x1;
               *y = y1;
               return( 0);
               }
         }
      }
   else           /*  direction = -1,  and we search backwards */
      {
      *x -= 1;
      if( (x1 = find_in_string( line->str, *x, search,
                                             case_sensitive, -1)) > -1)
         if( !blocking_mode || in_block( efile, x1, *y))
            {
            *x = x1;
            return( 0);             /* success */
            }
      for( y1 = *y - 1; y1 >= 0; y1--)
         {
         line--;
         x1 = find_in_string( line->str, line->size,
                                            search, case_sensitive, -1);
         if( x1 > -1)
            if( !blocking_mode || in_block( efile, x1, y1))
               {
               *x = x1;
               *y = y1;
               return( 0);
               }
         }
      }
   return( -1);
}

static size_t letter_strlen( const LETTER *lptr)
{
   size_t rval = 0;

   while( *lptr++)
      rval++;
   return( rval);
}

int find_occurrence_and_move( EFILE *efile, const char *search,
      const int case_sensitive, const int direction, const int blocking_mode)
{
   int x = efile->x, y = efile->y;
   int rval = find_occurrence( efile, search, &x, &y, case_sensitive,
                            direction, blocking_mode);

   if( !rval)
      {
      efile->x = x;
      efile->y = y;
      efile->top = efile->y - efile->ysize / 2;
      efile->command_loc = -1;
      }
   else
      efile->message = "String not found";
   return( rval);
}

static int html_to_txt( LINE *line, int curr_state)
{
   int i, j;

   for( i = j = 0; i < line->size; i++)
      {
      if( line->str[i] == '<')
         curr_state = 1;
      if( curr_state == 0)
         line->str[j++] = line->str[i];
      if( line->str[i] == '>')
         curr_state = 0;
      }
   line->str[j] = '\0';
   for( i = 0; line->str[i]; i++)
      if( line->str[i] == '&')
         {
         int k, len;
         const char *replacers[8] = { "&nbsp;", " ", "&quot;", "\"",
                             "&lt", "<",  "&gt", ">" };

         for( k = 0; k < 8; k += 2)
            if( !memcmp( replacers[k], line->str + i, len = strlen( replacers[k])))
               {
               line->str[i] = *replacers[k + 1];
               memmove( line->str + i + 1,
                        line->str + i + len,
                        letter_strlen( line->str + i) * sizeof( LETTER));
               }
         }
   line->size = letter_strlen( line->str);
   return( curr_state);
}

int insert_file( EFILE *efile, const char *filename)
{
   EFILE *get_file = read_efile( filename);

   if( efile && get_file)
      {
      int i;

      realloc_lines( efile, efile->n_lines + get_file->n_lines);
      add_lines( efile, efile->y + 1, get_file->n_lines);
      for( i = 0; i < get_file->n_lines; i++)
         {
         LINE *line = get_file->lines + i;

         if( line->str)
            line->str[line->size] = '\0';
         change_line( efile, line->str, efile->y + i + 1);
         }
      efile->y++;
      free_efile( get_file);
      }
   return( get_file ? 0 : -1);
}

#ifdef DIDNT_WORK_OUT
static int mc_visa_checksum( LETTER *str)
{
   int i, checksum = 0, add_in;

   if( !str)
      return( 0);
   for( i = 0; i < 24; i++)
      if( !str[i])
         return( 0);

   for( i = 0; i < 3; i++)
      if( (char)str[i] != ' ')
         return( 0);
   if( (char)str[3] == 'M' && (char)str[4] == 'C' && (char)str[5] == ' ')
      str += 6;
   else if( (char)str[3] == 'V' && (char)str[4] == 'I' && (char)str[5] == 'S'
         && (char)str[6] == 'A' && (char)str[7] == ' ')
      str += 8;
   else
      return( 0);
   for( i = 0; i < 16; i++, str++)
      if( (char)*str >= '0' && (char)*str <= '9')
         {
         add_in = *str - '0';
         if( !(i & 1))
            add_in *= 2;
         if( add_in > 9)
            add_in -= 9;
         checksum += add_in;
         if( (i & 3) == 3)
            str++;
         }
      else
         return( 0);
   return( (checksum % 10) ? 3 : 0);
}
#endif

int handle_command( EFILE **curr_file, const char *comm)
{
   EFILE *efile;
   int i;
   char recomm[100];

   if( *comm == ';')    /* comment */
      return( 0);

   for( i = 0; i < n_redefs; i++)
      {
      int len = strlen( redefs_from[i]);

      if( !_memicmp( redefs_from[i], comm, len) &&
                   (comm[len] == ' ' || !comm[len]))
         {
         strcpy( recomm, redefs_to[i]);
         strcat( recomm, comm +len);
         comm = recomm;
         i = n_redefs;
         }
      }

   if( curr_file)
      efile = *curr_file;
   else
      efile = NULL;

   if( ( !strcmp( comm, "?") || !_stricmp( comm, "file"))   /* check for mismatches */
                  && efile)
      {
      const int prev_x = efile->x;
      const int prev_y = efile->y;
      int x, y, overridden = 0;
      LETTER *s;

      for( y = 0; !overridden && y < efile->n_lines; y++)
         {
         for( x = efile->lines[y].size - 1, s = efile->lines[y].str; x >= 0; x--)
            if( strchr( "{}[]()", (char)s[x]))
               {
               efile->x = x;
               efile->y = y;
               if( find_matching_whatsis( efile) == -2)
                  {
                  efile->command_loc = -1;
                  return( -1);
                  }
               }
            else if( (char)s[x] == 'N' && !memcmp( s + x, "No-?", 4))
               overridden = 1;
#ifdef TRY_REMOVING_THIS
         if( x = mc_visa_checksum( efile->lines[y].str))
            {
            efile->x = x;
            efile->y = y;
            efile->command_loc = -1;
            return( -1);
            }
#endif
         }
      efile->x = prev_x;
      efile->y = prev_y;
      if( comm[0] == '?')
         {
         efile->message = "Checks out OK";
         return( 0);
         }
      }

   if( efile && !_memicmp( comm, "rev ", 4))
      {
      int i = efile->y, j = efile->y + atoi( comm + 4) - 1;

      if( j >= efile->n_lines)
         j = efile->n_lines - 1;
      while( i < j)
         {
         LINE tline = efile->lines[i];

         efile->lines[i] = efile->lines[j];
         efile->lines[j] = tline;
         i++;
         j--;
         }
      return( 0);
      }

   if( efile && !_memicmp( comm, "numb ", 5))
      {
      LETTER *tptr = efile->lines[efile->y].str + efile->x;
      int i, j, line0 = letter_atoi( tptr), len;
      int n_lines = 30000, step = 1;
      char tbuff[13];

      sscanf( comm + 5, "%d %d", &n_lines, &step);
      for( len = 0; tptr[len] >= '0' && tptr[len] <= '9'; len++)
         ;
      for( i = efile->y; i < efile->y + n_lines && i < efile->n_lines;
                                                   i++, line0 += step)
         {
         LETTER *tlett;

         snprintf( tbuff, sizeof( tbuff), "%08ld", (long)line0);
         reset_line_size( efile, i, efile->x + len + 5);
         tlett = efile->lines[i].str + efile->x;
         for( j = 0; j < len; j++)
            *tlett++ = tbuff[j + 8 - len];
         if( efile->lines[i].size < efile->x + len)
            efile->lines[i].size = efile->x + len;
         }
      return( 0);
      }

#if defined( _WIN32)
   if( !_memicmp( comm, "resize ", 7))
      {
      int ymin, ymax, xmin, xmax;

      if( sscanf( comm + 7, "%d,%d,%d,%d", &ymin, &ymax, &xmin, &xmax) == 4)
         {
         PDC_set_resize_limits( ymin, ymax, xmin, xmax);
         efile->message = "Resize limits reset";
         }
      else
         efile->message = "Resize limits NOT reset";
      return( 0);
      }
#endif
   if( efile && !_memicmp( comm, "mac ", 4))
      {
      FILE *ifile = fopen( comm + 4, "rb");

      if( ifile)
         {
         while( fgets( recomm, 100, ifile))
            handle_command( curr_file, recomm);
         fclose( ifile);
         }
      else
         efile->message = "Macro file not found";
      return( 0);
      }

   if( !_memicmp( comm, "key ", 4))
      {
      char c;
      const char *tptr = comm + 4;

      while( (c = *tptr++) != 0)
         {
         if( c == '#')
            {
            int len, xlate = xlate_key_string( tptr, &len);

            tptr += len;
            use_key( curr_file, xlate);
            }
         else if( c != ' ')
            {
            if( c == '_')
               c = ' ';
            use_key( curr_file, c);
            }
         }
      return( 0);
      }

#if !defined( _WIN32)
   if( !_memicmp( comm, "display=", 8))
      {
      set_video_mode( atoi( comm + 8));
      if( efile)
         {
         efile->xsize = xscr;
         efile->ysize = yscr - 2;
         }
      return( 0);
      }
#endif

   if( !_memicmp( comm, "maxlen=", 7))
      {
      max_line_length = atoi( comm + 7);
      return( 0);
      }

   if( !_memicmp( comm, "cursor ", 7))
      {
      extern int cursor_choice[];

      sscanf( comm + 7, "%d %d", cursor_choice, cursor_choice + 1);
      return( 0);
      }

   if( efile && !_stricmp( comm, "scr 2 v"))
      {
      efile->xsize = xscr / 2;
      efile->ysize = yscr - 2;
      efile->ystart = 1;
      efile->xstart = 0;
      efile->left = 0;
      efile->top = -1;
      full_refresh = 1;
      show_efile( efile);
      efile = efile->next_file;
      efile->xsize = xscr / 2;
      efile->ysize = yscr - 2;
      efile->ystart = 1;
      efile->xstart = xscr / 2;
      efile->left = 0;
      efile->top = -1;
      full_refresh = 1;
      show_efile( efile);
      return( 0);
      }

   if( efile && !_stricmp( comm, "fc"))
      {
      int rval = 0;
      EFILE *next = efile->next_file;

      assert( efile);
      if( !next || next == efile)
         {
         efile->message = "No comparison possible";
         rval = -1;
         }
      else
         {
         int i, x = 0;

         for( i = 0; !rval && i + efile->y < efile->n_lines
                           && i + next->y < next->n_lines; i++)
            {
            const LINE *p1 = efile->lines + efile->y + i;
            const LINE *p2 = next->lines + next->y + i;

            if( p1->size != p2->size ||
                     memcmp( p1->str, p2->str, p1->size * sizeof( LETTER)))
               {
               for( x = 0; x < p1->size && x < p2->size &&
                           p1->str[x] == p2->str[x]; x++)
                  ;
               if( i || x > efile->x)
                  rval = -1;
               }
            if( rval)
               {
               efile->x = next->x = x;
               efile->y += i;
               next->y += i;
               efile->message = "Mismatch found";
               }
            }
         }
      return( rval);
      }

   if( efile && !_memicmp( comm, "scr ", 4) && !comm[5])
      {
      int i = 0, n_splits = comm[4] - '0';
      EFILE *end_ptr = efile;

      do
         {
         efile->xsize = xscr;
         efile->ystart = 1 + (yscr - 2) *  i    / n_splits;
         efile->ysize =  1 + (yscr - 2) * (i+1) / n_splits - efile->ystart;
         efile->xstart = 0;
         efile->left = 0;
         efile->top = -1;
         full_refresh = 1;
         show_efile( efile);
         efile = efile->next_file;
         i = (i + 1) % n_splits;
         }
         while( efile != end_ptr);
      return( 0);
      }

   if( efile && !_stricmp( comm, "scr1"))
      {
      EFILE *end_ptr = efile;

      do
         {
         efile->xsize = xscr;
         efile->ysize = yscr - 2;
         efile->ystart = 1;
         efile->xstart = 0;
         efile->left = 0;
         efile->top = -1;
         efile = efile->next_file;
         }
         while( efile != end_ptr);
      return( 0);
      }

   if( !_stricmp( comm, "file") || !_stricmp( comm, "ffile"))
      {
      assert( efile);
      if( write_efile( efile))
         {
         efile->message = "FILE NOT SUCCESSFULLY WRITTEN";
         return( -1);
         }
      else
         comm = "qq";
      }

#ifndef __WATCOMC__
#ifndef __GNUC__
   if( efile && !_stricmp( comm, "chmod"))
      {
      if( _chmod( efile->filename, S_IWRITE | S_IREAD))
         efile->message = "PERMISSIONS NOT SUCCESSFULLY CHANGED";
      return( 0);
      }
#endif
#endif

   if( !_stricmp( comm, "save"))
      {
      assert( efile);
      if( !write_efile( efile))
         {
         efile->message = "File written";
         efile->dirty = 0;
         }
      else
         efile->message = "FILE NOT SUCCESSFULLY WRITTEN";
      return( 0);
      }

   if( !_stricmp( comm, "qq"))
      {
      assert( efile);
      if( efile->next_file == efile)      /* last file in ring */
         *curr_file = NULL;
      else
         *curr_file = efile->next_file;
      free_efile( efile);
      return( 0);
      }

   if( !_stricmp( comm, "top"))
      {
      assert( efile);
      efile->y = efile->x = efile->top = efile->left = 0;
      return( 0);
      }

   if( !memcmp( comm, "redef ", 6))
      {
      redefs_from = (char **)realloc( redefs_from, (n_redefs + 1) * sizeof( char *));
      redefs_to   = (char **)realloc( redefs_to,   (n_redefs + 1) * sizeof( char *));
      comm += 6;
      for( i = 0; comm[i] != ' '; i++)
         ;
      redefs_from[n_redefs] = (char *)malloc( i + 1);
      memcpy( redefs_from[n_redefs], comm, i);
      redefs_from[n_redefs][i] = '\0';
      comm += i + 1;
      redefs_to[n_redefs] = (char *)malloc( strlen( comm ) + 1);
      strcpy( redefs_to[n_redefs], comm);
      n_redefs++;
      return( 0);
      }

   if( !memcmp( comm, "centering ", 10))
      {
      centering = atoi( comm + 10);
      return( 0);
      }

   if( !memcmp( comm, "k ", 2) || !memcmp( comm, "ed ", 3)
                 || !memcmp( comm, "be ", 3)) /* add in a file */
      {
      EFILE *next_file;

      assert( efile);
      if( *comm == 'k')
         next_file = read_efile( comm + 2);
      else
         next_file = read_efile( comm + 3);
      if( next_file)
         {
         next_file->prev_file = efile;
         next_file->next_file = efile->next_file;
         next_file->prev_file->next_file = next_file->next_file->prev_file = next_file;
         *curr_file = next_file;
         }
      else
         efile->message = "File not found";
      return( 0);
      }

   if( efile && !memcmp( comm, "get ", 4))
      {
      if( insert_file( efile, comm + 4))
         efile->message = "File not found";
      return( 0);
      }

   if( efile && !_stricmp( comm, "k"))
      {
      *curr_file = efile->next_file;
      return( 0);
      }

   if( !strcmp( comm, "dir"))
      comm = "dir *.*";

#if defined( _WIN32)
   if( !memcmp( comm, "dir ", 4))    /* create a directory file */
      {
      EFILE *next_file;

      next_file = make_dir_file( comm + 4);
      if( next_file)
         {
         next_file->prev_file = efile;
         next_file->next_file = efile->next_file;
         next_file->prev_file->next_file = next_file->next_file->prev_file = next_file;
         *curr_file = next_file;
         }
      else
         efile->message = "No files of that spec found";
      return( 0);
      }
   if( !strcmp( comm, "di"))
      comm = "di *.*";

   if( !memcmp( comm, "di ", 3))    /* create a directory file */
      {
      EFILE *next_file;
      char buff[80];

      snprintf( buff, sizeof( buff), "dir %s > ickywax.ugh", comm + 3);
      system( buff);
      next_file = read_efile( "ickywax.ugh");
      if( next_file)
         {
         next_file->prev_file = efile;
         next_file->next_file = efile->next_file;
         next_file->prev_file->next_file = next_file->next_file->prev_file = next_file;
         *curr_file = next_file;
         }
      else
         efile->message = "No files of that spec found";
      return( 0);
      }
#endif

   if( efile && !memcmp( comm, "fn ", 3))    /* change in file name */
      {
      efile->filename = (char *)realloc( efile->filename, strlen( comm + 2));
      strcpy( efile->filename, comm + 3);
      return( 0);
      }

   if( *comm == '/' || *comm == '\\' || *comm == '~' || *comm == '!')       /* search time */
      {
      int direction = 1, blocking_mode = 0;

      if( *comm == '!')
         {
         blocking_mode = 1;
         comm++;
         }
      if( *comm == '~')
         {
         direction = -1;
         comm++;
         }
      assert( efile);
      find_occurrence_and_move( efile, comm + 1, *comm == '\\', direction,
                              blocking_mode);
      return( 0);
      }

   if( !memcmp( comm, "color ", 6))
      {
      int idx, foregnd, backgnd;

      if( sscanf( comm + 6, "%d,%d,%d", &idx, &foregnd, &backgnd) == 3)
         init_pair( idx, foregnd, backgnd);
      }

   if( !memcmp( comm, "ch ", 3) || !memcmp( comm, "sch ", 4)
            || !memcmp( comm, "bch ", 4) || !memcmp( comm, "bsch ", 5))
      {                                  /* global changes */
      int i, j, k;
      const char *str;
      const int in_block_mode = (*comm == 'b');
      const int ask_user = (*comm == 's' || comm[1] == 's');

      assert( efile);
      str = comm + 1;
      while( *str && str[-1] != ' ')
         str++;
      for( i = 1; str[i] && str[i] != *str && i < 70; i++);
      for( j = i+1; str[j] && str[j] != *str && j < 70; j++);
      if( str[i] == *str && str[j] == *str)
         {
         char search_str[80], rep_str[80];
         int x, y, n_changes = 0, c = 0;

         assert( efile);
         memcpy( search_str, str + 1, i - 1);
         search_str[i - 1] = '\0';
         memcpy( rep_str, str + i + 1, j - i - 1);
         rep_str[j - i - 1] = '\0';
         j -= i + 1;          /* j = strlen( rep_str) */
         i -= 1;              /* i = strlen( search_str) */
         x = efile->x;
         y = efile->y;
         while( !find_occurrence( efile, search_str, &x, &y, 1, 1,
                                             in_block_mode) && c != 27)
            {
            int change_it = 1;

            if( change_it && ask_user)
               {           /* selective change;  ask user before making */
               efile->x = x;
               efile->y = y;
               efile->command_loc = -1;
               efile->message = "F5 to make change, F6 to skip, ESC to quit";
               show_efile( efile);
               c = 0;
               while( c != 27 && c != KEY_F(5) && c != KEY_F(6))
                  c = extended_getch( );
               if( c != KEY_F(5))
                  change_it = 0;
               }
            if( change_it)
               {
               add_characters( efile, y, x, j - i);
               for( k = 0; k < j; k++)
                  efile->lines[y].str[x + k] = (LETTER)rep_str[k];
               efile->dirty = 1;
               x += j;
               n_changes++;
               }
            }
         if( !n_changes)
            efile->message = "Search string not found";
         else
            {
            static char msg[40];

            efile->message = msg;
            snprintf( msg, sizeof( msg), "%d occurrences changed", n_changes);
            }
         }
      else
         efile->message = "Didn't understand change command";
      return( 0);
      }

   if( !memcmp( comm, "mode=", 5))
      {
      assert( efile);
      if( comm[5] == 'd')
         {
         efile->dos_mode = 1;
         efile->message = "Carriage returns enabled";
         }
      else if( comm[5] == 'u')
         {
         efile->dos_mode = 0;
         efile->message = "Carriage returns disabled";
         }
      else
         efile->message = "Invalid mode";
      return( 0);
      }

   if( !memcmp( comm, "default_mode=", 13))
      {
      if( comm[13] == 'd')
         {
         default_dos_mode = 1;
         if( efile)
            efile->message = "Default is carriage returns enabled";
         }
      else if( comm[13] == 'u')
         {
         default_dos_mode = 0;
         if( efile)
            efile->message = "Default is carriage returns disabled";
         }
      else
         if( efile)
            efile->message = "Invalid default mode";
      return( 0);
      }

#ifdef MS_DOS
   if( !_stricmp( comm, "dos"))
      {
      endwin( );
      system( "command.com");
      refresh( );
      full_refresh = 1;
      return( 0);
      }

   if( !memcmp( comm, "dos ", 4))
      {
      endwin( );
      system( comm + 4);
      refresh( );
      full_refresh = 1;
      return( 0);
      }
#endif

   if( *comm == ':')    /* absolute line reference */
      {
      assert( efile);
      efile->y = atoi( comm + 1) - 1;
      efile->command_loc = -1;
      return( 0);
      }

   if( (*comm >= '0' && *comm <= '9') || *comm == '-')
                                  /* relative line reference */
      {
      assert( efile);
      efile->y += atoi( comm);
      efile->command_loc = -1;
      return( 0);
      }

   if( !strcmp( comm, "htxt"))
      {
      int curr_state = 0, y;

      assert( efile);
      for( y = efile->y; y < efile->n_lines; y++)
         if( y && efile->lines[y].str)
            curr_state = html_to_txt( efile->lines + y, curr_state);
      return( 0);
      }

   if( !memcmp( comm, "attr", 4) && comm[4] - '0' == display_mode &&
                                        comm[5] == ' ')
      {
      int i = 0;

      comm += 6;

      while( i < 20 && sscanf( comm, "%d", color + i))
         {
         while( *comm != ' ' && *comm)
            comm++;
         while( *comm == ' ')
            comm++;
         i++;
         }
      return( 0);
      }

   if( !memcmp( comm, "tabs ", 5))
      {
      int i = 0;

      comm += 5;

      while( i < 30 && sscanf( comm, "%d", tabs + i))
         {
         while( *comm != ' ' && *comm)
            comm++;
         while( *comm == ' ')
            comm++;
         i++;
         }
      return( 0);
      }

   if( !memcmp( comm, "tabx ", 5))
      {
      tab_xlate = atoi( comm + 5);
      return( 0);
      }

   if( !memcmp( comm, "nsort ", 6))
      {
      extern int numeric_sort;

      comm++;
      numeric_sort = 1;
      }

   if( !memcmp( comm, "csort ", 6))
      {
      extern int numeric_sort;

      comm++;                 /* case-sensitive sort */
      numeric_sort = 3;
      }

   if( !memcmp( comm, "asort ", 6))
      {
      extern int numeric_sort;
                 /* sort by absolute value */
      comm++;
      numeric_sort = 2;
      }

   if( !strcmp( comm, "sort"))
      comm = "sort *";

   if( !memcmp( comm, "sort ", 5))
      {
      int n_lines, order, y;

      assert( efile);
      y = efile->y;
      if( comm[5] == '*')        /* sort em all */
         n_lines = efile->n_lines;
      else if( comm[5] == 'm')      /* sort _marked_ lines */
         {
         extern int block_y1, block_y2;
         extern EFILE *block_file;

         if( block_file == efile && block_y1 != block_y2)
            {
            y = block_y1;
            n_lines = block_y2 - block_y1 + 1;
            }
         else
            {
            efile->message = "No lines marked to sort!";
            return( -1);
            }
         }
      else
         n_lines = atoi( comm + 5);
      if( comm[strlen( comm) - 1] == '-')
         order = -1;
      else
         order = 1;
      sort_file( efile, efile->x, y, n_lines, order);
      return( 0);
      }

   if( efile)
      efile->message = "Command not recognized";
   return( -1);
}

int read_profile_file( EFILE **curr_file, FILE *ifile)
{
   char str[180];

   if( !ifile)
      return( -1);
   while( fgets( str, 180, ifile))
      {
      int len = strlen( str);

      while( len && str[len - 1] < ' ')
         len--;
      str[len] = '\0';
      handle_command( curr_file, str);
      }
   return( 0);
}
