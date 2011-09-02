@echo off
echo Building..
make -C cdi clean all

echo elfins: Creating di.bin
ELFIns\elfins 00000001.app di.bin di\dimodule.elf > NUL

