#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "ed.h"
#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

#define LINE_STEP 50

extern int default_dos_mode;     /* defined in COMMHAND.C */
extern int xscr, yscr;     /* size of screen; default xscr=80, yscr=25 */
extern int min_line_realloc;
int max_line_length = 1024;
int skipped_bytes = 0;
int use_mac = 0, max_lines = 800000;

extern EFILE *block_file;      /* there's only one block,  so info on it is global */
extern int block_x1, block_y1, block_x2, block_y2, is_block;

static char *ext_fgets( char *buff, int max_len, FILE *ifile)
{
   int ichar;

   if( !use_mac)
      return( fgets( buff, max_len, ifile));
   ichar = getc( ifile);
   if( ichar == EOF)
      return( NULL);
   for( ; ichar != EOF && ichar != 13 && max_len; max_len--)
      {
      *buff++ = (char)ichar;
      ichar = getc( ifile);
      }
   *buff++ = '\0';
   return( buff);
}

int realloc_lines( EFILE *efile, int n)
{
   LINE *new_lines;

   n += 5;
   if( efile->lines_alloced >= n)   /* already got enough */
      return( 0);
   if( n < min_line_realloc)
      n = min_line_realloc;
   n += LINE_STEP + n / 4;
   if( n > max_lines)
      n = max_lines;
#ifdef CHECK_FOR_REALLOC_ERRORS
   new_lines = (LINE *)realloc( efile->lines, n * sizeof( LINE));
#else
   new_lines = (LINE *)calloc( n, sizeof( LINE));
   if( new_lines && efile->lines)
      memcpy( new_lines, efile->lines, efile->lines_alloced * sizeof( LINE));
   if( efile->lines)
      free( efile->lines);
#endif
   if( new_lines)
      {
      int i;

      efile->lines = new_lines;
      for( i = efile->lines_alloced; i < n; i++)
         {
         efile->lines[i].alloced = efile->lines[i].size = 0;
         efile->lines[i].str = NULL;
         }
      efile->lines_alloced = n;
      return( 0);
      }
   else
      return( -1);
}

#if defined( _WIN32)
   #define DIRMACRO "c:\\utils\\dirmacro"
   #define PATH_SEPARATOR '\\'
#else
   #define DIRMACRO "/home/phred/ed/dirmacro"
   #define PATH_SEPARATOR '/'
#endif

static void macroize( char *oname, const char *iname)
{
   int found_it = 0;

   if( strchr( iname, PATH_SEPARATOR))
      {
      FILE *ifile;
      char buff[180];
      int i;

#if defined( _WIN32)
      strcpy( buff, "c:\\utils\\dirmacro");
#else
      strcpy( buff, getenv( "HOME"));
      strcat( buff, "/ed/dirmacro");
#endif
      ifile = fopen( buff, "rb");
      assert( ifile);
      if( ifile)
         {
         while( !found_it && fgets( buff, sizeof( buff), ifile))
            {
            for( i = 0; buff[i] >= ' '; i++)
               ;
            buff[i] = '\0';            /* clear trailing CR/LF */
            for( i = 0; buff[i] > ' '; i++)
               ;
            if( !memcmp( buff, iname, i) && iname[i] == PATH_SEPARATOR)
               {
               strcpy( oname, buff + 4);

               strcat( oname, iname + i);
               found_it = 1;
               }
            }
         fclose( ifile);
         }
      }

   if( !found_it)
      strcpy( oname, iname);
   if( *oname == '~')
      {
      char buff[180];

      strcpy( buff, oname + 1);
      strcpy( oname, getenv( "HOME"));
      strcat( oname, buff);
      }
}

