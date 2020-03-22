cmake_minimum_required(VERSION 3.10)

# set the project name
project(c-calipto)

# add the executable
add_executable(c-calipto repl.c)

# "run": "cc -I. -o repl repl.c sexpr.c scanner.c reader.c -std=c18"