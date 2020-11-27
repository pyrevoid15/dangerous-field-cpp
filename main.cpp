// SDL_Test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <random>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>


inline bool RectXPoint(SDL_Rect* r, SDL_Point* p)
{
	return p->x >= r->x && p->x < r->x + r->w && p->y >= r->y && p->y < r->y + r->h;
}

inline bool RectXRect(SDL_Rect* r1, SDL_Rect* r2)
{
	return SDL_HasIntersection(r1, r2);
}

inline double getDistance(SDL_Point p1, SDL_Point p2) {
	return std::sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

struct Map {
	int w, h;
	int tile_s;
	int tiles[350][350];
	//std::vector<int> temperatures;
};

struct Player {
	SDL_Surface* image;
	SDL_Rect pr, hr;
	double pos[2];
	double vel[2];
	int g_pos[2];
	double hp, energy, running, friction;
	int ghostlvl;
	bool mov_X[2];
	bool mov_Y[2];
	//int mouspos[2];
};

struct Tumbleweed {
	SDL_Surface* image;
	SDL_Rect rect;
	double pos[2];
	int g_pos[2];
	double vel[2], speed;
	double angle;
};

struct Chaser {
	SDL_Surface* image;
	SDL_Rect rect;
	double pos[2];
	int g_pos[2];
	double vel[2];
	double goal[2];
	static constexpr double speed = 0.65;
	double angle;
	char mode;
};

struct TChaser : public Chaser {
	int about_to_teleport;
};

struct Arrow {
	SDL_Surface* image;
	SDL_Rect rect;
	double pos[2];
	double vel[2];
	double angle;
	int mode;

	static std::vector<Arrow> all_arrows;
};

struct Archer {
	SDL_Surface* image;
	SDL_Rect rect;
	double pos[2], goal[2];
	int g_pos[2], mode;
	double vel[2];
	double angle, f_angle;
	bool crazy;
};

struct PlayerStatsGUI {
	SDL_Surface* hbar_i;
	SDL_Surface* ebar_i;
	SDL_Rect hbar_r, ebar_r, blit_r;
	Uint32 hp_clr, ene_clr;
	char hp_T, ene_T;
	static constexpr int bar_len = 80;
};

struct Text {
	TTF_Font* font;
	SDL_Surface* text;
	SDL_Rect rect;
	SDL_Color color;
	const char* message;
};

struct Button {
	SDL_Surface* image;
	int mode;
	SDL_Rect rect;
};

SDL_Window* win;
SDL_Surface* scr;
Map world;
bool is_running = true, has_started = false, has_started_before = false, resetting = false;
SDL_Point drivf = { 0, 0 };
int camera_offset[] = { 0, 0 };
Uint32 colors[] = {
		0xF0000000, 0xF000C800, 0xF0009614, 0xF0149614,
		0xF00AC800, 0xA000CCFF, 0xA02222FF, 0xA00010CC };
std::mersenne_twister_engine<std::uint_fast32_t, 32, 624, 397, 31,
	0x9908b0df, 11,
	0xffffffff, 7,
	0x9d2c5680, 15,
	0xefc60000, 18, 1812433253> rng;

int ticks_A, ticks_B, wait_A = 2;

void do_tick(int* ctr, bool* stat) {
	int wait_B = 2;
	std::chrono::duration<double> tween_B;
	do {
		tween_B.zero();
		auto start = std::chrono::steady_clock::now();
		SDL_Delay(wait_B);
		
		if (tween_B.count() < 0.009)
			++wait_B;
		else if (tween_B.count() > 0.012 && wait_B > 1)
			--wait_B;
		
		auto end = std::chrono::steady_clock::now();
		tween_B = end - start;
		//std::cout << tween_B.count() << '\n';

		++(*(int*)(ctr));
	} while (*stat);
}

void CreateText(Text* txt, SDL_Rect* rect, SDL_Color* clr, const char* text, int flags) {
	if (clr != NULL)
		txt->color = *clr;
	if (text != NULL)
		txt->message = text;
	if (flags != NULL)
		TTF_SetFontStyle(txt->font, flags);
	if (rect != NULL) {
		txt->font = TTF_OpenFont("8bfont.ttf", (*rect).h);
		txt->rect = { (*rect).x, (*rect).y, (*rect).w, (*rect).h };
	}

	SDL_assert(&txt->color != NULL && &txt->message != NULL &&
		txt->font != NULL && &txt->rect != NULL);
	txt->text = TTF_RenderText_Solid(txt->font, text, txt->color);
}

void CreateButton(Button* btn, SDL_Surface* img, SDL_Rect* rect) {
	btn->image = img;
	btn->rect = *rect;
	btn->mode = 0;
}

bool UpdateButton(Button* btn) {
	if (SDL_PointInRect(&drivf, &btn->rect))
		return true;
	else
		return false;
}

void manual(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0xFFC0FDEC);
	Text txt;
	SDL_Rect placement = { 50, 50, 600, 20 };
	SDL_Color color = { 255, 255, 255 };
	CreateText(&txt, &placement, &color, "- Press ARROW keys to move.", NULL);
	SDL_FillRect(screen, NULL, 0xFF000000);
	SDL_BlitSurface(txt.text, NULL, screen, &txt.rect);

	placement = { 50, 80, 600, 20 };
	color = { 255, 255, 255 };
	CreateText(&txt, &placement, &color, "- Press SHIFT to dash.", NULL);
	SDL_BlitSurface(txt.text, NULL, screen, &txt.rect);

	placement = { 50, 110, 600, 20 };
	color = { 255, 255, 255 };
	CreateText(&txt, &placement, &color, "- Press ESCAPE to pause.", NULL);
	SDL_BlitSurface(txt.text, NULL, screen, &txt.rect);

	placement = { 50, 140, 600, 20 };
	color = { 255, 255, 255 };
	CreateText(&txt, &placement, &color, "- Press Q to leave.", NULL);
	SDL_BlitSurface(txt.text, NULL, screen, &txt.rect);

	SDL_UpdateWindowSurface(win);

	SDL_Event e;
	bool helping = true;
	while (helping) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				helping = false;
				is_running = false;
			}
			else if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_q)
					helping = false;
			}
		}
	}

	SDL_FreeSurface(txt.text);
}

