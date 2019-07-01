ROOT=..
BIN_DIR=$(ROOT)/bin


LIBS += -L$(ROOT)/libs

INCLUDE_PATH += -I$(ROOT)/flyric_parser/include
LIBS += -lfparser

INCLUDE_PATH += -I/usr/local/include/freetype2
LIBS += -lfreetype


LIBS += -lGL -lglfw
LIBS += -lm

main_linux.out:main_linux.c glenv.h flyric_rendergl.h flyric_rendergl.c
	cc -o $(BIN_DIR)/$@ main_linux.c flyric_rendergl.c $(LIBS) $(INCLUDE_PATH) 
main_linux_debug.out:main_linux.c glenv.h flyric_rendergl.h flyric_rendergl.c
	cc -o $(BIN_DIR)/$@ main_linux.c flyric_rendergl.c $(LIBS) $(INCLUDE_PATH) -g
