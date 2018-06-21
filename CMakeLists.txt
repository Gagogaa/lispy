# This is here just to statisfy cmake 
cmake_minimum_required(VERSION 3.11) 

# This specifies the project make and the language thats being used
project(lispy C) 

# These are the additinal arguments to the c compiler
add_definitions(-g -O3 -Wall -Wextra -Wshadow)

# This adds the executable with all the files needed to compile it
add_executable(lispy main.c mpc.c)

# The first argument is the name of the executable followed by the list of required libraries
target_link_libraries(lispy readline m)
