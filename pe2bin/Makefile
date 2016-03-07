all : ../bin/pe2bin.exe ../bin/bindmp.exe

../bin/pe2bin.exe : main.cpp pe.h Makefile
	@gcc -Wall -x c++ -std=c++11 -o3 -DNDEBUG -march=i386 -m32 -mconsole -o pe2bin.exe main.cpp
	@mv -f pe2bin.exe ../bin/pe2bin.exe

../bin/bindmp.exe : bindmp.cpp pe.h Makefile
	@gcc -Wall -x c++ -std=c++11 -o3 -DNDEBUG -march=i386 -m32 -mconsole -o bindmp.exe bindmp.cpp
	@mv -f bindmp.exe ../bin/bindmp.exe