/*
	Copyright (C) 2013, 2014 Craig McPartland

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL/SDL.h>

#define GAME_QUIT 0
#define GAME_START 1
#define GAME_PREPLAY 2
#define GAME_PLAY 3
#define GAME_PAUSE 4
#define GAME_STRIKE 5
#define GAME_LOSE 6
#define GAME_WIN 7

#define HEIGHT 480
#define WIDTH 640
#define STOREY_WIDTH 20
#define STOREY_HEIGHT 20
#define SCORE_INCREMENT 20
#define START_LEVEL 1
#define TITLE "Blitz-SDL"
#define PLANE_INC 1
#define BOMB_INC 1
#define DELAY 10
#define VOPTIONS SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_HWPALETTE

typedef struct {
	Sint16 x, y;
	int frames_between_move;
	int frames_to_next_move;
} position;

char *title;
int bcount, bheight[32], frame_buffer = 0, game_state, key_press, level;
int previous_game_state = -1, score, sound = 1;
int total, x_offset = 0, y_offset = 0, y_rise;
position bomb_position, plane_position;
SDL_AudioSpec audio_spec;
SDL_Surface *build_surf, *sprite_sheet, *sky, *window = NULL;
Sint16 shake_y = 0;
Uint32 audio_length;
Uint8 *ab, *audio_buffer = NULL;

void finish(void)
{
	SDL_Quit();
}

void *get_title(char *path)
{
	title = strrchr(path, '/');
	if (title == NULL)
		title = path;
	else
		title++;
}

void quit(const char *msg1, const char *msg2)
{
	if (msg1 != NULL)
		fprintf(stderr, "%s: %s ", title, msg1);
	if (msg2 != NULL)
		fprintf(stderr, "%s", msg2);
	fprintf(stderr, "\n%s: %s\n", title, SDL_GetError());
	exit(1);
}

void initialise(void)
{
	int status = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	if (status == 0) {
		atexit(finish);
		srand(time(NULL));
	} else
		quit("Error in function", __func__);
}

void set_rect(SDL_Rect *rect, Sint16 x, Sint16 y, Uint16 w, Uint16 h)
{
	rect->x = x;
	rect->y = y;
	rect->w = w;
	rect->h = h;
}

void surface_blit(SDL_Surface *from, Sint16 x, Sint16 y, Uint16 w,
	Uint16 h, SDL_Surface *to, Sint16 x1, Sint16 y1, int not_main_surface)
{
	SDL_Rect from_rect, to_rect;

	/* Don't draw outside borders. This is for framebuffer with centred game window. */
	if (not_main_surface == 0) {
		if (y1 + h >= HEIGHT)
			h = HEIGHT - y1;
		if (x1 + w >= WIDTH)
			w = WIDTH - x1;
		else if (x1 < 0) {
			w += x1;
			x += -x1;
			x1 = 0;
		}
	}

	set_rect(&from_rect, x, y, w, h);
	set_rect(&to_rect, x_offset + x1, y_offset + y1, 0, 0);

	SDL_BlitSurface(from, &from_rect, to, &to_rect);
}

SDL_Surface *try_modes(void)
{
	SDL_Surface *s;
	SDL_Rect **modes;

	modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
	if (modes != NULL && modes != (SDL_Rect **)-1) {
		int i;

		for (i = 0; modes[i]; i++);
		for (--i; i >= 0; i--) {
			if (modes[i]->w >= WIDTH && modes[i]->h >= HEIGHT) {
				s = SDL_SetVideoMode(modes[i]->w, modes[i]->h, 0, VOPTIONS);
				if (s != NULL) {
					x_offset = modes[i]->w / 2 - WIDTH / 2;
					y_offset = modes[i]->h / 2 - HEIGHT / 2;

					return s;
				}
			}
		}
	}

	return NULL;
}

void create_main_window(void)
{
	window = SDL_SetVideoMode(WIDTH, HEIGHT, 0, VOPTIONS);
	if (window == NULL)
		window = try_modes();
	if (window == NULL)
		quit("Error in function", __func__);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(TITLE, TITLE);
}

unsigned char get_transparency(SDL_Surface *surface)
{
	unsigned char t;

	if (SDL_MUSTLOCK(surface) == 1)
		SDL_LockSurface(surface);
	t = *(unsigned char *)surface->pixels;
	if (SDL_MUSTLOCK(surface) == 1)
		SDL_UnlockSurface(surface);

	return t;
}

void load_sprite_sheet(char *path)
{
	unsigned char transparent;
	SDL_Surface *temp;

	temp = SDL_LoadBMP(path);
	if (temp == NULL)
		quit("Cannot open", path);

	transparent = get_transparency(temp);
	SDL_SetColorKey(temp, SDL_SRCCOLORKEY | SDL_RLEACCEL, transparent);
	sprite_sheet = SDL_DisplayFormat(temp);
	if (sprite_sheet == NULL)
		sprite_sheet = temp;
	else
		SDL_FreeSurface(temp);
}

