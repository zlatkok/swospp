all : ../bin/patchit.com

pdata.asm : fcmp.pl $(SWOS_DIR)/swos_orig.exe $(SWOS_DIR)/patch_orig.exe
	perl fcmp.pl "$(SWOS_DIR)\\swos_orig.exe" "$(SWOS_DIR)\\patch_orig.exe" \
         >pdata.asm


../bin/patchit.com : pdata.asm patchit.asm util.asm
	nasm -fbin -w+orphan-labels -o..\\bin\\patchit.com patchit.asm