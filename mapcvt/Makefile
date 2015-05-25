# name of map file
map_file = swos.map

all : ../main/swos.asm ../main/swos.inc ../main/swossym.h

../main/swos.inc ../main/swos.asm : $(map_file) filter.txt parsemap.pl
	@echo Parsing map file...
	@perl parsemap.pl $(map_file)
	@mv -f swos.asm swos.inc swossym.h ../main/
