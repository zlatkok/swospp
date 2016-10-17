all : out/crash_test.exe out/qalloc_test.exe out/options_test.exe out/bfile_test.exe out/xml_test.exe

CC := wpp386
# escape ;'s or shell will try to execute them as separate commands
INCLUDES=-i=..\main\;..\main\crashlog\;..\main\qalloc\;..\main\mplayer\;..\main\xml
CRASH_CFLAGS =-bt=dos4g -d3 -s -wx -fp6 -mf -fr=nul -dDEBUG=1 -dTEST=1 $(INCLUDES)
CFLAGS =-bt=nt -j -d3 -hc -s -wx -fp6 -mf -fr=nul -dDEBUG=1 -dTEST=1 $(INCLUDES)

#
# crash test
#

obj/crash_test.obj : crash_test.c swos.h util.h dos.h ../main/crashlog/crashlog.h
	@$(CC) $(CRASH_CFLAGS) -fo=obj\crash_test.obj crash_test.c

obj/crash_mock.obj : crash_mock.c util.h swos.h
	@$(CC) $(CRASH_CFLAGS) -fo=obj\crash_mock.obj crash_mock.c

obj/crashlog.obj : ../main/crashlog/crashlog.cpp swos.h util.h dos.h ../main/crashlog/crashlog.h
	@$(CC) $(CRASH_CFLAGS) -fo=obj\crashlog.obj ..\main\crashlog\crashlog.cpp

obj/xmlparse_c.obj : ../main/xml/xmlparse.cpp ../main/types.h swos.h util.h \
    ../main/xml/xmlparse.h ../main/xml/xmltree.h dos.h ../main/qalloc/qalloc.h
	@$(CC) $(CRASH_CFLAGS) -fo=obj\xmlparse_c.obj ..\main\xml\xmlparse.cpp

obj/vsprintf.obj : ../main/vsprintf.cpp swos.h
	@$(CC) $(CRASH_CFLAGS) -fo=obj\vsprintf.obj ..\main\vsprintf.cpp

out/crash_test.exe : obj/crash_test.obj obj/crashlog.obj obj/crash_mock.obj obj/vsprintf.obj obj/xmlparse_c.obj
	@echo option quiet, stack=32k, map system dos4g name $@ file obj/crash_test.obj >obj/lnk.txt
	@echo file obj/crashlog.obj, obj/crash_mock.obj, obj/vsprintf.obj >>obj/lnk.txt
	@wlink @obj/lnk.txt
	@mv crash_test.map map

#
# qalloc test
#

obj/qalloc_mock.obj : qalloc_mock.c swos.h util.h
	@$(CC) $(CFLAGS) -fo=obj\qalloc_mock.obj qalloc_mock.c

obj/qalloc.obj : ../main/qalloc/qalloc.cpp ../main/qalloc/qalloc.h swos.h util.h
	@$(CC) $(CFLAGS) -fo=obj\qalloc.obj ..\main\qalloc\qalloc.cpp

obj/qalloc_test.obj : qalloc_test.c ../main/qalloc/qalloc.h
	@$(CC) $(CFLAGS) -fo=obj\qalloc_test.obj qalloc_test.c

out/qalloc_test.exe : obj/qalloc_test.obj obj/qalloc.obj obj/qalloc_mock.obj
	@echo Debug Codeview option quiet, stack=32k, map system nt name $@ >obj/lnk.txt
	@echo file obj/qalloc_test.obj, obj/qalloc.obj, obj/qalloc_mock.obj >>obj/lnk.txt
	@wlink @obj/lnk.txt
	@mv qalloc_test.map map

#
# options test
#

obj/options_test.obj : options_test.c swos.h ../main/qalloc/qalloc.h bfile_mock.h util.h
	@$(CC) $(CFLAGS) -fo=obj\options_test.obj options_test.c

obj/options.obj : ../main/options.cpp swos.h util.h ../main/options.h ../main/xml/xmltree.h
	@$(CC) $(CFLAGS) -fo=obj\options.obj ..\main\options.cpp

obj/xmltree.obj : ../main/xml/xmltree.cpp util.h ../main/xml/xmltree.h dos.h \
    ../main/qalloc/qalloc.h swos.h
	@$(CC) $(CFLAGS) -fo=obj\xmltree.obj ..\main\xml\xmltree.cpp

obj/xmlparse.obj : ../main/xml/xmlparse.cpp ../main/types.h swos.h util.h \
    ../main/xml/xmlparse.h ../main/xml/xmltree.h dos.h ../main/qalloc/qalloc.h
	@$(CC) $(CFLAGS) -fo=obj\xmlparse.obj ..\main\xml\xmlparse.cpp

obj/util.obj : ../main/util.cpp swos.h util.h
	@$(CC) $(CFLAGS) -fo=obj\util.obj ..\main\util.cpp

