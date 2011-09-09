@echo off
echo Building..
make -C di clean all

echo elfins: Creating di.bin
ELFIns\elfins 00000001.app di.bin di\dimodule.elf > NUL

