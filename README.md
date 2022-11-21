# Achlys
Achlys is a tool for automatic detection of NaN poisoning vulnerabilities in C/C++ programs based on LLVM

## Taint Analysis

### Setup
1. Clone this repository
2. `cd` into the directory of this repository
3. Run `./setup.sh`
4. Run `./build.sh` (_Note: It will take a long time as it builds llvm from source_)
5. set the environment variable for your shell: `export ACHLYSLLVM=$(pwd)/build/bin` (alternatively, you can add this to your `.bashrc`)

### Modifying TaintChecker
Run `./build.sh` from the root directory of this repository every time you make a change to TaintChecker

### Run Benchmark
- `cd Benchmarks/synthetic/testBasic/`
- Ensure that ACHLYS environment variables are loaded using `../../../init.sh`
- Compile cpp to LLVM IR using `./compile.sh`
- Enable/Disable Verbose Flag and run pass using `./runPass.sh`


## Fault Injection
TODO
