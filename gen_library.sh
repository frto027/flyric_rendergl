#!/bin/sh
echo 'building libfrparser.a ...'
if [ ! -d libs ]; then
    mkdir ./libs
fi
cd ./flyric_parser
make clean
make
mv *.a ../libs
cd ..
