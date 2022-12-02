#!/bin/bash

CFLAGS="-O0 -Xclang -disable-O0-optnone"
OPTFLAGS="-O0 -licm -argpromotion -deadargelim -adce -always-inline -inline -mem2reg -mergereturn -dse"

declare -a testCases=("intraproc" "test_pointer_exhaustive" "taintSourceTest"
               "test_inter_procedural_backtracking" "test_inter_procedural_multifun"
               "arrayStructTestBaic" "nested_loop")

for str in "${testCases[@]}"
do
  $ACHLYSLLVM/clang++ -emit-llvm -S ${str}.cpp ${CFLAGS} -o ${str}.ll
  $ACHLYSLLVM/opt -enable-new-pm=0 ${OPTFLAGS} -S ${str}.ll -o ${str}.ll
done
