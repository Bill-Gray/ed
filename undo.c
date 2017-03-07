#include <string.h>
#include <stdlib.h>
#include "ed.h"
#ifdef DEBUG_MEM
#include "checkmem.h"
#endif

#define N_SAVED 50

static LETTER *prev[N_SAVED], *saved[N_SAVED];

int push_string( LETTER *string, int len)
{
   if( !len || !string || string == prev[0])
      return( 0);
   if( saved[N_SAVED - 1])
      free( saved[N_SAVED - 1]);
   memmove( prev + 1, prev, (N_SAVED - 1) * sizeof( LETTER *));
   memmove( saved + 1, saved, (N_SAVED - 1) * sizeof( LETTER *));
   prev[0] = string;
   saved[0] = (LETTER *)calloc( len + 5, sizeof( LETTER));
   memcpy( saved[0], string, len * sizeof( LETTER));
   saved[0][len] = '\0';
   return( 0);
}

LETTER *pop_string( void)
{
   LETTER *rval;          /* Note that it's the callers responsibility to */
                          /* free the returned string */
   rval = saved[0];
   memmove( saved, saved + 1, (N_SAVED - 1) * sizeof( LETTER *));
   memmove( prev, prev + 1, (N_SAVED - 1) * sizeof( LETTER *));
   saved[N_SAVED - 1] = prev[N_SAVED - 1] = NULL;
   return( rval);
}
