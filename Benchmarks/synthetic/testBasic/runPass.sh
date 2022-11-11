#! /bin/bash
$ACHLYSLLVM/opt -enable-new-pm=0 -load $ACHLYSLLVM/../lib/Achlys.so -adce -always-inline -argpromotion -deadargelim  -inline -reg2mem -mergereturn -dse -taintanalysis -S intraproc.ll > passOutput.ll