void set_palette(SDL_Surface *surface)
{
	int i;
	SDL_Color colour[256];
	
	for (i = 0; i < sprite_sheet->format->palette->ncolors; i++)
		colour[i] = sprite_sheet->format->palette->colors[i];
	
	SDL_SetColors(surface, colour, 0, sprite_sheet->format->palette->ncolors);
}

void set_icon(void)
{
	SDL_Surface *icon = SDL_CreateRGBSurface(SDL_HWSURFACE, 32, 32,
		sprite_sheet->format->BitsPerPixel,
		sprite_sheet->format->Rmask, sprite_sheet->format->Gmask,
		sprite_sheet->format->Bmask, sprite_sheet->format->Amask);

	if (icon == NULL)
		quit("Error in function", __func__);
	if (sprite_sheet->format->BitsPerPixel == 8)
		set_palette(icon);
	surface_blit(sprite_sheet, 600, 0, 32, 32, icon, -x_offset, -y_offset, 1);
	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
}

void create_building_surface(void)
{
	SDL_Surface *temp = SDL_CreateRGBSurface(SDL_HWSURFACE,
		WIDTH, HEIGHT, sprite_sheet->format->BitsPerPixel,
		sprite_sheet->format->Rmask, sprite_sheet->format->Gmask,
		sprite_sheet->format->Bmask, sprite_sheet->format->Amask);

	if (temp == NULL)
		quit("Error in function", __func__);
	SDL_SetColorKey(temp, SDL_SRCCOLORKEY | SDL_RLEACCEL,
		SDL_MapRGB(temp->format, 0, 0, 0));
	if (sprite_sheet->format->BitsPerPixel == 8)
		set_palette(temp);
	build_surf = SDL_DisplayFormat(temp);
	if (build_surf == NULL)
		build_surf = temp;
	else
		SDL_FreeSurface(temp);
}

void show_instructions(void)
{
	SDL_FillRect(build_surf, NULL, SDL_MapRGB(build_surf->format, 0, 0, 0));
	/* Show game title. */
	surface_blit(sprite_sheet, 237, 327, 353, 95, window, 143, 0, 0);
	/* Show instructions. */
	surface_blit(sprite_sheet, 0, 327, 236, 87, window, 202, 197, 0);
	/* Show copyright. */
	surface_blit(sprite_sheet, 244, 422, 388, 55, window, 130, 420, 0);
}

void pause(void)
{
	if (game_state != GAME_PAUSE) {
		if (game_state == GAME_PLAY || game_state == GAME_STRIKE) {
			previous_game_state = game_state;
			game_state = GAME_PAUSE;
		}
	} else
		game_state = previous_game_state;
}

void escape_key(void)
{
	if (game_state == GAME_START)
		game_state = GAME_QUIT;
	else
		game_state = GAME_START;
}

void mute(void)
{
	sound ^= 1;
	if (sound == 0)
		SDL_PauseAudio(1);
}

void key_down(int key)
{
	if (key == SDLK_ESCAPE)
		escape_key();
	else if (key == SDLK_s)
		mute();
	else if (key == SDLK_p)
		pause();
	else if (game_state == GAME_LOSE) {
		previous_game_state = game_state;
		game_state = GAME_START;
	} else if (game_state == GAME_START) {
		previous_game_state = game_state;
		game_state = GAME_PREPLAY;
	} else if (game_state == GAME_WIN) {
		previous_game_state = game_state;
		level++;
		game_state = GAME_PREPLAY;
	} else
		key_press = 1;
}

void get_events(void)
{
	SDL_Event event;

	while ((SDL_PollEvent(&event))) {
		switch (event.type) {
			case SDL_KEYDOWN:
				key_down(event.key.keysym.sym);
				break;
			case SDL_QUIT:
				game_state = GAME_QUIT;
				break;
		}
	}
}

void draw_buildings(void)
{
	int i, tallest = HEIGHT;

	for (i = 0; i < 32; i++) {
		int c, j, height;
		Sint16 x, x1, y1;

		c = rand() % 4;
		x = 328 + STOREY_WIDTH * c;
		x1 = STOREY_WIDTH * i;
		height = 1 + (rand() % 3 - rand() % 6) + 2 * level + 6;
		for (j = 0; j < height; j++) {
			y1 = HEIGHT - STOREY_HEIGHT - j * STOREY_HEIGHT;
			surface_blit(sprite_sheet, x, 99, STOREY_WIDTH, STOREY_HEIGHT,
				build_surf, x1 - x_offset, y1 - y_offset, 1);
		}
		bheight[i] = j;
		if (y1 < tallest)
			tallest = y1;
	}
	if (level == 1)
		y_rise = HEIGHT - tallest;
	else
		y_rise = 0;
}

