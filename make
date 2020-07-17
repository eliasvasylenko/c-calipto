#!/usr/bin/env bash
cmake -Bbuild \
      -S. \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_MAKE_PROGRAM=make \
      -DCMAKE_C_COMPILER=gcc \
      -DBUILD_TESTING=ON \
      -DCMAKE_C_FLAGS_DEBUG="-g -O0"
cmake --build build
