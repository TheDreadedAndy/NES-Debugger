#!/bin/bash
g++ -O3 -ffast-math -c fast_math.cc -o fast_math.o
g++ -O3 -c finv_math.cc -o finv_math.o
g++ -O3 -c normal_math.cc -o normal_math.o
g++ -O3 -c tay_math.cc -o tay_math.o
g++ -c main.cc -o main.o
g++ fast_math.o finv_math.o normal_math.o tay_math.o main.o -o test
