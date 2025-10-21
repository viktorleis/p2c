#!/bin/bash

sf=${1:-"1"}

rm -rf input && mkdir -p input
( # generate csv data
  cd dbgen || exit 1
  test -f dbgen || ( git submodule update --init .; make target=release )
  ./dbgen -C"$sf" -s"$sf"
  chmod uga+rw-x *.tbl
  mv -f ./*.tbl* ../input
) || exit $?
# convert csv data to binary
make
./all.out input