EFILE *read_efile( const char *name)
{
   FILE *ifile;
   EFILE *rval;
   char *buff;
   int i;

   rval = (EFILE *)calloc( 1, sizeof( EFILE));
   if( !rval)
      return( NULL);
   rval->next_file = rval->prev_file = rval;
   rval->lines_alloced = 0;
   rval->lines = (LINE *)malloc( 30);
   realloc_lines( rval, 30);
   buff = (char *)malloc( max_line_length + 2);
   macroize( buff, name);
   rval->filename = (char *)malloc( strlen( buff) + 1);
   rval->dos_mode = 0;
   if( !rval->filename || !rval->lines)
      {
      free_efile( rval);
      free( buff);
      return( NULL);
      }
   strcpy( rval->filename, buff);
   ifile = fopen( rval->filename, "rb");
   if( !ifile)
      rval->message = "New file...";
   else
      fseek( ifile, skipped_bytes, SEEK_SET);
   skipped_bytes = 0L;
   for( i = 0; ifile && ext_fgets( buff, max_line_length, ifile) &&
                                  i < max_lines; i++)
      {
      int len, j, k;
      LINE *line;
      extern int tab_xlate;

      if( i == rval->lines_alloced)
         if( realloc_lines( rval, i))
            {
            free_efile( rval);
            free( buff);
            fclose( ifile);
            return( NULL);
            }
      line = rval->lines + i;
      len = strlen( buff);
      while( len > 0 && (unsigned char)buff[len - 1] <= ' ') len--;
      for( j = 0; j < len; j++)
         if( buff[j] == 9 && tab_xlate)          /* tab translation */
            {
            memmove( buff + j + tab_xlate - 1, buff + j, len - j);
            memset( buff + j, ' ', tab_xlate);
            len += tab_xlate - 1;
            }
         else if( buff[j] == 13)
            buff[j] = ' ';
      line->size = line->alloced = len;
      if( len > 0)
         {
         unsigned char *tptr = (unsigned char *)buff;

         if( buff[len] == 13)
            rval->dos_mode = 1;
         line->str = (LETTER *)malloc( (len + 5) * sizeof( LETTER));
         if( !line->str)
            {
            free_efile( rval);
            free( buff);
            fclose( ifile);
            return( NULL);
            }
         k = 0;
         for( j = 0; j < len; j++, tptr++)
            if( !(*tptr & 0x80))                   /* handle UTF8 encoding */
               line->str[k++] = (LETTER)buff[j];
            else if( (*tptr & 0xe0) == 0xc0)
               {                  /* U+0080 to U+07ff */
               line->str[k++] = ((LETTER)( *tptr & 0x1f) << 6)
                      | (LETTER)( tptr[1] & 0x3f);
               tptr++;
               j++;
               }
            else if( (*tptr & 0xf0) == 0xe0)
               {                  /* U+0800 to U+ffff */
               line->str[k] = ((LETTER)( *tptr & 0x0f) << 12)
                      | (LETTER)( tptr[1] & 0x3f) << 6
                      | (LETTER)( tptr[2] & 0x3f);
               if( line->str[k] != (LETTER)0xfeff)
                  k++;        /* skip the byte-order mark */
               tptr++;
               tptr++;
               j++;
               j++;
               }
            else if( (*tptr & 0xf8) == 0xf0)
               {                  /* U+10000 to U+10ffff: SMP */
               line->str[k++] = ((LETTER)( *tptr & 0x07) << 18)
                      | (LETTER)( tptr[1] & 0x3f) << 12
                      | (LETTER)( tptr[2] & 0x3f) << 6
                      | (LETTER)( tptr[3] & 0x3f);
               tptr++;
               tptr++;
               tptr++;
               j++;
               j++;
               j++;
               }
            else
               {
               line->str[k++] = '?';    /* mark as undeciphered */
               tptr++;
               j++;
               }
         line->size = k;
         }
      else
         line->str = NULL;
      }
   rval->n_lines = i;
   rval->x = rval->y = rval->dirty = 0;
   if( !i)
      rval->y = -1;
   rval->xsize = xscr;
   rval->ysize = yscr - 2;
   rval->ystart = 1;
   rval->xstart = 0;
   rval->left = 0;
   rval->top = -1;
   rval->next_file = rval->prev_file = rval;
   rval->command_loc = 0;
   rval->command[0] = '\0';
   if( ifile)
      fclose( ifile);
   else
      rval->dos_mode = (char)default_dos_mode;
   free( buff);
   return( rval);
}

void free_efile( EFILE *efile)
{
   int i;

   if( !efile)
      return;
   efile->next_file->prev_file = efile->prev_file;
   efile->prev_file->next_file = efile->next_file;
   if( efile->lines)
      {
      for( i = 0; i < efile->n_lines; i++)
         if( efile->lines[i].str)
            free( efile->lines[i].str);
      free( efile->lines);
      }
   if( efile->filename)
      free( efile->filename);
   free( efile);
}

