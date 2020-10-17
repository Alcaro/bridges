PROGRAM = bridges

#GL and game are only used for the UI; if you replace it, you can disable it
ARGAME = 1
AROPENGL = 1

#threads can be disabled if you don't want them
#this makes level generator delays annoyingly large, but is otherwise safe
ARTHREAD = 1

include arlib/Makefile
