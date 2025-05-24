#!/bin/sh

winegcc -mwindows -mno-cygwin -shared *.c OmniMIDI.spec -o OmniMIDI.dll -ldl -luser32 -lkernel32