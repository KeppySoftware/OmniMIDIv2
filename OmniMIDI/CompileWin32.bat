@echo off

xmake f -p mingw --sdk="F:\Compilers\llvm-mingw-20250114-msvcrt-x86_64" --runtimes=c++_static -p windows -a x86
xmake
xmake f -p mingw --sdk="F:\Compilers\llvm-mingw-20250114-msvcrt-x86_64" --runtimes=c++_static -p windows -a x86_64
xmake
xmake f -p mingw --sdk="F:\Compilers\llvm-mingw-20250114-msvcrt-x86_64" --runtimes=c++_static -p windows -a arm64
xmake