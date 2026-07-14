#!/usr/bin/env bash
# Builds mimicry-helper.exe with MinGW-w64 g++. Run from this directory.
#
# xcomreader/xcomwriter (collision meshes) and xnvmshreader/xnvmshwriter (Gothic 3's mesh
# variant) need mCCooking (PhysX-dependent, mi_cooking.cpp) which we don't build — they're
# excluded below since mimicry-helper doesn't use them (only .xmsh/.xmac + .xmat).
set -euo pipefail

SRC="vendor/mimicry-source/Mimicry"
OBJ_DIR="build/obj"
mkdir -p "$OBJ_DIR"

echo "Compiling mimicry library..."
for f in "$SRC"/*.cpp; do
  base=$(basename "$f" .cpp)
  case "$base" in
    mi_cooking|mi_xcomreader|mi_xcomwriter|mi_xnvmshreader|mi_xnvmshwriter) continue ;;
  esac
  g++ -c -I "vendor/mimicry-source" -std=gnu++17 -fpermissive -w "$f" -o "$OBJ_DIR/$base.o"
done

echo "Compiling driver..."
g++ -c -I "vendor/mimicry-source" -std=gnu++17 -fpermissive -w driver/main.cpp -o "$OBJ_DIR/main.o"

echo "Linking mimicry-helper.exe..."
g++ "$OBJ_DIR"/*.o -o mimicry-helper.exe -static

echo "Done: mimicry-helper.exe"
