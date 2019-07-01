#!/bin/sh
echo 'building libfparser.a ...'
if [ ! -d libs ]; then
    mkdir ./libs
fi
cd ./flyric_parser
make clean
make
mv ./obj/*.a ../libs
cd ..