void check_for_crash(void)
{
	int col = (plane_position.x + 37) / STOREY_WIDTH;
	if (col >= 0 && col < 32)
		if (plane_position.y > HEIGHT - bheight[col] * STOREY_HEIGHT - 40)
			game_state = GAME_LOSE;
}

void move_plane(void)
{
	if (plane_position.frames_to_next_move == 0) {
		plane_position.x += PLANE_INC;
		if (plane_position.x > WIDTH) {
			plane_position.x = -67;
			plane_position.y += 10;
		}
		plane_position.frames_to_next_move =
			plane_position.frames_between_move - 1;
	} else
		plane_position.frames_to_next_move--;

	check_for_crash();
}

void blit_plane(void)
{
	Sint16 src_x = 528;

	if (game_state != GAME_PAUSE)
		src_x = 200;

	surface_blit(sprite_sheet, src_x, 0, 67, 40, window, plane_position.x, plane_position.y, 0);
}

void blit_moon(void)
{
	Sint16 src_x = 403;

	if (rand() > RAND_MAX / 50 && game_state != GAME_PAUSE)
		src_x = 328;

	surface_blit(sprite_sheet, src_x, 0, 75, 99, window, 555, 10, 0);
}

void remove_storey(int col)
{
	SDL_Rect destination;

	set_rect(&destination, col * STOREY_WIDTH, HEIGHT - STOREY_HEIGHT * bheight[col], STOREY_WIDTH, STOREY_HEIGHT);

	SDL_FillRect(build_surf, &destination, SDL_MapRGB(build_surf->format, 0, 0, 0));
}

void move_explosion(void)
{
	static int countdown = 10;

	countdown--;
	shake_y = countdown % 3;
	if (countdown == 0) {
		shake_y = 0;
		bomb_position.y = -30;
		game_state = GAME_PLAY;
		countdown = 10;
		key_press = 0;
	}
}

void blit_explosion(void)
{
	surface_blit(sprite_sheet, 0, 32, 128, 120, window, bomb_position.x - 50, bomb_position.y - 30, 0);
}

int check_for_strike(void)
{
	int col = bomb_position.x / STOREY_WIDTH;

	if (bomb_position.y > HEIGHT - bheight[col] * STOREY_HEIGHT - 30) {
		if (bheight[col] != 0) {
			game_state = GAME_STRIKE;
			remove_storey(col);
			if (audio_buffer != NULL && sound == 1) {
				ab = audio_buffer;
				SDL_PauseAudio(0);
			}
			score += SCORE_INCREMENT;
			bheight[col]--;
			if (bheight[col] == 0)
				bcount--;
			if (bcount == 0)
				game_state = GAME_WIN;
		}
		return 1;
	}

	return 0;
}

void move_bomb(void)
{
	if (key_press != 0 && bomb_position.y == -30) {
		int col = plane_position.x / STOREY_WIDTH;
		if (col >= 0 && col < 32) {
			bomb_position.x = col * STOREY_WIDTH;
			bomb_position.y = plane_position.y + 30;
		}
	} else if (bomb_position.y != -30) {
		bomb_position.y += BOMB_INC;
		check_for_strike();
	}

	if (bomb_position.y > HEIGHT) {
		bomb_position.y = -30;
		key_press = 0;
	}
}

void blit_bomb(void)
{
	if (bomb_position.y > 0)
		surface_blit(sprite_sheet, 267, 0, 20, 30, window, bomb_position.x, bomb_position.y, 0);
}

void blit_score(void)
{
	int i, r, dst_x = 70;
	int digit_x[10] = { 67, 80, 93, 106, 119, 132, 145, 158, 171, 184 };
	SDL_Rect rect;

	if (score - total > SCORE_INCREMENT)
		total = score;
	else if (total < score)
		total++;
	for (i = total; i > 0; i /= 10)
		dst_x += 11;
	i = total;
	surface_blit(sprite_sheet, 0, 0, 64, 32, window, 10, 10, 0);
	for (r = i % 10; i > 0; i /= 10, r = i % 10) {
		surface_blit(sprite_sheet, digit_x[r], 0, 11, 32,
			window, dst_x, 10, 0);
		dst_x -= 11;
	}
}

void reset_plane(void)
{
	plane_position.x = 0;
	plane_position.y = 10;
	plane_position.frames_between_move = 2;
	plane_position.frames_to_next_move =
		plane_position.frames_between_move - 1;
}

