PROGRAM = bridges

#GL and game are only used for the UI; if you replace it, you can disable it
ARGAME = 1
AROPENGL = 1

#threads are also optional; the level generator delay is huge without them, but it's otherwise safe
ARTHREAD = 1

include arlib/Makefile
