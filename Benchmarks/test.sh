#! /bin/bash

if [ $# -lt 1 ]; then
    echo "$0: Usage: ./test.sh <testcase_name:string> [<recompile_test_case:bool>]"
    exit 1
fi

##############################################################3
VERBOSE=4 #Flag for logging verbose pass details
CFLAGS="-O0 -Xclang -disable-O0-optnone"
OPTFLAGS="-O0 -licm -argpromotion -deadargelim -adce -always-inline -inline -mem2reg -mergereturn -dse"

FILENAME="${1%.*}"
echo "Analyzing $FILENAME"
if [[ "$2" == "true" ]]; then
    echo "Recompiling test case $1"
    $ACHLYSLLVM/clang++ -emit-llvm -S $1 ${CFLAGS} -o $FILENAME.ll
fi

if [ -f "$FILENAME.ll" ]; then
    $ACHLYSLLVM/opt -enable-new-pm=0 ${OPTFLAGS} -S $FILENAME.ll -o $FILENAME.ll
    $ACHLYSLLVM/opt -enable-new-pm=0 -load $ACHLYSLLVM/../lib/Achlys.so \
    -basic-aa --cfl-anders-aa -taintanalysis -achlys-verbose=$VERBOSE -S $FILENAME.ll > dummy.txt
else
    echo "Check $1 for compilation errors!"
fi