void pause(SDL_Surface* screen) {
	
	SDL_FillRect(screen, NULL, 0x96001E00);

	SDL_Rect psbar;
	psbar.x = 328;
	psbar.y = 315;
	psbar.w = 12;
	psbar.h = 55;

	
	SDL_FillRect(screen, &psbar, 0xFFFFFFFF);

	psbar.x = 360;
	SDL_FillRect(screen, &psbar, 0xFFFFFFFF);
	SDL_UpdateWindowSurface(win);

	bool paused = true;
	SDL_Event e;
	do {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				paused = false;
				is_running = false;
			}
			else if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_ESCAPE)
					paused = false;
				else if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL) {
					resetting = true;
					paused = false;
				}
			}
		}
		SDL_Delay(100);
	} while (paused);
}

void CreatePlayer(Player* ply) {
	ply->image = SDL_CreateRGBSurface(0, 40, 40, 32, 0, 0, 0, 0);
	SDL_FillRect(ply->image, NULL, SDL_MapRGB(ply->image->format, 0, 100, 100));

	ply->pr.x = 350;
	ply->pr.y = 350;
	ply->pr.w = 40;
	ply->pr.h = 40;
	ply->hr.x = 357;
	ply->hr.y = 357;
	ply->hr.w = 26;
	ply->hr.h = 26;
	ply->mov_X[0] = false;
	ply->mov_X[1] = false;
	ply->mov_Y[0] = false;
	ply->mov_Y[1] = false;
	ply->pos[0] = 350;
	ply->pos[1] = 350;
	ply->running = 1;
	ply->energy = 69;
	ply->friction = 1;
	ply->hp = 100;
	ply->ghostlvl = 30;
	//ply.mouspos[0] =
}

void pGUISetUp(PlayerStatsGUI* psg) {
	psg->hbar_i = SDL_CreateRGBSurface(0, 100, 10, 32, 0, 0, 0, 0);
	psg->ebar_i = SDL_CreateRGBSurface(0, 100, 10, 32, 0, 0, 0, 0);
	psg->hbar_r.w = 98;
	psg->hbar_r.h = 8;
	psg->hbar_r.x = 1;
	psg->hbar_r.y = 1;
	psg->ebar_r.w = 98;
	psg->ebar_r.h = 8;
	psg->ebar_r.x = 1;
	psg->ebar_r.y = 1;
	psg->blit_r.x = 8;
	psg->blit_r.y = 8;
	psg->blit_r.w = 100;
	psg->blit_r.h = 12;
	psg->hp_clr = SDL_MapRGB(psg->hbar_i->format, 10, 255, 50);
	psg->hp_T = 'g';
	psg->ene_clr = SDL_MapRGB(psg->hbar_i->format, 0, 255, 255);
	psg->ene_T = 'g';

	SDL_FillRect(psg->hbar_i, &(psg->hbar_r), psg->hp_clr);
	SDL_FillRect(psg->ebar_i, &(psg->ebar_r), psg->ene_clr);
}

