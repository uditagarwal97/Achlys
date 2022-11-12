#!/bin/bash

$ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
$ACHLYSLLVM/clang++ -emit-llvm -S taintSourceTest.cpp -o taintSourceTest.ll
