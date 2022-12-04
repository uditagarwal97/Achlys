#!/bin/bash

CFLAGS="-O0 -Xclang -disable-O0-optnone"
OPTFLAGS="-O0 -licm -argpromotion -deadargelim -adce -always-inline -inline -mem2reg -mergereturn -dse"

declare -a testCases=("stephen_dolan" "phillip_klenze" "peter_eastman"
                      "michael_dunphy" "matt_bierner" "linus_akesson_2"
                      "linus_akesson_1" "josh_lospinoso" "ghislain_lemaur")

for str in "${testCases[@]}"
do
  $ACHLYSLLVM/clang++ -emit-llvm -S ${str}.c ${CFLAGS} -o ${str}.ll
  FILE="${str}.ll"
  if [ -f "$FILE" ]; then
    $ACHLYSLLVM/opt -enable-new-pm=0 ${OPTFLAGS} -S ${str}.ll -o ${str}.ll
  else 
    echo "Skipping $FILE. Check for compilation errors!"
  fi
done