void pGUIUpdate(PlayerStatsGUI* psg, Player& ply) {
	psg->hbar_r.w = int(98 * ply.hp / 100);
	psg->ebar_r.w = int(98 * ply.energy / 70);
	SDL_FillRect(psg->hbar_i, NULL, 0x00000000);
	SDL_FillRect(psg->ebar_i, NULL, 0x00000000);

	if (ply.hp < 33.3 && psg->hp_T != 'r') {
		psg->hp_T = 'r';
		psg->hp_clr = SDL_MapRGB(psg->hbar_i->format, 200, 50, 50);
	}
	else if (33.3 < ply.hp && ply.hp < 66.7 && psg->hp_T != 'y') {
		psg->hp_T = 'y';
		psg->hp_clr = SDL_MapRGB(psg->hbar_i->format, 150, 150, 50);
	}
	else if (ply.hp > 66.7 && psg->hp_T != 'g') {
		psg->hp_T = 'g';
		psg->hp_clr = SDL_MapRGB(psg->hbar_i->format, 10, 255, 50);
	}

	if (ply.ghostlvl % 3 == 1)
		SDL_FillRect(psg->hbar_i, &(psg->hbar_r), SDL_MapRGB(psg->hbar_i->format, 255, 255, 255));
	else
		SDL_FillRect(psg->hbar_i, &(psg->hbar_r), psg->hp_clr);

	if (ply.energy < 10 && psg->ene_T != 'b') {
		psg->ene_T = 'b';
		psg->ene_clr = SDL_MapRGB(psg->ebar_i->format, 50, 50, 50);
	}
	else if (10 < ply.energy && ply.energy < 66 && psg->ene_T != 'o') {
		psg->ene_T = 'o';
		psg->ene_clr = SDL_MapRGB(psg->ebar_i->format, 20, 20, 200);
	}
	else if (ply.energy > 66 && psg->ene_T != 'g') {
		psg->ene_T = 'g';
		psg->ene_clr = SDL_MapRGB(psg->ebar_i->format, 0, 255, 255);
	}

	SDL_FillRect(psg->ebar_i, &(psg->ebar_r), psg->ene_clr);
}

void GenerateMap(Map* map, int seed) {
	map->w = 350;
	map->h = 350;
	map->tile_s = 50;
	rng.seed(seed);

	int x, y;
	int i, k, j, d;

	for (y = 0; y < map->h; y++) {
		for (x = 0; x < map->w; x++)
			map->tiles[y][x] = rng() % 4 + 1;
	}

	std::vector<SDL_Point> seeds, barriers;
	double rays[5][2];
	double wetness = 0.5, m;
	SDL_Point p;

	k = rng() % 8 + 5;
	for (i = 0; i < k * (1 - wetness) + 3; i++)
		barriers.push_back({ (signed int)rng() % map->w, (signed int)rng() % map->h });
	barriers.push_back({5, 5});
		
	for (i = 0; i < k * wetness * 2 + 1; i++)
		seeds.push_back({ (signed int)rng() % map->w, (signed int)rng() % map->h });

	for (SDL_Point& s : seeds) {
		for (i = 0; i < 5; i++) {
			rays[i][0] = (rng() % map->w) * (rng() % 16) * wetness;
			rays[i][1] = (rng() % map->h) * (rng() % 16) * wetness;
			m = std::tan(rays[i][1] / rays[i][0]) * (rng() % 64 < 32 ? -1 : 1);

			for (x = 0; x < abs(rays[i][0]); rays[i][0] < 0 ? x-- : x++) {
				if (x + s.x >= 0 && x + s.x < map->w) {
					y = int(x * m);
					if (y + s.y >= 0 && y + s.y < map->h) {
						d = rng() % int(51 * wetness) + 30;
						for (j = 0; j < 80 * wetness * d / 2; j++) {
							p = { x + s.x + int(rng() % d),  y + s.y + int(rng() % d) };
							if (p.x >= 0 && p.x < map->w &&
								p.y >= 0 && p.y < map->h)
								map->tiles[p.y][p.x] = 5;// + int(1.5 / std::sqrt(pow(p.x - x - s.x, 2) + pow(p.y - y - s.y, 2)));
						}
					}else break;
				}
				else break;
			}
		}
	}

	for (SDL_Point& b : barriers) {
		for (i = 0; i < 4; i++) {
			rays[i][0] = (rng() % map->w) * (1 - wetness);
			rays[i][1] = (rng() % map->h) * (1 - wetness);
			m = std::tan(rays[i][1] / rays[i][0]) * (rng() % 64 < 32 ? -1 : 1);

			for (x = 0; x < abs(rays[i][0]); rays[i][0] < 0 ? x-- : x++) {
				if (x + b.x >= 0 && x + b.x < map->w) {
					y = int(x * m);
					if (y + b.y >= 0 && y + b.y < map->h) {
						d = rng() % int(30 * wetness) + 30;
						for (j = 0; j < 80 * wetness * d / 2; j++) {
							p = { x + b.x + int(rng() % d),  y + b.y + int(rng() % d) };
							if (p.x >= 0 && p.x < map->w &&
								p.y >= 0 && p.y < map->h)
								map->tiles[p.y][p.x] = rng() % 4 + 1;
						}
					}
					else break;
				}
				else break;
			}
		}
	}

	for (y = 0; y < map->h; y++) {
		for (x = 0; x < map->w; x++) {
			if (map->tiles[y][x] < 0)
				map->tiles[y][x] = 0;
			else if (map->tiles[y][x] > 7)
				map->tiles[y][x] = 7;
		}
	}
	rng.seed(std::chrono::steady_clock::now().time_since_epoch().count() % INT64_MIN);
}

