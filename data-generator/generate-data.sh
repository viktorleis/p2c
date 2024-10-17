#!/bin/bash

mkdir input
cd tpch-dbgen || exit
./dbgen
chmod +rw nation.tbl
chmod -x nation.tbl
mv -f ./*.tbl ../input
cd ..
make
./all.out input/
