#!/usr/bin/env bash
cmake -Bbuild -S. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=make -DCMAKE_C_COMPILER=gcc -DTARGET_GROUP=test
cmake --build build
