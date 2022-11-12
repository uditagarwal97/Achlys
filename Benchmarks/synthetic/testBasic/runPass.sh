#! /bin/bash

VERBOSE=1 #Flag for logging verbose pass details

$ACHLYSLLVM/opt -enable-new-pm=0 -load $ACHLYSLLVM/../lib/Achlys.so -adce \
-always-inline -argpromotion -deadargelim  -inline -reg2mem -mergereturn \
-dse -taintanalysis -achlys-verbose=$VERBOSE -S intraproc.ll > passOutput.ll
