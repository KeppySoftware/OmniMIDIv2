#!/bin/sh

echo "Compiling OmniMIDI winelib..."
rm OmniMIDI.dll 2> /dev/null
rm OmniMIDI.dll.so 2> /dev/null
winegcc -v -mwindows -mno-cygwin -shared *.c OmniMIDI.spec -o OmniMIDI.dll -ldl -luser32 -lkernel32
mv OmniMIDI.dll.so OmniMIDI.dll 2> /dev/null
echo "Done."
