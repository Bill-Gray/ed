#define EFILE struct efile
#define LINE struct line

#define LETTER unsigned

EFILE
   {
   EFILE *next_file, *prev_file;
   char *filename, command[80], *message;
   LINE *lines;
   int lines_alloced, n_lines, dirty;
   int x, y, top, left, command_loc;
   int xsize, ysize, xstart, ystart, dos_mode;
   };

LINE
   {
   LETTER *str;
   int size, alloced;
   };

EFILE *read_efile( const char *name);
int write_efile( EFILE *efile);
void free_efile( EFILE *efile);
int change_line( EFILE *efile, LETTER *newstr, int line_no);
int remove_lines( EFILE *efile, int start, int n_lines);
int add_lines( EFILE *efile, int start, int n_lines);
int reset_line_size( EFILE *efile, int line_no, int new_size);
int add_characters( EFILE *efile, int line_no, int start, int n_chars);
int push_changed_line( EFILE *efile, int line_no);
int pop_changed_line( EFILE *efile, int line_no);
void show_efile( EFILE *efile);
int correct_efile( EFILE *efile);
int extended_getch( void);
int push_string( LETTER *string, int len);
LETTER *pop_string( void);
void reset_line( LINE *line);
int find_matching_whatsis( EFILE *efile);
int line_length( LETTER *str);
int line_copy( LETTER *dest, LETTER *src);