static unsigned long crc_32( unsigned char *buff, unsigned nbytes)
{
   unsigned long rval;

   for( rval = 0xffffffff; nbytes; nbytes--)
      {
      unsigned bit;
      unsigned n = ((unsigned)(rval >> 24) & 0xff) ^ *buff++;
      unsigned long tblval = (unsigned long)n << 24;
      const unsigned long magic_polynomial = 0x4c11db7;

      for( bit = 8; bit; bit--)
         if( tblval & 0x80000000)
            tblval = (tblval << 1) ^ magic_polynomial;
         else
            tblval <<= 1;
      rval = (rval << 8) ^ tblval;
      }
   return( ~rval);
}

int write_efile( EFILE *efile)
{
   FILE *ofile;
   int i, len, j, out_loc, curr_len = 800, rval = 0;
   LINE *line;
   char *obuff;
   long crc = 0L, osize = 0L;

   ofile = fopen( efile->filename, "wb");
   if( !ofile)
      return( -1);
   obuff = (char *)malloc( curr_len + 10);
   for( i = 0; !rval && i < efile->n_lines; i++)
      {
      LETTER *s = NULL;

      line = efile->lines + i;
      len = line->size;
      if( line->str)
         s = line->str;
      else
         len = 0;
      if( len + 50 > curr_len)
         {
         free( obuff);
         curr_len = len + 50;
         obuff = (char *)malloc( curr_len + 10);
         }

      if( sizeof( LETTER) == 1)        /* no fancy stuff w/attributes */
         {
         memcpy( obuff, s, len);
         out_loc = len;
         }
      else
         for( out_loc = j = 0; j < len; j++)
            {
            if( s[j] < 0x80)
               obuff[out_loc++] = (char)s[j];
            else if( s[j] < 0x0800)
               {
               obuff[out_loc++] = (char)( 0xc0 | (s[j] >> 6));
               obuff[out_loc++] = (char)( 0x80 | (s[j] & 0x3f));
               }
            else if( s[j] < 0x10000)
               {
               obuff[out_loc++] = (char)( 0xe0 | (s[j] >> 12));
               obuff[out_loc++] = (char)( 0x80 | ((s[j] >> 6) & 0x3f));
               obuff[out_loc++] = (char)( 0x80 | (s[j] & 0x3f));
               }
            }
      if( efile->dos_mode)
         {
         obuff[out_loc++] = 13;
         obuff[out_loc++] = 10;
         }
      else
         obuff[out_loc++] = 10;

      if( !fwrite( obuff, out_loc, 1, ofile))
         rval = -1;
      osize += (long)out_loc;
      crc ^= crc_32( (unsigned char *)obuff, (unsigned)out_loc);
      }
   fclose( ofile);

#ifdef __GNUC__
   ofile = fopen( "/mnt/win_c/ed/be.log", "a");
#else
   ofile = fopen( "c:\\ed\\be.log", "ab");
#endif
   if( ofile)
      {
      const time_t t0 = time( NULL);

      strcpy( obuff, ctime( &t0));
#ifdef __GNUC__
      sprintf( obuff + 24, "%10ld %8lx %s\n", osize, crc, efile->filename);
#else
      sprintf( obuff + 24, "%10ld %8lx ", osize, crc);
      _fullpath( obuff + 44, efile->filename, _MAX_PATH);
      strcat( obuff, "\n");
#endif
      fprintf( ofile, "%s", obuff);
      fclose( ofile);
      }
   free( obuff);
   return( rval);
}

int add_lines( EFILE *efile, int start, int n_lines)
{
   int i;

   if( n_lines < 0)
      return( remove_lines( efile, start, -n_lines));
   if( start > efile->n_lines)
      start = efile->n_lines;
   if( efile->n_lines + n_lines > efile->lines_alloced)
      if( realloc_lines( efile, efile->n_lines + n_lines))
         return( -1);
   memmove( efile->lines + start + n_lines, efile->lines + start,
                                (efile->n_lines - start) * sizeof( LINE));
   for( i = start; i < start + n_lines; i++)
      {
      efile->lines[i].size = efile->lines[i].alloced = 0;
      efile->lines[i].str = NULL;
      }
   efile->n_lines += n_lines;
   efile->dirty = 1;
   if( efile == block_file)
      {
      if( block_y1 >= start)
         block_y1 += n_lines;
      if( block_y2 >= start)
         block_y2 += n_lines;
      }
   return( 0);
}