void RenderMap(Map* map, SDL_Surface* screen) {

	SDL_Rect tR;
	tR.w = map->tile_s;
	tR.h = map->tile_s;
	tR.x = camera_offset[0] % map->tile_s * -1;
	tR.y = camera_offset[1] % map->tile_s * -1;

	for (int y = 0; y < map->h; y++) {
		tR.y = y * map->tile_s - camera_offset[1];
		for (int x = 0; x < map->w; x++) {
			tR.x = x * map->tile_s - camera_offset[0];

			if (tR.x > -map->tile_s && tR.y > -map->tile_s) {
				if (tR.x < 700 && tR.y < 700) {
					SDL_FillRect(screen, &tR, colors[map->tiles[y][x]]);
				}
				else break;
			}
		}
	}
}

void CreateTumbleweed(Tumbleweed* t) {
	t->image = SDL_CreateRGBSurface(0, 35, 35, 32, 0, 0, 0, 0);
	SDL_FillRect(t->image, NULL, SDL_MapRGB(t->image->format, 200, 200, 0));
	t->pos[0] = 100;
	t->pos[1] = 100;
	t->rect.x = t->pos[0];
	t->rect.y = t->pos[1];
	t->rect.w = 35;
	t->rect.h = 35;
	t->angle = ((rng() % 512) / 512.0 * 360 - 180) / 180 * std::_Pi;
	t->speed = (rng() % 20 + 80.0) / 10.0;
	t->vel[0] = std::cos(t->angle) * t->speed;
	t->vel[1] = std::sin(t->angle) * t->speed;
}

void UpdateTumbleweed(Tumbleweed* t) {
	t->speed -= 0.00005;
	t->pos[0] += t->vel[0] / 2;
	t->pos[1] += t->vel[1] / 2;
	t->rect.x = int(t->pos[0]) - camera_offset[0];
	t->rect.y = int(t->pos[1]) - camera_offset[1];

	if (t->pos[0] < 0 || t->pos[0] >= (world.tile_s * world.w - t->rect.w)) {
		t->vel[0] *= -1;
		t->angle = std::atan2(t->vel[1], t->vel[0]);
	}
	if (0 > t->pos[1] || t->pos[1] > (world.tile_s * world.h - t->rect.h)) {
		t->vel[1] *= -1;
		t->angle = std::atan2(t->vel[1], t->vel[0]);
	}

	t->vel[0] = std::cos(t->angle) * t->speed;
	t->vel[1] = std::sin(t->angle) * t->speed;
	t->pos[0] += t->vel[0] / 2;
	t->pos[1] += t->vel[1] / 2;
	t->g_pos[0] = int((t->pos[0] + t->rect.w / 2) / world.tile_s);
	t->g_pos[1] = int((t->pos[1] + t->rect.h) / world.tile_s);
}

void CreatePChaser(Chaser* pc) {
	pc->image = SDL_CreateRGBSurface(0, 60, 60, 32, 0, 0, 0, 0);
	SDL_FillRect(pc->image, NULL, 0xFF141414);
	pc->angle = 0;
	pc->mode = 'i';
	pc->pos[0] = (signed)(rng() % 700) + 350.0;
	pc->pos[1] = (signed)(rng() % 700) + 350.0;
	pc->vel[0] = 0;
	pc->vel[1] = 0;
	pc->rect = { int(pc->pos[0] - camera_offset[0]),
				int(pc->pos[0] - camera_offset[0]),
				60, 60 };
}

void UpdatePChaser(Chaser* pc, Player* ply) {
	double dist = getDistance({ int(pc->pos[0]), int(pc->pos[1]) }, { int(ply->pos[0]), int(ply->pos[1]) });

	if (pc->mode == 'i') { // if idle
		if (dist < 2000) {
			pc->mode = 'd';
			pc->goal[0] = (ply->pos[0] - pc->pos[0]) * 1.8 + ply->pos[0] + rng() % 400 - 200;
			pc->goal[1] = (ply->pos[1] - pc->pos[1]) * 1.7 + ply->pos[1] + rng() % 400 - 200;
		}
		else if (dist > 3500);
	}
	else if (pc->mode == 'c') { // if chasing
		pc->goal[0] = ply->pos[0];
		pc->goal[1] = ply->pos[1];
		pc->angle = std::atan2(ply->pos[1] - pc->pos[1], ply->pos[0] - pc->pos[0]);

		pc->pos[0] += std::cos(pc->angle) * Chaser::speed + (pc->goal[0] - pc->pos[0]) / 350.0;
		pc->pos[1] += std::cos(pc->angle) * Chaser::speed + (pc->goal[1] - pc->pos[1]) / 350.0;

		if (ticks_B % 210 < 10 && rng() % 100 < 30) {
			pc->mode = 'd';

			pc->goal[0] = (ply->pos[0] - pc->pos[0]) * (rng() % 6 + 28) / 10 + ply->pos[0] + ply->vel[0] * 20;																																																																																																																2.4 + ply->pos[0] + (rng() % 400) - 200;
			pc->goal[1] = (ply->pos[1] - pc->pos[1]) * (rng() % 6 + 28) / 10 + ply->pos[1] + ply->vel[1] * 20;
		}
		else if (ticks_B % 100 == 0 && dist > 2050)
			pc->mode = 'i';
	}
	else { //if charging
		pc->pos[0] += (pc->goal[0] - pc->pos[0]) / 170.0;
		pc->pos[1] += (pc->goal[1] - pc->pos[1]) / 170.0;

		if (ticks_B % 300 < 2)
			pc->mode = 'c';
		else if (ticks_B % 20 == 1) {
			pc->goal[0] = (ply->pos[0] - pc->pos[0]) * (rng() % 6 + 28) / 10 + ply->pos[0] + ply->vel[0] * 20;																																																																																																																2.4 + ply->pos[0] + (rng() % 400) - 200;
			pc->goal[1] = (ply->pos[1] - pc->pos[1]) * (rng() % 6 + 28) / 10 + ply->pos[1] + ply->vel[1] * 20;
		}
	}

	pc->rect.x = int(pc->pos[0] - camera_offset[0]);
	pc->rect.y = int(pc->pos[1] - camera_offset[1]);
	pc->g_pos[0] = int((pc->pos[0] + pc->rect.w / 2) / world.tile_s);
	pc->g_pos[1] = int((pc->pos[1] + pc->rect.h) / world.tile_s);
}

