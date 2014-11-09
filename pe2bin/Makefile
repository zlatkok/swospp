all : ../bin/pe2bin.exe ../bin/bindmp.exe

../bin/pe2bin.exe : main.c pe.h
	@wcl386 -ox -fe=pe2bin.exe main.c -fr=nul -bt=nt -wcd=138 | sed -e'/Watcom/Id' -e'/Sybase/Id'
	@mv -f pe2bin.exe ../bin/pe2bin.exe

../bin/bindmp.exe : bindmp.c pe.h
	@wcl386 -ox -fe=bindmp.exe bindmp.c -fr=nul -bt=nt -wcd=138 | sed -e'/Watcom/Id' -e'/Sybase/Id'
	@mv -f bindmp.exe ../bin/bindmp.exe