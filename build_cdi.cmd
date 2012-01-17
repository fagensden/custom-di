@echo off
echo Building..
make -C di clean all

echo eFIX: Creating di.bin
eFIX\eFIX di\dimodule.elf>NUL

