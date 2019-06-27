LIBS += -L./libs

INCLUDE_PATH += -I"./flyric_parser"
LIBS += -lfrparser

INCLUDE_PATH += -I"/usr/local/include/freetype2"
LIBS += -lfreetype


LIBS += -lGL -lglfw
LIBS += -lm

main_linux.out:main_linux.o flyric_rendergl.o
	cc -o $@ $+ $(LIBS)
main_linux.o:main_linux.c glenv.h flyric_rendergl.h
	cc -c $(INCLUDE_PATH) main_linux.c
flyric_rendergl.o:flyric_rendergl.c glenv.h flyric_rendergl.h
	cc -c $(INCLUDE_PATH) flyric_rendergl.c
clean:
	rm -rf libs build *.o main_linux.out
