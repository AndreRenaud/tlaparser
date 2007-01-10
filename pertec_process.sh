#!/bin/sh

for f in $* ; do
    echo "Processing ${f}"
    ./tlaparser  -o pertecid=4 -p -1 ${f} > ${f}.log
done

