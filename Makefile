# TODO: dist da zavisi od rel-a, & clean

# set default shell
SHELL = sh.exe

# needed in sub-makefiles
export SWOS_DIR = d:\games\swos

# variables used only here
STORE_DIR = c:\\backup\\swos\\swospp
#WEB_FILES_DIR = g:\usr\home_page\files
ZIP = "c:\Program Files\Winzip\winzip32.exe"

OBJ_DIR  := obj
BIN_DIR  := bin
DIST_DIR := dist
DOC_DIR  := doc
DBG_DIR  := $(OBJ_DIR)/dbg
REL_DIR  := $(OBJ_DIR)/rel

all : rel dbg

dbg : $(BIN_DIR)/loader.bin $(BIN_DIR)/patchit.com
	@cd pe2bin && $(MAKE)
	@cd mapcvt && $(MAKE)
	@cd patch && $(MAKE)
	@cd loader && $(MAKE)
	@cd main && perl Makefile.pl dbg
	@cp -f -u $(BIN_DIR)/swospp.bin $(SWOS_DIR)
	@cp -f -u $(BIN_DIR)/loader.bin $(SWOS_DIR)
	@cp -f -u $(BIN_DIR)/patchit.com $(SWOS_DIR)

rel : $(BIN_DIR)/loader.bin $(BIN_DIR)/patchit.com
	@cd pe2bin && $(MAKE)
	@cd mapcvt && $(MAKE)
	@cd patch && $(MAKE)
	@cd loader && $(MAKE)
	@cd main && perl Makefile.pl rel
	@cp -f -u $(BIN_DIR)/swospp.bin $(SWOS_DIR)
	@cp -f -u $(BIN_DIR)/loader.bin $(SWOS_DIR)
	@cp -f -u $(BIN_DIR)/patchit.com $(SWOS_DIR)

$(BIN_DIR)/loader.bin :
	@cd loader && $(MAKE)
	@cp -f $(BIN_DIR)/loader.bin $(SWOS_DIR)

$(BIN_DIR)/patchit.com :
	@cd patch && $(MAKE)
	@cp -f $(BIN_DIR)/patchit.com $(SWOS_DIR)

dist : rel $(DIST_DIR)/swospp.zip

$(DIST_DIR)/swospp.zip : $(DOC_DIR)\\file_id.diz $(DOC_DIR)\\readme.txt \
                         $(BIN_DIR)\\patchit.com $(BIN_DIR)\\loader.bin \
                         $(BIN_DIR)\\swospp.bin
	@echo Creating distributable archive...
	@$(ZIP) -min -a -ex $(DIST_DIR)/swospp.zip $?

store :
	@echo Making backup...
	@rm -rf $(STORE_DIR)/*
	@cp -r -p * --target-directory=$(STORE_DIR)

.PHONY : all dbg rel dist store clean