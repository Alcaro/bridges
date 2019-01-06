PROGRAM = bridges
#GUI and GL are only used for the UI; if you replace it, you can disable these
ARGUI = 1
AROPENGL = 1
#threads can be disabled if you don't want them
#this makes level generator delays annoyingly large, but is otherwise safe
ARTHREAD = 1
ARWUTF = 0
ARSOCKET = 0
ARSANDBOX = 0

SOURCES += resources.cpp
include arlib/Makefile

$(call OBJMANGLE,DEFAULT,game.cpp): resources.cpp
resources.cpp: rescompile.py resources/ resources/*
	$(ECHOQ) rescompile.py
	$(Q)python3 rescompile.py || python rescompile.py
