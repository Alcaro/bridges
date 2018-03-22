PROGRAM = bridges
ARGUI = 1
AROPENGL = 1
ARTHREAD = 0
ARWUTF = 0
ARSOCKET = 0
#valid values: openssl (default), gnutls, tlse, bearssl, no
ARSOCKET_SSL = openssl
#valid values: schannel (default), bearssl, no (others may work, not tested)
ARSOCKET_SSL_WINDOWS = schannel
ARSANDBOX = 0

SOURCES += resources.cpp
include arlib/Makefile

#OBJMANGLE usage is kinda ugly...
$(call OBJMANGLE,DEFAULT,game.cpp): resources.cpp
resources.cpp: rescompile.py resources/ resources/*
	$(ECHOQ) rescompile.py
	$(Q)python3 rescompile.py || python rescompile.py || echo 'python not installed...? Assuming resources.cpp is up to date...'
