ACHLYS_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo "ACHLYS_ROOT is $DIR"

export PATH_TO_PASS=$ACHLYS_ROOT/lib/Achlys.so
export ACHLYSLLVM=$ACHLYS_ROOT/build/bin
