#!/bin/sh

f=0

while inotifywait --event modify --exclude '\.o$' --exclude 'mmdb$' --exclude 'mmdb_tests$' .; do
  make test

  r=$?

  if [ $r = 0 ]; then
    if [ $f -eq 1 ]; then
      notify-send -t 2500 "Tests are passing"
      f=0
    fi
  else
    notify-send -t 2500 "Tests failed"
    f=1
  fi
done
