#include <gb/gb.h>
#include <gb/hardware.h>
#include <rand.h>

#include "hUGEDriver.h"
#include "assets/MazeMap.c"
#include "assets/MazeTiles.c"
#include "assets/Numbers.c"
#include "assets/Sprites.c"
#include "assets/choose_mode_data.c"
#include "assets/choose_mode_map.c"
#include "assets/levelselect_data.c"
#include "assets/levelselect_map.c"
#include "assets/newtitle_data.c"
#include "assets/newtitle_map.c"
#include "assets/studio_data.c"
#include "assets/studio_map.c"
#include "assets/winscreen_data.c"
#include "assets/winscreen_map.c"

#define MAX_WIDTH 15
#define MAX_HEIGHT MAX_WIDTH
#define SEED 101
#define WEIGHT 2
#define N 1
#define S 2
#define E 4
#define W 8
#define FADE_DELAY 5
#define MODE_EASY 0
#define MODE_HARD 1
#define MODE_INF 2
#define SELECTOR_X 60

extern const hUGESong_t music;
extern const hUGESong_t mainmenu_music;

unsigned char grid[MAX_HEIGHT][MAX_WIDTH] = {{0}};
unsigned char playerpos[2] = {16, 24};

unsigned char i = 0, y = 0, x = 0;

unsigned char bg1, bg2;
unsigned char option = 0, coins = 0, canscroll = 0, completed_levels = 0;
unsigned char gameplay = 1, pallete_flashing = 1, window_shown = 1;

unsigned char level = 2;

unsigned char konami_inputted = 0;
unsigned char old_joy = 0;

unsigned char just_keys;
unsigned char keys;
unsigned char rand_max(unsigned char in) { return (unsigned char)rand() % in; }

// Delays execution for f frames.
// More performant than built in delay as it relies on interrupts,
// and not cpu usage. Won't max out CPU

void delay_frames(unsigned char f) {
	for (unsigned char j = 0; j < f; j++) {
		wait_vbl_done();
	}
}

// Fades Background and Sprite palletes to black.
void fadeout() {
	for (i = 0; i < 4; i++) {
		switch (i) {
			case 0:
				BGP_REG = 0xE4;
				OBP0_REG = 0xE4;
				break;
			case 1:
				BGP_REG = 0xF9;
				OBP0_REG = 0xF9;
				break;
			case 2:
				BGP_REG = 0xFE;
				OBP0_REG = 0xFE;
				break;
			case 3:
				BGP_REG = 0xFF;
				OBP0_REG = 0xFF;
				break;
		}
		delay_frames(FADE_DELAY);
	}
	delay_frames(FADE_DELAY);
}

// To be used after fadeout.
// Fades palletes back to white.
void fadein() {
	for (i = 0; i < 3; i++) {
		switch (i) {
			case 0:
				BGP_REG = 0xFE;
				OBP0_REG = 0xFE;
				break;
			case 1:
				BGP_REG = 0xF9;
				OBP0_REG = 0xF9;
				break;
			case 2:
				BGP_REG = 0xE4;
				OBP0_REG = 0xE4;
				break;
		}
		delay_frames(FADE_DELAY);
	}
}

// Scrolls background and sprite.
void move_entire_window(signed char relativex, signed char relativey) {
	scroll_bkg(relativex, relativey);
	scroll_sprite(0, -relativex, -relativey);
}

// Smooth player movement
void interp_player(signed char relativex, signed char relativey) {
	while (relativex != 0) {
		scroll_sprite(0, relativex < 0 ? -2 : 2, 0);
		relativex += relativex < 0 ? 2 : -2;
		wait_vbl_done();
	}
	while (relativey != 0) {
		scroll_sprite(0, 0, relativey < 0 ? -2 : 2);
		relativey += relativey < 0 ? 2 : -2;
		wait_vbl_done();
	}
}