void reset_bomb(void)
{
	bomb_position.x = 0;
	bomb_position.y = -30;
}

void reset_game(void)
{
	bcount = 32;
	key_press = 0;
	level = START_LEVEL;
	previous_game_state = GAME_START;
	score = 0;
	total = -1;	
	reset_plane();
	reset_bomb();	
}

void game_start(void)
{
	if (previous_game_state != GAME_START)
		reset_game();
	show_instructions();	
}

void game_preplay(void)
{
	reset_plane();
	reset_bomb();
	draw_buildings();
	bcount = 32;
	key_press = 0;
	shake_y = 0;
	previous_game_state = GAME_PREPLAY;
	game_state = GAME_PLAY;
}

void blit_buildings(void)
{
	SDL_Rect rect, brect;

	set_rect(&rect, x_offset, y_offset - shake_y + y_rise, 0, 0);
	set_rect(&brect, 0, 0, WIDTH, HEIGHT - y_rise);
	SDL_BlitSurface(build_surf, &brect, window, &rect);
	if (y_rise > 0)
		y_rise -= 5;
}

void game_play(void)
{
	blit_moon();
	blit_score();
	blit_buildings();
	blit_plane();
	blit_bomb();
	move_plane();
	move_bomb();
}

void game_pause(void)
{
	blit_moon();
	blit_score();
	blit_buildings();
	blit_plane();
	if (previous_game_state == GAME_STRIKE)
		blit_explosion();
	else
		blit_bomb();
	surface_blit(sprite_sheet, 478, 0, 50, 50, window, 295, 220, 0);
}

void game_strike(void)
{
	blit_moon();
	blit_score();
	blit_buildings();
	blit_plane();
	blit_explosion();
	move_plane();
	move_explosion();
}

void game_lose(void)
{
	blit_moon();
	blit_score();
	blit_buildings();
	surface_blit(sprite_sheet, 0, 32, 128, 120, window,
		plane_position.x - 25, plane_position.y - 15, 0);
	surface_blit(sprite_sheet, 0, 153, 559, 86, window, 41, 197, 0);
}

void game_win(void)
{
	blit_moon();
	blit_score();
	blit_buildings();
	surface_blit(sprite_sheet, 0, 239, 548, 88, window, 46, 196, 0);
}

void action(void)
{
	switch(game_state) {
		case GAME_START:
			game_start();
			return;
		case GAME_PREPLAY:
			game_preplay();   
			break;
		case GAME_PLAY:
			game_play();
			break;
		case GAME_PAUSE:
			game_pause();
			break;
		case GAME_STRIKE:
			game_strike();
			break;
		case GAME_LOSE:
			game_lose();
			break;
		case GAME_WIN:
			game_win();
			break;
	}
}

void play(void)
{
	Uint32 inverse_fr = DELAY, next_time = 0, now;

	game_state = GAME_START;
	while (game_state) {
		surface_blit(sprite_sheet, 0, HEIGHT, WIDTH, HEIGHT, window, 0, 0, 0);
		action();
		get_events();
		SDL_Flip(window);
		if ((now = SDL_GetTicks()) < next_time)
			SDL_Delay(next_time - now);
		next_time = SDL_GetTicks() + DELAY;
	}
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{
	int i;
	static int total;

	if (ab == audio_buffer)
		total = 0;
	if (audio_length - total < len)
		len = audio_length - total;
	if (len == 0) {
		SDL_PauseAudio(1);
		return;
	}
	for (i = 0; i < len; i++)
		*stream++ = *ab++;
	total += len;
}

void load_audio(void)
{
	char *explosion_path = DATADIR "/explosion.wav";
	SDL_AudioSpec *status;

	status = SDL_LoadWAV(explosion_path, &audio_spec,
		&audio_buffer, &audio_length);
	if (status == NULL) {
		fprintf(stderr, "%s: Cannot open %s\n", title, explosion_path);
		fprintf(stderr, "%s: %s\n", title, SDL_GetError());
		
		return;
	}

	audio_spec.callback = audio_callback;
	if (SDL_OpenAudio(&audio_spec, NULL) < 0) {
		fprintf(stderr, "%s: Cannot open audio device.\n", title);
		fprintf(stderr, "%s: %s\n", title, SDL_GetError());
		if (audio_buffer != NULL)
			SDL_FreeWAV(audio_buffer);
		audio_buffer = NULL;

		return;
	}
}

int main(int argc, char *argv[])
{
	char *sprite_sheet_path = DATADIR "/spritesheet.bmp";
	char *background_path = DATADIR "/background.bmp";

	get_title(argv[0]);
	initialise();
	load_sprite_sheet(sprite_sheet_path);
	set_icon();
	create_building_surface();
	load_audio();
	create_main_window();	
	play();

	return 0;
}

