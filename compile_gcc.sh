#! /bin/sh
gcc demo.c -g -o demo  -I /usr/local/include -L /usr/local/lib -lavformat -lavcodec -lavutil -lswscale -lswresample
