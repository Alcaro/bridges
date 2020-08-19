PROGRAM = bridges
ARGUI = 0
#GL and game are only used for the UI; if you replace it, you can disable it
ARGAME = 1
AROPENGL = 1
#threads can be disabled if you don't want them
#this makes level generator delays annoyingly large, but is otherwise safe
ARTHREAD = 1
ARWUTF = 0
ARSOCKET = 0
ARSANDBOX = 0

include arlib/Makefile
