#!/bin/bash
# Incremental per-commit full builds of Xen for all architectures
# Usage: inc.sh <arch> <base> <commit>...
arch=$1; base=$2; shift 2
repo=/home/bkaindl/gh/xen
d=/tmp/xi-$arch
case $arch in
x86_64) mk="make -C xen" ;;
arm64) mk="make -C xen XEN_TARGET_ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-" ;;
arm32) mk="make -C xen XEN_TARGET_ARCH=arm32 CROSS_COMPILE=arm-linux-gnueabihf-" ;;
riscv64) mk="make -C xen XEN_TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu-" ;;
esac
rm -rf $d; mkdir -p $d
(cd $repo && git archive $base xen config Config.mk Makefile CODING_STYLE tools/flask tools/Rules.mk tools/include | tar -x -C $d)
cd $d
$mk defconfig >/dev/null 2>&1
for l in $EXTRA; do case $l in !*) echo "# ${l#!} is not set" >> xen/.config;; *) echo "$l" >> xen/.config;; esac; done
yes '' | $mk olddefconfig >/dev/null 2>&1
echo "config: $(grep -E '^CONFIG_(XSM_FLASK|NUMA|LLC_COLORING|XSM)=' xen/.config | tr '\n' ' ')"
$mk -j$(nproc) > /tmp/inc-$arch-base.log 2>&1; echo "base rc=$?"
prev=$base
for c in "$@"; do
  files=$(cd $repo && git diff --name-only $prev $c -- xen)
  for f in $files; do
    if (cd $repo && git cat-file -e $c:$f 2>/dev/null); then
      mkdir -p $(dirname $f); (cd $repo && git show $c:$f) > $f
    else rm -f $f; fi
  done
  $mk -j$(nproc) > /tmp/inc-$arch-$c.log 2>&1; rc=$?
  echo "$c rc=$rc warnings=$(grep -c 'warning:' /tmp/inc-$arch-$c.log)"
  grep -E 'error|warning:' /tmp/inc-$arch-$c.log | head -5
  prev=$c
done
