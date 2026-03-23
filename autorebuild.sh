#!/bin/sh

ENTR_CMD="./build s && ./out/r4web $@"

find . -type f \
	-name "*.c" -o \
	-name "*.h" -o \
	-name "Makefile" -o \
	-name "_*.html" \
	| entr -r bash -c "$ENTR_CMD"
