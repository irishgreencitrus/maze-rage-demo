CC	= /opt/gbdk-2020/build/gbdk/bin/lcc -Wa-l -Wl-m -Wl-j -I/opt/hUGEDriver/include/

BINS	= maze_game.gb

all:	$(BINS)

# Compile and link single file in one pass
%.gb:	%.c
	rgbasm -o hUGEDriver.obj -i /opt/hUGEDriver/ /opt/hUGEDriver/hUGEDriver.asm
	rgb2sdas hUGEDriver.obj
	$(CC) -o $@ $< hUGEDriver.obj.o music.c mainmenu_music.c
	rm -f *.o *.lst *.map *~ *.rel *.cdb *.ihx *.lnk *.sym *.asm *.noi
	

clean:
	rm -f *.o *.lst *.map *.gb *~ *.rel *.cdb *.ihx *.lnk *.sym *.asm *.noi
format:
	clang-format -i *.c

