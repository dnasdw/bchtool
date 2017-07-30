#!/bin/bash

cwdir=`pwd`
rootdir=`dirname "$0"`
cd "$rootdir"
rootdir=`pwd`
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"$rootdir"
cd "$cwdir"
"$rootdir/bchtool" "$1" "$2" "$3" "$4" "$5" "$6"