int remove_lines( EFILE *efile, int start, int n_lines)
{
   int i;
   LINE *line;

   if( n_lines < 0)
      return( add_lines( efile, start, -n_lines));
   if( start >= efile->n_lines)
      return( 0);
   if( start + n_lines > efile->n_lines)
      n_lines = efile->n_lines - start;
   if( start < 0)
      {
      n_lines += start;
      start = 0;
      }
   if( n_lines <= 0)
      return( 0);
   line = efile->lines + start;
   for( i = 0; i < n_lines; i++)
      if( line[i].str)
         {
         push_string( line[i].str, line[i].size);
         free( line[i].str);
         }
   memmove( efile->lines + start, efile->lines + start + n_lines,
                          (efile->n_lines - start - n_lines) * sizeof( LINE));
   efile->n_lines -= n_lines;
   efile->dirty = 1;
   if( efile == block_file)
      {
      if( block_y1 >= start)
         {
         block_y1 -= n_lines;
         if( block_y1 < start)
            block_y1 = start;
         }
      if( block_y2 >= start)
         {
         block_y2 -= n_lines;
         if( block_y2 < block_y1)
            block_file = NULL;
         if( block_y2 < start)
            block_y2 = start - 1;
         }
      }
   return( 0);
}

int line_length( LETTER *str)
{
   int rval = 0;

   while( *str++)
      rval++;
   return( rval);
}

int line_copy( LETTER *dest, LETTER *src)
{
   while( src && *src)
      *dest++ = *src++;
   *dest = 0;
   return( 0);
}

int change_line( EFILE *efile, LETTER *newstr, int line_no)
{
   LINE *line;
   int new_len;

   if( line_no > efile->lines_alloced)
      if( realloc_lines( efile, line_no))
         {
         free_efile( efile);
         return( -1);
         }

   if( line_no >= efile->n_lines)
      efile->n_lines = line_no + 1;

   if( !newstr)
      new_len = 0;
   else
      new_len = line_length( newstr);
   reset_line_size( efile, line_no, new_len + 5);
   line = efile->lines + line_no;
   line_copy( line->str, newstr);
   line->size = new_len;
   efile->dirty = 1;
   return( 0);
}

int reset_line_size( EFILE *efile, int line_no, int new_size)
{
   LINE *line;
   LETTER *new_letters;
   int i;

   realloc_lines( efile, line_no);
   line = efile->lines + line_no;
   push_string( line->str, line->size);
   if( line->alloced < new_size)
      {
      if( line->str)
         new_letters = (LETTER *)realloc( line->str, (new_size + 10) * sizeof( LETTER));
      else
         new_letters = (LETTER *)calloc( new_size + 10, sizeof( LETTER));
      if( new_letters)
         line->str = new_letters;
      else
         return( -1);
      line->alloced = new_size + 5;
      }
   for( i = line->size; i < new_size; i++)
      line->str[i] = ' ';
   return( 0);
}

int add_characters( EFILE *efile, int line_no, int start, int n_chars)
{
   int rval = 0, i;
   LINE *line;

   realloc_lines( efile, line_no);
   line = efile->lines + line_no;
   push_string( line->str, line->size);
   if( n_chars <= 0)          /* delete or no change */
      {
      if( start >= line->size)
         return( 0);
      if( start - n_chars > line->size)
         n_chars = start - line->size;
      memmove( line->str + start, line->str + start - n_chars,
                  (line->size - (start - n_chars)) * sizeof( LETTER));
      line->size += n_chars;
      efile->dirty = 1;
      return( 0);
      }
                  /* now we know we're adding chars */
   efile->dirty = 1;
   if( start >= line->size)
      return( reset_line_size( efile, line_no, start + n_chars));
   rval = reset_line_size( efile, line_no, line->size + n_chars);
   memmove( line->str + start + n_chars, line->str + start,
                               (line->size - start) * sizeof( LETTER));
   for( i = 0; i < n_chars; i++)
      line->str[ i + start] = ' ';
   line->size += n_chars;
   return( rval);
}
