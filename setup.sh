#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

echo -e "Note: This script is for Ubuntu only"
echo -e "${CYAN}This script requires root privileges to install cmake ninja and other necessary components${OFF}"

if ! sudo apt-get -y install build-essential cmake ninja-build git wget;
then
	echo -e "${RED}Could not install build-essential, cmake, ninja-build and git. Are you sure you have sudo privileges?${OFF}"
	exit 1
fi


echo -e "${YELLOW}Fetching llvm-project-13.0.0 source code${OFF}"
if ! wget https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/llvm-project-13.0.0.src.tar.xz;
then
	echo -e "${RED}Could not fetch llvm-project-13.0.0. Are you sure you are connected to the internet?${OFF}"
	exit 1
fi


echo -e "${YELLOW}Extracting llvm-project-13.0.0${OFF}"
tar -xf llvm-project-13.0.0.src.tar.xz
rm llvm-project-13.0.0.src.tar.xz


echo -e "${YELLOW}Adding TaintChecker to llvm Transforms directory${OFF}"
ln -s $(pwd)/TaintChecker $(pwd)/llvm-project-13.0.0.src/llvm/lib/Transforms/TaintChecker
echo -e "add_subdirectory(TaintChecker)" >> llvm-project-13.0.0.src/llvm/lib/Transforms/CMakeLists.txt


echo -e "${YELLOW}Executing cmake${OFF}"
mkdir build
cd build
if ! cmake -G Ninja ../llvm-project-13.0.0.src/llvm/ \
	-DLLVM_ENABLE_PROJECTS="tools;clang;compiler-rt" \
	-DLLVM_TARGETS_TO_BUILD="host" \
	-DLLVM_ENABLE_ASSERTIONS=ON \
	-DLLVM_ENABLE_RTTI=ON \
	-DLLVM_OPTIMIZED_TABLEGEN=ON \
	-DCMAKE_BUILD_TYPE=Release;
then
	echo -e "${RED}cmake failed!${OFF}"
	exit 1
fi
cd ..


echo -e "${GREEN}The build environment is ready. You can now run ${CYAN}build.sh ${GREEN} to build TaintChecker${OFF}"