// Converts player position to tile position and sets tile
void set_tile(unsigned char pixelx, unsigned char pixely, unsigned char tile) {
	unsigned char tilex, tiley;
	tilex = (pixelx - 8) / 8;
	tiley = (pixely - 16) / 8;
	set_bkg_tile_xy(tilex, tiley, tile);
}

// Returns tile at player position
unsigned char get_tile(unsigned char pixelx, unsigned char pixely) {
	unsigned char tilex, tiley;
	tilex = (pixelx - 8) / 8;
	tiley = (pixely - 16) / 8;
	return get_bkg_tile_xy(tilex, tiley);
}

unsigned char is_dead_end(unsigned char gx, unsigned char gy) {
	unsigned char ends = 0;
	if (grid[gy][gx] & N) {
		ends++;
	}
	if (grid[gy][gx] & S) {
		ends++;
	}
	if (grid[gy][gx] & E) {
		ends++;
	}
	if (grid[gy][gx] & W) {
		ends++;
	}
	return ends == 1;
}

void generatemaze_sidewinder() {
	unsigned char runstart;
	unsigned char cell;
	for (y = 0; y < level; y++) {
		runstart = 0;
		for (x = 0; x < level; x++) {
			if (y > 0 && (x + 1 == level || rand_max(WEIGHT) == 0)) {
				cell = runstart + rand_max(x - runstart + 1);
				grid[y][cell] |= N;
				grid[y - 1][cell] |= S;
				runstart = x + 1;
			} else if (x + 1 < level) {
				grid[y][x] |= E;
				grid[y][x + 1] |= W;
			}
		}
	}
}

void set_coins() {
	x = coins;
	y = 0;
	i = 0;
	if (x) {
		while (x) {
			i = x % 10;
			set_win_tile_xy(4 - y, 0, 0x04 + i);
			x /= 10;
			y++;
		}

	} else {
		set_win_tile_xy(4, 0, 0x04);
		set_win_tile_xy(3, 0, 0x04);
		set_win_tile_xy(2, 0, 0x04);
	}
}

void set_levels() {
	x = completed_levels;
	y = 0;
	i = 0;
	while (x) {
		i = x % 10;
		set_win_tile_xy(4 - y, 1, 0x04 + i);
		x /= 10;
		y++;
	}
}

void setgridtiles() {
	for (y = 0; y < 2 * level + 1; y++) {
		for (x = 0; x < 2 * level + 1; x++) {
			set_bkg_tile_xy(x, y, 1);
		}
	}
	unsigned char gridx = 0;
	unsigned char gridy = 0;
	for (y = 1; y < 2 * level + 1; y += 2) {
		for (x = 1; x < 2 * level + 1; x += 2) {
			if (grid[gridy][gridx] != 0) {
				set_bkg_tile_xy(x, y, 0);
			}
			if ((grid[gridy][gridx] & N) == N) {
				set_bkg_tile_xy(x, y - 1, 0);
			}
			if ((grid[gridy][gridx] & S) == S) {
				set_bkg_tile_xy(x, y + 1, 0);
			}
			if ((grid[gridy][gridx] & E) == E) {
				set_bkg_tile_xy(x + 1, y, 0);
			}
			if ((grid[gridy][gridx] & W) == W) {
				set_bkg_tile_xy(x - 1, y, 0);
			}
			gridx++;
		}
		gridx = 0;
		gridy++;
	}
	gridx = level - 1;
	gridy = level - 1;
	unsigned char goalflag = 0;
	for (y = 2 * level + 1; y > 1; y -= 2) {
		for (x = 2 * level + 1; x > 1; x -= 2) {
			if (gridx == 0 && gridy == 0) {
			} else {
				if (is_dead_end(gridx, gridy) && !goalflag) {
					set_bkg_tile_xy(x - 2, y - 2, 3);
					goalflag = 1;
				} else if (is_dead_end(gridx, gridy) && option) {
					set_bkg_tile_xy(x - 2, y - 2, 2);
					coins++;
				}
			}
			gridx--;
		}
		gridx = level - 1;
		gridy--;
	}
	if (option) {
		set_coins();
	}
}

