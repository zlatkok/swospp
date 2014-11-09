# Makefile for ZK binaries loader

../bin/loader.bin : loader.asm util.asm misc.inc
	nasm -o..\\bin\\loader.bin -fbin -l..\\etc\\loader.lst -w+orphan-labels \
         -w+macro-params -O3 loader.asm
	@cp -f -u ../bin/loader.bin $(SWOS_DIR)