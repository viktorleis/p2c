#!/bin/bash

sf=${1:-"1"}

mkdir -p input
( # generate csv data
  cd tpch-dbgen || exit 1
  test -f dbgen || ( git submodule update --init .; make )
  ./dbgen -fs"$sf"
  chmod uga+rw-x *.tbl
  mv -f ./*.tbl ../input
) || exit $?
# convert csv data to binary
make
./all.out input