obj/options_mock.obj : options_mock.c ../main/types.h ../main/mplayer/mplayer.h \
    ../main/mplayer/dosipx.h ../main/xml/xmltree.h
	@$(CC) $(CFLAGS) -fo=obj\options_mock.obj options_mock.c

obj/bfile_mock.obj : bfile_mock.c
	@$(CC) $(CFLAGS) -fo=obj\bfile_mock.obj bfile_mock.c

out/options_test.exe : obj/options_test.obj obj/options.obj obj/xmltree.obj obj/qalloc.obj \
    obj/options_mock.obj obj/qalloc_mock.obj obj/util.obj obj/xmlparse.obj obj/bfile_mock.obj
	@echo Debug Codeview All option quiet, stack=128k, map, cvpack system nt name $@ >obj/lnk.txt
	@echo file clib3r.lib\(memset\), clib3r.lib\(_strcmp\), clib3r.lib\(strncmp\) >>obj/lnk.txt
	@echo file clib3r.lib\(memmove\), clib3r.lib\(strlen\), clib3r.lib\(segread\) >>obj/lnk.txt
	@echo file clib3r.lib\(exit\), clib3r.lib\(time\), clib3r.lib\(asctime.c\) >>obj/lnk.txt
	@echo file obj/options_test.obj, obj/options.obj, obj/qalloc.obj >>obj/lnk.txt
	@echo file obj/options_mock.obj, obj/xmltree.obj, obj/qalloc_mock.obj >>obj/lnk.txt
	@echo file obj/util.obj, obj/xmlparse.obj, obj/bfile_mock.obj disable 1027 >>obj/lnk.txt
	@wlink @obj/lnk.txt
	@mv options_test.map map

#
# BFile test
#

obj/bfile_test.obj : bfile_test.cpp bfile_mock.h util.h ../main/types.h dos.h
	@$(CC) $(CFLAGS) -fo=obj\bfile_test.obj bfile_test.cpp

out/bfile_test.exe : obj/bfile_test.obj obj/util.obj obj/options_mock.obj obj/qalloc.obj \
    obj/qalloc_mock.obj obj/bfile_mock.obj
	@echo Debug Codeview All option quiet, stack=32k, map, cvpack system nt name $@ >obj/lnk.txt
	@echo file clib3r.lib\(memset\), clib3r.lib\(_strcmp\) >>obj/lnk.txt
	@echo file clib3r.lib\(memmove\), clib3r.lib\(strlen\), clib3r.lib\(segread\) >>obj/lnk.txt
	@echo file clib3r.lib\(exit\), clib3r.lib\(time\), clib3r.lib\(asctime.c\) >>obj/lnk.txt
	@echo file clib3r.lib\(rand\), obj/bfile_test.obj, obj/util.obj >>obj/lnk.txt
	@echo file obj/options_mock.obj, obj/qalloc.obj, obj/qalloc_mock.obj, obj/bfile_mock.obj >>obj/lnk.txt
	@echo disable 1027 >>obj/lnk.txt
	@wlink @obj/lnk.txt
	@mv bfile_test.map map

#
# XML test
#

obj/xml_test.obj : xml_test.c ../main/types.h swos.h util.h ../main/xml/xmltree.h \
    ../main/xml/xmlparse.h ../main/qalloc/qalloc.h bfile_mock.h dos.h
	@$(CC) $(CFLAGS) -fo=obj\xml_test.obj xml_test.c

out/xml_test.exe : obj/xml_test.obj obj/xmlparse.obj obj/xmlparse.obj obj/util.obj obj/options_mock.obj \
    obj/qalloc_mock.obj obj/qalloc.obj obj/bfile_mock.obj obj/xmltree.obj
	@echo Debug Codeview All option quiet, stack=64k, map, cvpack system nt name $@ >obj/lnk.txt
	@echo file clib3r.lib\(memset\), clib3r.lib\(_strcmp\), clib3r.lib\(strncmp\) >>obj/lnk.txt
	@echo file clib3r.lib\(memmove\), clib3r.lib\(strlen\), clib3r.lib\(segread\) >>obj/lnk.txt
	@echo file clib3r.lib\(exit\), clib3r.lib\(time\), clib3r.lib\(asctime.c\) >>obj/lnk.txt
	@echo file clib3r.lib\(rand\), obj/xml_test.obj, obj/xmlparse.obj, obj/util.obj >>obj/lnk.txt
	@echo file obj/options_mock.obj, obj/qalloc_mock.obj, obj/qalloc.obj, obj/bfile_mock.obj >>obj/lnk.txt
	@echo file obj/xmltree.obj >>obj/lnk.txt
	@echo disable 1027 >>obj/lnk.txt
	@wlink @obj/lnk.txt
	@mv xml_test.map map

obj/lnk.txt : .INTERMEDIATE