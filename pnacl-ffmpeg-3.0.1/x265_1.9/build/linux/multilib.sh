#!/bin/sh

mkdir -p 8bit 10bit 12bit

cd 12bit
cmake -DENABLE_LIBNUMA:BOOL=OFF -DCMAKE_LIBNUMA:BOOL=OFF -DYASM_EXECUTABLE:FILEPATH= -DCMAKE_STRIP:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-strip -DCMAKE_RANLIB:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ranlib -DCMAKE_NM:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-nm -DCMAKE_LINKER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ld -DCMAKE_C_COMPILER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-clang -DCMAKE_CXX_COMPILER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-clang++ -DCMAKE_AR:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ar -DCMAKE_INSTALL_PREFIX:PATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/le32-nacl/usr  -DENABLE_SHARED:BOOL=OFF -G "Unix Makefiles" ../../../source -DHIGH_BIT_DEPTH=ON -DEXPORT_C_API=OFF -DENABLE_SHARED=OFF -DENABLE_CLI=OFF -DMAIN12=ON && ccmake ../../../source
make ${MAKEFLAGS}

cd ../10bit
cmake -DENABLE_LIBNUMA:BOOL=OFF -DCMAKE_LIBNUMA:BOOL=OFF -DYASM_EXECUTABLE:FILEPATH= -DCMAKE_STRIP:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-strip -DCMAKE_RANLIB:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ranlib -DCMAKE_NM:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-nm -DCMAKE_LINKER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ld -DCMAKE_C_COMPILER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-clang -DCMAKE_CXX_COMPILER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-clang++ -DCMAKE_AR:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ar -DCMAKE_INSTALL_PREFIX:PATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/le32-nacl/usr -DENABLE_SHARED:BOOL=OFF -G "Unix Makefiles" ../../../source -DHIGH_BIT_DEPTH=ON -DEXPORT_C_API=OFF -DENABLE_SHARED=OFF -DENABLE_CLI=OFF && ccmake ../../../source
make ${MAKEFLAGS}

cd ../8bit
ln -sf ../10bit/libx265.a libx265_main10.a
ln -sf ../12bit/libx265.a libx265_main12.a
cmake -DENABLE_CLI:BOOL=OFF -DENABLE_LIBNUMA:BOOL=OFF -DCMAKE_LIBNUMA:BOOL=OFF -DYASM_EXECUTABLE:FILEPATH= -DCMAKE_STRIP:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-strip -DCMAKE_RANLIB:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ranlib -DCMAKE_NM:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-nm -DCMAKE_LINKER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ld -DCMAKE_C_COMPILER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-clang -DCMAKE_CXX_COMPILER:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-clang++ -DCMAKE_AR:FILEPATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/bin/pnacl-ar -DCMAKE_INSTALL_PREFIX:PATH=/e/nacl_sdk/pepper_49/toolchain/linux_pnacl/le32-nacl/usr -DENABLE_SHARED:BOOL=OFF -G "Unix Makefiles" ../../../source -DEXTRA_LIB="x265_main10.a;x265_main12.a" -DEXTRA_LINK_FLAGS=-L. -DLINKED_10BIT=ON -DLINKED_12BIT=ON && ccmake ../../../source
make ${MAKEFLAGS}

# rename the 8bit library, then combine all three into libx265.a
mv libx265.a libx265_main.a

uname=`uname`
if [ "$uname" = "Linux" ]
then

# On Linux, we use GNU ar to combine the static libraries together
ar -M <<EOF
CREATE libx265.a
ADDLIB libx265_main.a
ADDLIB libx265_main10.a
ADDLIB libx265_main12.a
SAVE
END
EOF

else

# Mac/BSD libtool
libtool -static -o libx265.a libx265_main.a libx265_main10.a libx265_main12.a 2>/dev/null

fi

pnacl-ranlib libx265.a
