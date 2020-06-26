#!/usr/bin/env bash
cmake -Bbuild -S. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=make -DCMAKE_C_COMPILER=gcc -DBUILD_TESTING=ON
cmake --build build