void init_game() {
	set_bkg_data(0, 4, MazeTileset);
	set_bkg_tiles(0, 0, MazeMapHeight, MazeMapWidth, MazeMap);
	set_sprite_tile(0, 0);
	playerpos[0] = 16;
	playerpos[1] = 24;
	move_sprite(0, playerpos[0], playerpos[1]);
	move_bkg(0, 0);
	setgridtiles();
	switch (level) {
		case 2:
			move_entire_window(-60, -52);
			break;
		case 3:
			move_entire_window(-52, -44);
			break;
		case 4:
			move_entire_window(-44, -36);
			break;
		case 5:
			move_entire_window(-36, -28);
			break;
		case 6:
			move_entire_window(-28, -20);
			break;
		case 7:
			move_entire_window(-20, -12);
			break;
		case 8:
			move_entire_window(-12, -4);
			break;
		case 9:
			move_entire_window(-4, 4);
			break;
		default:
			canscroll = 1;
			break;
	}
}

void init_logo() {
	BGP_REG = 0xff;
	set_bkg_data(0, 95, studio_data);
	set_bkg_tiles(0, 0, 20, 18, studio_map);
	fadein();
	delay_frames(120);
	fadeout();
}

void init_splash() {
	set_bkg_data(0, 86, newtitle_data);
	set_bkg_tiles(0, 0, MazeMapHeight, MazeMapWidth, MazeMap);
	set_bkg_tiles(0, 0, 20, 18, newtitle_map);
}

void init_winscreen() {
	set_bkg_data(0, 199, winscreen_data);
	set_bkg_tiles(0, 0, 20, 18, winscreen_map);
}

void transition_level() {
	fadeout();
	HIDE_BKG;
	HIDE_SPRITES;
	for (y = 0; y < MAX_HEIGHT; y++) {
		for (x = 0; x < MAX_WIDTH; x++) {
			grid[y][x] = 0;
		}
	}
	generatemaze_sidewinder();
	init_game();
	SHOW_BKG;
	SHOW_SPRITES;
	fadein();
}

unsigned char reset_if_invalid() {
	if (get_tile(playerpos[0], playerpos[1]) == 1) {
		// If you are on the wall, reset and don't animate movement.
		reset();
		return 0;
	} else if (get_tile(playerpos[0], playerpos[1]) == 2) {
		// If you are on a coin, collect in and decrement the coin counter
		set_tile(playerpos[0], playerpos[1], 0);
		coins--;
		set_coins();
		return 1;
	} else if (get_tile(playerpos[0], playerpos[1]) == 3) {
		if (coins != 0) {
			// If you are on the goal, but you don't have all the coins ignore
			return 1;
		}
		if (option == MODE_INF) {
			// If you're in infinite mode, increment completed levels and choose a
			// random new level
			level = rand_max(MAX_WIDTH - 2) + 2;
			completed_levels++;
			set_levels();
			transition_level();
			return 0;
		}
		if (level != MAX_WIDTH) {
			// Otherwise, if you aren't on the last level add one to the level
			level++;
			transition_level();
		} else {
			// If you are on the last level, show the win screen and end gameplay.
			gameplay = 0;
		}
		return 0;
	} else {
		return 1;
	}
}

unsigned char counter = 0;
unsigned char text_pallete = 0xE4;

void parallax() {
	switch (LYC_REG) {
		case 0:
			move_bkg(bg1, 0);
			LYC_REG = 32;
			BGP_REG = 0xE4;
			break;
		case 32:
			move_bkg(0, 0);
			LYC_REG = 112;
			if (pallete_flashing) {
				if (counter % 128 == 0) {
					text_pallete = 0xE4;
				} else if (counter % 64 == 0) {
					text_pallete = 0x24;
				}
			} else {
				text_pallete = 0xE4;
			}
			BGP_REG = text_pallete;
			break;
		case 112:
			move_bkg(bg2, 0);
			LYC_REG = 0;
			BGP_REG = 0xE4;
			break;
	}
}

