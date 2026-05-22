cc = cl
rc = rc
link = link

CFLAGS = /nologo /O2 /DUNICODE /D_UNICODE /D_AMD64_
LIBS = user32.lib gdi32.lib strmiids.lib ole32.lib oleaut32.lib

all: amcap.exe

amcap.exe: amcap.obj amcap.res
	$(link) /NOLOGO /SUBSYSTEM:WINDOWS /OUT:amcap.exe amcap.obj amcap.res $(LIBS)

amcap.obj: amcap.cpp amcap.h resource.h
	$(cc) $(CFLAGS) /c amcap.cpp

amcap.res: amcap.rc resource.h
	$(rc) /r amcap.rc