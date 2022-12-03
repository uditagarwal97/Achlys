#! /bin/bash

if [ $# -lt 1 ]; then
    echo "$0: Usage: ./test.sh <testcase_name_without_extension:string> [<recompile_test_case:bool>]"
    exit 1
fi

##############################################################3
VERBOSE=4 #Flag for logging verbose pass details
CFLAGS="-O0 -Xclang -disable-O0-optnone"
# OPTFLAGS="-O0 -licm -argpromotion -deadargelim -adce -always-inline -inline -mem2reg -mergereturn -dse"

if [[ "$2" == "true" ]]; then
    echo "Recompiling test case"
    $ACHLYSLLVM/clang++ -emit-llvm -S $1.cpp ${CFLAGS} -o $1.ll
fi

$ACHLYSLLVM/opt -enable-new-pm=0 -load $ACHLYSLLVM/../lib/Achlys.so -adce \
-always-inline -argpromotion -deadargelim  -inline -reg2mem -mergereturn \
-dse -basic-aa -taintanalysis -achlys-verbose=$VERBOSE -S $1.ll
