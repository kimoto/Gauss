#!/bin/sh

set -x
TODAY_TIMESTAMP=`timestamp | cut -d- -f1`
CURDIR_NAME=`basename \`pwd\``
RELEASE_SET="./Release/${CURDIR_NAME}_${TODAY_TIMESTAMP}"
RELEASE_SET_ZIP="$RELEASE_SET.zip"

mkdir -p $RELEASE_SET
msbuild.exe /p:Configuration=Release

cp ./Release/Gauss.exe $RELEASE_SET/
cp ./Release/KeyHook.dll $RELEASE_SET/
cp ./readme.txt $RELEASE_SET/

zip -r $RELEASE_SET_ZIP $RELEASE_SET

