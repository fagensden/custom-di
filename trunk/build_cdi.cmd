@echo off
echo Building..
make -C di-usb clean all

echo eFIX: Creating di.bin
eFIX\eFIX di-usb\dimodule.elf>NUL

