#!/bin/bash

$ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
