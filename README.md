# Achlys
Achlys is a tool for automatic detection of NaN poisoning vulnerabilities in C/C++ programs based on LLVM

## Taint Analysis

### Compile LLVM
1. Clone the repository `git@github.com:uditagarwal97/Achlys.git`
2. Download source code for [LLVM 13.0.0](https://github.com/llvm/llvm-project/releases/tag/llvmorg-13.0.0)
3. Add symbolic link for TaintChecker/ in llvm
`ln -s $(pwd)/TaintChecker $(pwd)/llvm-project-llvmorg-13.0.0/llvm/lib/Transforms/TaintChecker`
4. Add TaintChecker to the CMakeLists
`echo "add_subdirectory(TaintChecker)" >> llvm-project-llvmorg-13.0.0/llvm/lib/Transforms/CMakeLists.txt`
5. Create a build directory `mkdir build && cd build`
6. Create the build files using cmake. Additional params to build for single host architecture with Ninja based optimizations
```cmake -G Ninja ../llvm-project-llvmorg-13.0.0/llvm/ -DLLVM_ENABLE_PROJECTS="tools;clang;compiler-rt"  -DLLVM_TARGETS_TO_BUILD="host"  -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_OPTIMIZED_TABLEGEN=ON -DCMAKE_BUILD_TYPE=Release```
7. Install the Ninja `sudo apt-get install ninja-build`
8. Export Path to Achlys - `export PATH_TO_PASS=$(pwd)/lib/Achlys.so`

### Usage
Run these steps in the `build` directoryafter making any changes to the TaintChecker
- Create the build orchestration using `ninja -j4 -j2`
- Export variable for binaries [Recommended] - `export ACHLYSLLVM=$(pwd)/build/bin`
- Or install binaries globally - `sudo ninja install`

### Run Benchmark
- `cd Benchmarks/synthetic/testBasic/
- Compile cpp to LLVM IR using `./compile.sh`
- Enable/Disable Verbose Flag and run pass using `./runPass.sh`


## Fault Injection
TODO