unsigned char joypad_justpressed() {
	i = ~old_joy & joypad();
	old_joy = joypad();
	return i;
}

unsigned char check_konami() {
	if (!just_keys) {
		return 0;
	}
	switch (konami_inputted) {
		case 0:
			if (just_keys & J_UP) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 1:
			if (just_keys & J_UP) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 2:
			if (just_keys & J_DOWN) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 3:
			if (just_keys & J_DOWN) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 4:
			if (just_keys & J_LEFT) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 5:
			if (just_keys & J_RIGHT) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 6:
			if (just_keys & J_LEFT) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 7:
			if (just_keys & J_RIGHT) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 8:
			if (just_keys & J_B) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 9:
			if (just_keys & J_A) {
				konami_inputted++;
			} else {
				konami_inputted = 0;
			}
			return 0;
		case 10:
			if (just_keys & J_START) {
				return 1;
			} else {
				konami_inputted = 0;
			}
	}
	return 0;
}

void remove_parallax() {
	bg1 = 0;
	bg2 = 0;
	remove_LCD(parallax);
	BGP_REG = 0xE4;
	move_bkg(0, 0);
}

void init_hidden() {
	set_bkg_data(0, 52, levelselect_data);
	set_bkg_tiles(0, 0, 20, 18, levelselect_map);
	set_sprite_tile(0, 1);
	set_sprite_tile(1, 1);
	move_sprite(0, 20, 48);
	move_sprite(1, 36, 144);
}

void init_coin() {
	set_bkg_data(4, 11, Numbers);
	SHOW_WIN;
	set_win_tile_xy(0, 0, 0x02);
	set_win_tile_xy(1, 0, 0x0E);
	set_win_tile_xy(2, 0, 0x04);
	set_win_tile_xy(3, 0, 0x04);
	set_win_tile_xy(4, 0, 0x04);
	if (option == MODE_HARD) {
		move_win(125, 134);
	} else {
		move_win(125, 126);
		set_win_tile_xy(0, 1, 0x03);
		set_win_tile_xy(1, 1, 0x0E);
		set_win_tile_xy(2, 1, 0x04);
		set_win_tile_xy(3, 1, 0x04);
		set_win_tile_xy(4, 1, 0x04);
	}
}

