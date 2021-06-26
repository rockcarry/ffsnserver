#!/bin/sh

gcc -Wall ffsnserver.c -o ffsnserver -lws2_32
strip ffsnserver.exe

