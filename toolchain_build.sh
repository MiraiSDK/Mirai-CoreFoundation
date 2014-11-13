#!/bin/sh

checkError()
{
    if [ "${1}" -ne "0" ]; then
        echo "*** Error: ${2}"
        exit ${1}
    fi
}

CFLAGS="$ARCHFLAGS" CPPFLAGS="$ARCHFLAGS" ./configure --host=arm-linux-androideabi --disable-gcd --disable-implicit-bridge
checkError $? "configure core-base failed"

make
checkError $? "build core-base failed"

make install

pushd $MIRAI_SDK_PREFIX/lib
mv libgnustep-corebase.so.0.2 libgnustep-corebase.so
rm libgnustep-corebase.so.0
popd