VPATH = ./vpx
PROGRAM = server.exe client.exe
LDFLAGS = -Wall -g -O0 -fno-omit-frame-pointer -gdwarf-2 \
-lwsock32 -lgdi32 -limm32 -lolepro32 -lole32 \
-luuid -lwinmm -ldsound -lpthread -lz -lvJoyInterface \
-lComdlg32 -lGdiplus -lpsapi -lXinput -L.
#CLIENTOBJS = client.o vpxConverter.o
CLIENTOBJS = client.o
#SERVEROBJS = server.o vpxConverter.o
SERVEROBJS = server.o
#COMMONOBJS = tools_common.o y4minput.o libvpx.a libyuv.a liblz4_static.lib libturbojpeg.a libopus.a
COMMONOBJS = liblz4_static.lib libturbojpeg.a libopus.a
CC = g++
#CFLAGS = -Wall -O2 -I.
CFLAGS = -Wall -g -O0 -I.
#CFLAGS = -Wall -g -O0 -fno-omit-frame-pointer -gdwarf-2 -I.
.SUFFIXES: .c .o

.PHONY: all
all: depend $(PROGRAM)

server.exe: $(SERVEROBJS)
	$(CC)  -o $@ $^ $(COMMONOBJS) $(LDFLAGS)

client.exe: $(CLIENTOBJS)
	$(CC)  -o $@ $^ $(COMMONOBJS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	del $(PROGRAM) $(SERVEROBJS) $(CLIENTOBJS)

.PHONY: depend
