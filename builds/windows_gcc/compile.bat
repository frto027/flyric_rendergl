gcc -c -Llibs -Iinclude -I../../flyric_parser/include -I../../flyric_parser/src/ -I../../src/ -Iplatform -lglfw3 -lopengl32 -lglew32 -lglu32 -lgdi32 -lfreetype ../../flyric_parser/src/*.c ../../src/*.c
mkdir bin
move *.o bin/
cd bin
gcc *.o -L../libs -lopengl32 -lfreetype -lglfw3 -lglew32 -lgdi32
cd ..