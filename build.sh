#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

echo -e "${YELLOW}Building TaintChecker${OFF}"
echo -e "${YELLOW}Note that the first build will take quite a lot of time. This is normal. The future builds should be faster${OFF}"

cd build
ninja -j$(nproc)
export ACHLYSLLVM=$(pwd)/bin
cd ..