void CreateTChaser(TChaser* tc) {
	CreatePChaser(tc);
	SDL_FillRect(tc->image, NULL, 0xFF1D1D96);
	tc->about_to_teleport = 0;
}

void UpdateTChaser(TChaser* tc, Player* ply) {
	double dist = getDistance({ int(tc->pos[0]), int(tc->pos[1]) }, { int(ply->pos[0]), int(ply->pos[1]) });

	if (tc->mode == 'i') { // if idle
		if (dist < (world.w + world.h) / 5 * world.tile_s) {
			tc->mode = 'd';
			tc->goal[0] = (ply->pos[0] - tc->pos[0]) * 1.8 + ply->pos[0] + rng() % 400 - 200;
			tc->goal[1] = (ply->pos[1] - tc->pos[1]) * 1.7 + ply->pos[1] + rng() % 400 - 200;
		}
		else if (dist > 3500);
	}
	else if (tc->mode == 'c') { // if chasing
		tc->goal[0] = ply->pos[0];
		tc->goal[1] = ply->pos[1];
		tc->angle = std::atan2(ply->pos[1] - tc->pos[1], ply->pos[0] - tc->pos[0]);

		tc->pos[0] += std::cos(tc->angle) * Chaser::speed + (tc->goal[0] - tc->pos[0]) / 400.0;
		tc->pos[1] += std::cos(tc->angle) * Chaser::speed + (tc->goal[1] - tc->pos[1]) / 400.0;

		if (ticks_B % 210 < 10 && rng() % 100 < 30) {
			tc->mode = 'd';

			tc->goal[0] = (ply->pos[0] - tc->pos[0]) * 2.4 + ply->pos[0] + rng() % 400 - 200;
			tc->goal[1] = (ply->pos[1] - tc->pos[1]) * 2.2 + ply->pos[1] + rng() % 400 - 200;
		}
		else if (ticks_B % 100 == 0 && dist > (world.w + world.h) / 5 * world.tile_s)
			tc->mode = 'i';

		if (ticks_B % 300 == (rng() % 4) * 100) {
			tc->about_to_teleport = 20;
		}

		if (tc->about_to_teleport == 5) {
			if (rng() % 100 < 60)
				tc->mode = 'd';

			if (rng() % 100 < 30) {
				tc->pos[rng() % 2] += rng() % 100 * (rng() % 100 < 50 ? -1 : 1);

				if (rng() % 100 < 40) {
					tc->pos[0] = (rng() % 700 + 200) * (ply->vel[0] < 0 ? -1 : 1) + ply->pos[0];
					tc->pos[1] = (rng() % 700 + 200) * (ply->vel[1] < 0 ? -1 : 1) + ply->pos[1];
				}
				else {
					if (rng() % 100 < 55)
						tc->pos[0] = (rng() % 1400 + 200) * (rng() % 100 < 50 ? -1 : 1) + ply->pos[0];
					else
						tc->pos[1] = (rng() % 1400 + 200) * (rng() % 100 < 50 ? -1 : 1) + ply->pos[1];
				}
			}
			else {
				if (ply->vel[0] != 0 && rng() % 100 < 80)
					tc->pos[0] = ply->pos[0] + ply->vel[0] * 350;
				else if (ply->vel[1] != 0 && rng() % 100 < 80)
					tc->pos[1] = ply->pos[1] + ply->vel[1] * 350;
				else {
					tc->pos[0] = rng() % (world.w * world.tile_s);
					tc->pos[1] = rng() % (world.h * world.tile_s);
				}
			}

			tc->about_to_teleport = 0;
		}
		else if (tc->about_to_teleport > 5) --tc->about_to_teleport;
		else if (tc->about_to_teleport > 1) --tc->about_to_teleport;
	}
	else { //if charging
		tc->pos[0] += (tc->goal[0] - tc->pos[0]) / 280.0;
		tc->pos[1] += (tc->goal[1] - tc->pos[1]) / 290.0;

		if (ticks_B % 500 < 2 || (dist < 60 && rng() % 200 < 7)) {
			tc->mode = 'c';
			if (rng() % 100 < 25)
				tc->about_to_teleport = 17;
		}
		else if (ticks_B % 70 == 1) {
			tc->goal[0] = (ply->pos[0] - tc->pos[0]) * 2.4 + ply->pos[0] + rng() % 400 - 200;
			tc->goal[1] = (ply->pos[1] - tc->pos[1]) * 2.2 + ply->pos[1] + rng() % 400 - 200;
		}
		if (dist < 100 and rng() % 500 < 2)
			tc->pos[0] += rng() % 100 - 50;
		if (dist < 100 and rng() % 500 < 2)
			tc->pos[0] += rng() % 100 - 50;

	}

	if (-90 > tc->pos[0])
		tc->pos[0] = -50;
	else if (tc->pos[0] > world.tile_s * world.w + 60)
		tc->pos[0] = world.tile_s * world.w + 10;

	if (-50 > tc->pos[1])
		tc->pos[1] = 10;
	else if (tc->pos[1] > world.tile_s * world.h + 60)
		tc->pos[1] = world.tile_s * world.h + 10;

	tc->rect.x = int(tc->pos[0] - camera_offset[0]);
	tc->rect.y = int(tc->pos[1] - camera_offset[1]);
	tc->g_pos[0] = int((tc->pos[0] + tc->rect.w / 2) / world.tile_s);
	tc->g_pos[1] = int((tc->pos[1] + tc->rect.h) / world.tile_s);
	//std::cout << tc->g_pos[0] << " " << tc->g_pos[0] << '\n';
}

