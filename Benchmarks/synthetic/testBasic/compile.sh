#!/bin/bash

$ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
$ACHLYSLLVM/clang++ -emit-llvm -S taintSourceTest.cpp -o taintSourceTest.ll
$ACHLYSLLVM/clang++ -emit-llvm -S test_inter_procedural_backtracking.cpp -o test_inter_procedural_backtracking.ll