void main() {
	DISPLAY_OFF;
	set_sprite_data(0, 2, MazeSprites);
	SHOW_BKG;
	HIDE_WIN;
	SHOW_SPRITES;
	DISPLAY_ON;
	init_logo();
	init_splash();
	fadein();
	NR52_REG = 0x80;
	NR51_REG = 0xFF;
	NR50_REG = 0x77;
	STAT_REG = 0x45;
	LYC_REG = 0x00;
	disable_interrupts();
	__critical {
		hUGE_init(&mainmenu_music);
		add_VBL(hUGE_dosound);
	}
	add_LCD(parallax);
	enable_interrupts();
	set_interrupts(VBL_IFLAG | LCD_IFLAG);
	while (1) {
		just_keys = joypad_justpressed();
		keys = joypad();
		if (check_konami()) {
			option = 0xFF;
			break;
		}
		if (pallete_flashing) {
			if (keys & J_START) {
				pallete_flashing = 0;
				set_bkg_data(79, 45, choose_mode_data);
				set_bkg_tiles(0, 4, 20, 10, choose_mode_map);
				set_sprite_tile(0, 1);
				move_sprite(0, 60, 80);
			}
		} else {
			if (option == MODE_EASY) {
				move_sprite(0, SELECTOR_X, 72);
			} else if (option == MODE_HARD) {
				move_sprite(0, SELECTOR_X, 88);
			} else {
				move_sprite(0, SELECTOR_X, 104);
			}
			if (just_keys & J_SELECT) {
				switch (option) {
					case MODE_EASY:
						option = MODE_HARD;
						break;
					case MODE_HARD:
						option = MODE_INF;
						break;
					case MODE_INF:
						option = MODE_EASY;
						break;
				}
			} else if (just_keys & J_A) {
				break;
			}
		}
		bg1 += 1;
		bg2 += 2;
		counter++;
		wait_vbl_done();
	}
	remove_parallax();
	if (option == 0xFF) {
		fadeout();
		option = 0;
		playerpos[0] = 0;
		playerpos[1] = 0;
		init_hidden();
		delay_frames(10);
		fadein();
		while (1) {
			just_keys = joypad_justpressed();
			keys = joypad();
			if (just_keys & J_START) {
				break;
			}
			if (just_keys & J_SELECT) {
				option = !option;
			}
			if (option) {
				move_sprite(1, 84, 144);
			} else {
				move_sprite(1, 36, 144);
			}
			if ((keys & J_B) && (keys & J_A)) {
			} else {
				if ((just_keys & J_UP) && playerpos[1] > 0) {
					playerpos[1]--;
				} else if ((just_keys & J_DOWN) && playerpos[1] < 2) {
					if (!(playerpos[1] == 1 && playerpos[0] == 4)) {
						playerpos[1]++;
					}
				} else if ((just_keys & J_LEFT) && playerpos[0] > 0) {
					playerpos[0]--;
				} else if ((just_keys & J_RIGHT) && playerpos[0] < 4) {
					if (!(playerpos[1] == 2 && playerpos[0] == 3)) {
						playerpos[0]++;
					}
				}
			}
			move_sprite(0, 20 + playerpos[0] * 24, 48 + playerpos[1] * 16);
			wait_vbl_done();
		}
		move_sprite(1, 0, 0);
		level = (2 + playerpos[1]) + (3 * playerpos[0]);
	}
	initrand(DIV_REG);
	__critical {
		hUGE_reset_wave();
		hUGE_init(&music);
	}

	fadeout();
	generatemaze_sidewinder();
	if (option) {
		init_coin();
	}
	init_game();
	fadein();
	while (gameplay) {
		just_keys = joypad_justpressed();
		keys = joypad();
		if (just_keys & J_SELECT) {
			window_shown = !window_shown;
		}
		if (window_shown && option) {
			SHOW_WIN;
		} else {
			HIDE_WIN;
		}
		if (canscroll && (keys & J_B)) {
			if (keys & J_RIGHT) {
				move_entire_window(-4, 0);
			}
			if (keys & J_LEFT) {
				move_entire_window(4, 0);
			}
			if (keys & J_DOWN) {
				move_entire_window(0, -4);
			}
			if (keys & J_UP) {
				move_entire_window(0, 4);
			}
		} else {
			if (keys & J_RIGHT) {
				playerpos[0] += 8;
				if (reset_if_invalid()) {
					interp_player(8, 0);
				}
			}
			if (keys & J_LEFT) {
				playerpos[0] -= 8;
				if (reset_if_invalid()) {
					interp_player(-8, 0);
				}
			}
			if (keys & J_UP) {
				playerpos[1] -= 8;
				if (reset_if_invalid()) {
					interp_player(0, -8);
				}
			}
			if (keys & J_DOWN) {
				playerpos[1] += 8;
				if (reset_if_invalid()) {
					interp_player(0, 8);
				}
			}
		}
		delay_frames(4);
	}
	fadeout();
	HIDE_SPRITES;
	init_winscreen();
	move_bkg(0, 0);
	fadein();
	waitpadup();
	while (1) {
		if (joypad()) {
			reset();
		}
		wait_vbl_done();
	}
}