void CreateArcher(Archer* ar, bool crazy) {
	ar->image = SDL_CreateRGBSurface(0, 45, 45, 32, 0, 0, 0, 0);
	ar->pos[0] = (signed)(rng() % 700) + 350.0;
	ar->pos[1] = (signed)(rng() % 700) + 350.0;
	ar->vel[0] = 0;
	ar->vel[1] = 0;
	ar->rect = { int(ar->pos[0] - camera_offset[0]),
				int(ar->pos[1] - camera_offset[1]),
				45, 45 };
	ar->angle = 0;
	ar->f_angle = 0;
	ar->mode = 'i';
	ar->crazy = crazy;
}

int main(int args, char* argv[])
{
	std::cout << "---Begin---\n";

	SDL_Init(SDL_INIT_EVERYTHING);
	IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
	TTF_Init();

	SDL_Surface* ico; // = SDL_CreateRGBSurface(0, 32, 32, 32, 0, 0, 0, 0);
	ico = IMG_Load("Images/chaseicon.png");

	win = SDL_CreateWindow("Chase", 200, 200, 700, 700, SDL_WINDOW_SHOWN);
	scr = SDL_GetWindowSurface(win);

	void* param = nullptr;
	ticks_A = 0;
	ticks_B = 0;
	std::thread timer(do_tick, &ticks_B, &is_running);
	int i = 0;
	SDL_Event e;

	Player ply;
	PlayerStatsGUI bars;
	std::vector<Tumbleweed> tumbleweeds;
	std::vector<Chaser> pchasers;
	std::vector<TChaser> tchasers;
	std::vector<Archer> archers;

	Button* help = new Button();
	Text txt;
	SDL_Color color = { 255, 255, 255 };
	SDL_Rect placement = {10, 0, 70, 25 };
	SDL_Surface* goldbutton = IMG_Load("Images/bgnbtn.png");
	
	restart:
	SDL_FillRect(scr, NULL, 0xFF960096);
	CreateText(&txt, &placement, &color, "HELP", NULL);
	SDL_BlitSurface(txt.text, NULL, goldbutton, &placement);
	placement = { 10, 10, 80, 40 };
	SDL_BlitSurface(goldbutton, NULL, scr, &placement);
	CreateButton(help, goldbutton, &placement);

	placement = { 200, 330, 200, 24 };
	color = { 0, 20, 30 };
	CreateText(&txt, &placement, &color, "Press SHIFT to start!", TTF_STYLE_UNDERLINE);
	SDL_BlitSurface(txt.text, NULL, scr, &placement);
	
	SDL_SetWindowIcon(win, ico);
	while (!has_started) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				has_started = true;
				is_running = false;
			}
			else if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_LSHIFT ||
					e.key.keysym.sym == SDLK_RSHIFT)
					has_started = true;
			}
			else if (e.type == SDL_MOUSEMOTION) {
				SDL_GetMouseState(&(drivf.x), &(drivf.y));
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN) {
				if (e.button.button == SDL_BUTTON_LEFT && UpdateButton(help)) {
					manual(scr);

					SDL_FillRect(scr, NULL, 0xFF960096);
					CreateText(&txt, &placement, &color, "HELP", NULL);
					SDL_BlitSurface(txt.text, NULL, goldbutton, &placement);
					placement = { 10, 10, 80, 40 };
					SDL_BlitSurface(goldbutton, NULL, scr, &placement);
					CreateButton(help, goldbutton, &placement);

					placement = { 200, 330, 200, 24 };
					color = { 0, 20, 30 };
					CreateText(&txt, &placement, &color, "Press SHIFT to start!", TTF_STYLE_UNDERLINE);
					SDL_BlitSurface(txt.text, NULL, scr, &placement);
				}
			}
		}
		SDL_Delay(10);
		SDL_UpdateWindowSurface(win);
	}

	SDL_FillRect(scr, NULL, 0x99FEFEFE);
	SDL_UpdateWindowSurface(win);

	if (!has_started_before) {
		for (i = 0; i < 3; i++) {
			Tumbleweed t;
			tumbleweeds.push_back(t);
			CreateTumbleweed(&(tumbleweeds[i]));
		}
		for (i = 0; i < 1; i++) {
			Chaser pc;
			pchasers.push_back(pc);
			CreatePChaser(&(pchasers[i]));
		}
		for (i = 0; i < 2; i++) {
			TChaser tc;
			tchasers.push_back(tc);
			CreateTChaser(&(tchasers[i]));
		}

		GenerateMap(&world, 999);
		CreatePlayer(&ply);
		pGUISetUp(&bars);
	}

	ticks_A = 0;
	ticks_B = 0;
	has_started_before = true;

	while (is_running) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				is_running = false;
			else if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_LEFT) {
					ply.mov_X[0] = true;
					ply.mov_X[1] = false;
				}
				else if (e.key.keysym.sym == SDLK_RIGHT) {
					ply.mov_X[1] = true;
					ply.mov_X[0] = false;
				}
				else if (e.key.keysym.sym == SDLK_UP) {
					ply.mov_Y[1] = false;
					ply.mov_Y[0] = true;
				}
				else if (e.key.keysym.sym == SDLK_DOWN) {
					ply.mov_Y[0] = false;
					ply.mov_Y[1] = true;
				}
				else if ((e.key.keysym.sym == SDLK_LSHIFT ||
						e.key.keysym.sym == SDLK_RSHIFT) &&
					ply.energy > 10) {
					ply.running = 6;
					SDL_FillRect(ply.image, NULL, SDL_MapRGB(ply.image->format, 0, 170, 170));
				}
				else if (e.key.keysym.sym == SDLK_ESCAPE) {
					pause(scr);
					if (resetting) {
						has_started = false;
						resetting = false;
						goto restart;
					}
				}
			}
			else if (e.type == SDL_KEYUP) {
				if (e.key.keysym.sym == SDLK_LEFT) {
					ply.mov_X[0] = false;
				}
				else if (e.key.keysym.sym == SDLK_RIGHT) {
					ply.mov_X[1] = false;
				}
				else if (e.key.keysym.sym == SDLK_UP) {
					ply.mov_Y[0] = false;
				}
				else if (e.key.keysym.sym == SDLK_DOWN) {
					ply.mov_Y[1] = false;
				}
				else if (e.key.keysym.sym == SDLK_LSHIFT ||
						e.key.keysym.sym == SDLK_RSHIFT) {
					ply.running = 1;
					if (ply.energy > 10)
						SDL_FillRect(ply.image, NULL, SDL_MapRGB(ply.image->format, 0, 100, 100));
					else
						SDL_FillRect(ply.image, NULL, SDL_MapRGB(ply.image->format, 0, 50, 50));
				}
			}
			else if (e.type == SDL_MOUSEMOTION)
				SDL_GetMouseState(&(drivf.x), &(drivf.y));
			else if (e.type == SDL_WINDOWEVENT_MOVED)
				pause(scr);
		}

		//player update
		if (true) { 
			if (ply.mov_X[0] && ply.mov_X[1])
				ply.vel[0] = 0;
			else if (ply.mov_X[0])
				ply.vel[0] = -1;
			else if (ply.mov_X[1])
				ply.vel[0] = 1;
			else
				ply.vel[0] = 0;


			if (ply.mov_Y[0] && ply.mov_Y[1])
				ply.vel[1] = 0;
			else if (ply.mov_Y[0])
				ply.vel[1] = -1;
			else if (ply.mov_Y[1])
				ply.vel[1] = 1;
			else
				ply.vel[1] = 0;

			ply.pos[0] += ply.vel[0] * (1.6 + (ply.energy / 50) * 0.4) *
				pow(ply.running, 1.5) / (ply.running == 1 ? ply.friction : 1);
			ply.pos[1] += ply.vel[1] * (1.6 + (ply.energy / 55) * 0.4) *
				pow(ply.running, 1.5) / (ply.running == 1 ? ply.friction : 1);
			
			if (ply.energy <= 70)
				ply.energy += 0.02 - (ply.running - 1) / 8; // spend energy to dash
			else {
				//ply.running = 1;
				ply.energy = 70;
			}

			if (ply.energy < 0)	
				ply.energy = 0;
			else if (int(ply.energy) == 10 && ply.running == 1)
				SDL_FillRect(ply.image, NULL, SDL_MapRGB(ply.image->format, 0, 100, 100));
			 
			if (ply.ghostlvl > 0)
				--ply.ghostlvl;

			if (ply.running > 1.2)
				ply.running -= 0.18;
			else if (ply.running > 1) {
				ply.running -= 0.2;
				if (ply.energy > 10)
					SDL_FillRect(ply.image, NULL, SDL_MapRGB(ply.image->format, 0, 100, 100));
				else
					SDL_FillRect(ply.image, NULL, SDL_MapRGB(ply.image->format, 0, 50, 50));
			}
			else
				ply.running = 1;

			if (ply.pos[0] < 350) {
				camera_offset[0] = 0;
				if (ply.pos[0] < 0)
					ply.pos[0] = 0;
			}
			else if (ply.pos[0] > world.w * world.tile_s - 350) {
				camera_offset[0] = (world.w) * world.tile_s - 700;
				if (ply.pos[0] > (world.w + 1) * world.tile_s - 40)
					ply.pos[0] = (world.w + 1) * world.tile_s - 40;
			}
			else
				camera_offset[0] = int(ply.pos[0]) - 350;

			if (ply.pos[1] < 350) {
				camera_offset[1] = 0;
				if (ply.pos[1] < 0)
					ply.pos[1] = 0;
			}
			else if (ply.pos[1] > world.h * world.tile_s - 350) {
				camera_offset[1] = (world.h) * world.tile_s - 700;
				if (ply.pos[1] > (world.h + 1) * world.tile_s - 40)
					ply.pos[1] = (world.h + 1) * world.tile_s - 40;
			}
			else
				camera_offset[1] = int(ply.pos[1]) - 350;

			//std::cout << (*camera_offset) << '\n';
			ply.g_pos[0] = int((ply.pos[0] + ply.pr.w / 2) / world.tile_s);
			ply.g_pos[1] = int((ply.pos[1] + ply.pr.h) / world.tile_s);

			if (world.tiles[ply.g_pos[1]][ply.g_pos[0]] == 5)
				ply.friction = 2;
			else
				ply.friction = 1;

			if (ply.ghostlvl > 40) {
				camera_offset[0] += rng() % 16 - 8;
				camera_offset[1] += rng() % 16 - 8;
			}

			if (ply.ghostlvl < 2 && ply.running == 1) {
				for (Tumbleweed& tw : tumbleweeds) {
					if (SDL_HasIntersection(&(tw.rect), &(ply.hr))) {
						ply.hp -= 25;
						ply.ghostlvl += 150;
					}
				}
				for (Chaser& pc : pchasers) {
					if (SDL_HasIntersection(&(pc.rect), &(ply.hr))) {
						ply.hp -= 51;
						ply.ghostlvl += 150;
					}
				}
				for (TChaser& tc : tchasers) {
					if (SDL_HasIntersection(&(tc.rect), &(ply.hr))) {
						if (tc.about_to_teleport < 3) {
							ply.hp -= 50;
							ply.ghostlvl += 150;
						}
						else {
							ply.hp -= 3;
							ply.ghostlvl += 20;
						}
					}
				}
			}

			if (ply.hp < 0)
				is_running = false; //death state
			else if (ply.hp < 100)
				ply.hp += 0.008 * ply.energy / 36;
			else
				ply.hp = 100;

			ply.pr.x = int(ply.pos[0]) - camera_offset[0];
			ply.pr.y = int(ply.pos[1]) - camera_offset[1];
			ply.hr.x = ply.pr.x + 7;
			ply.hr.y = ply.pr.y + 7;
		}

		//enemy update
		if (true) {
			for (Tumbleweed& tw : tumbleweeds)
				UpdateTumbleweed(&tw);
			for (Chaser& pc : pchasers)
				UpdatePChaser(&pc, &ply);
			for (TChaser& tc : tchasers)
				UpdateTChaser(&tc, &ply);
		}
		
		pGUIUpdate(&bars, ply); //update health/energy displays


		SDL_FillRect(scr, NULL, 0xFF969696);
		RenderMap(&world, scr);

		if ((2 > ply.running || ply.running > 4) && (ply.ghostlvl % 6 != 2))
			SDL_BlitSurface(ply.image, NULL, scr, &(ply.pr)); // Blit player to screen.

		// enemy render
		if (true) {
			for (Tumbleweed& tw : tumbleweeds)
				SDL_BlitSurface(tw.image, NULL, scr, &(tw.rect));
			for (Chaser& pc : pchasers)
				SDL_BlitSurface(pc.image, NULL, scr, &(pc.rect));
			for (TChaser& tc : tchasers) {
				if (tc.about_to_teleport == 0)
					SDL_BlitSurface(tc.image, NULL, scr, &(tc.rect));
				else
					SDL_FillRect(scr, &tc.rect, 0xffffffff);
			}
		}

		bars.blit_r.y = 8; //blit to screen health/energy displays
		SDL_BlitSurface(bars.hbar_i, NULL, scr, &(bars.blit_r));
		bars.blit_r.y = 20;
		SDL_BlitSurface(bars.ebar_i, NULL, scr, &(bars.blit_r));

		SDL_UpdateWindowSurface(win);
		
		++ticks_A;
		
		while ((ticks_A > ticks_B && is_running)) //ensure game runs smoothly
			SDL_Delay(1);

		//SDL_Delay(8);
	}

	SDL_DestroyWindow(win);
	timer.join();
	
	SDL_Quit();
	IMG_Quit();
	TTF_Quit();
	return 0;
}

