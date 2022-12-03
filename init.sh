ACHLYS_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo "ACHLYS_ROOT is $ACHLYS_ROOT"

export PATH_TO_PASS=$ACHLYS_ROOT/build/lib/Achlys.so
export ACHLYSLLVM=$ACHLYS_ROOT/build/bin
