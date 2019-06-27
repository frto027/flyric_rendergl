#!/bin/sh
./gen_library.sh
make
./main_linux.out /home/frto027/Project/testFreeType/Deng.ttf lrc.txt
