display=0
; display=0 is 'normal' text mode
; display=1 uses ANSI text mode sequences
; display=2 is 640x480 (80 chars by 30 lines) SVGA mode
; display=3 is 800x600 (100 chars by 37 lines) SVGA mode
; display=4 is 1024x768 (128 chars by 48 lines) SVGA mode
;random=c:\ed\rand
cursor 8 2
tabx 3
redef e k d:err
# DOS: redef #499 key #266 #9 { #266 } #259 #266
redef #4155 key #266 #9 { #266 } #259 #266
# The above should redef alt-; to insert a pair of braces...
redef #272 key auth_xxxxxx_(28_jun_09) #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331 #331
# The above redefs Alt-Q for use in credit card orders
redef #280 key #316 28_Dec_98___Upgr_to_7.0__(check_encl) #316 ROUND_28
# The above redefs Alt-O to enter a "check encl"
redef #369 key Thank_you,_________Bill_Gray__________(Project_Pluto)
# The above redefs Alt-F10 to insert my 'sig'
redef #284 key http://www.projectpluto.com
# The above redefs Shift-F8 to show my Web site...
redef #309 key pluto@projectpluto.com
# The above remaps Alt-F9 to show my e-mail address

redef #282 key #13 (#335)
# The above remaps Alt-[ to enter,  insert a (, go to end of line, add )
# redef #498 key #316 int_main(_const_int_argc,_const_char_**argv)
redef #4157 key #316 int_main(_const_int_argc,_const_char_**argv)
# The above remaps Alt-] to insert a default 'main' def
# DOS: redef #485 key 3.1415926535897932384626433832795028841971693993751058209749445923
redef #4189 key 3.1415926535897932384626433832795028841971693993751058209749445923

default_mode=u    ;new files default to line-feed only endings
tabs 0 3 6 9 12 15 18 21 24 27 30 33 36 39 42 45 48 51 54 57 60 63 66 69 72 75 78 81 84 87 90 93 96 99 102 105
#tabs 0 4 8 12 16 20 24 28 32 36 40 44 48 52 56 60 64 68 72 76 80 84 88 92 96 100
#attr0 63 111 15 95 31 112 46 79  120 240 15 9 112 240 240 112
attr0 2 15 0 3 11 9 6 7 1 2 3 4 5 6 7 0
attr1 46 44 40 44 42 47 45 41
attr2 32 2 15 121 34 79 95 144 8 9 10 11 12
maxlen=4096
; Above line states that the maximum line length allowed is 2048 chars
;  Attr0 is for display mode 0,  attr1 for mode 1,  etc.
;  Attributes are defined in the following order:
;  0: top line attribute
;  1: top line attribute for error messages
;  2: 'normal' (unmarked) text
;  3: marked text
;  4: command line text color
;  5: cursor attribute in overwrite mode
;  6: cursor attribute in insert mode
;  7: top and end of file line attributes
; attr0 is for normal (direct access) display;   attr1 is for ANSI displays
; attr2 is for VGA displays
