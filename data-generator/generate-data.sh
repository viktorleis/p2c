#!/bin/bash

sf=${1:-"1"}

mkdir input
cd tpch-dbgen || exit
test -f dbgen || ( git submodule update --init .; make )
./dbgen -s"$sf"
chmod +rw nation.tbl
chmod -x nation.tbl
mv -f ./*.tbl ../input
cd ..
make
./all.out input/
