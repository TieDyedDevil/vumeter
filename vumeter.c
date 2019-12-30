
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <SDL.h>

static const char *TITLE = "vumeter";

static const char *VERSION = "2.0";

static const int WIDTH = 800, HEIGHT = WIDTH / 4;

static const float SIX_DBA = powf(10, 0.3);

/* Calibrated meter labels, measured using
	play </dev/zero -q -t s32 -r 22050 -c 2 - synth sine 1000 vol <number> dB
*/
static const char *METER_SCALE[8] = {"-∞", "-42", "-36", "-27", "-19", "-11", "-4", "0"};

struct sprung_mass {
	float m; /* Mass. */
	float k; /* Spring constant. */
	float d; /* Damping coefficient. */
	float x; /* Displacement. */
	float v; /* Velocity. */
};

static int time;
static float left_force, right_force;

/* The natural frequency of this system should be 2.1 Hz +/- 10%. */
static struct sprung_mass left_mass = {0.005, 1.0, 0.08, 1}, right_mass = {0.005, 1.0, 0.08, 1};

/* Apply the specified force to the sprung mass for the specified number of milliseconds. */
static float model(struct sprung_mass *sm, float force, float x_min, float x_max, int millis) {
	int t, a;
	for (t = 0; t < millis; t++) {
		if (sm->x < x_min) {
			sm->x = x_min;
			if (sm->v < 0) {
				sm->v = -sm->v;
			}
		}
		if (sm->x > x_max) {
			sm->x = x_max;
			if (sm->v > 0) {
				sm->v = -sm->v;
			}
		}
		/* Acceleration due to force. */
		a = (force - sm->k * sm->x - sm->d * sm->v) / sm->m;
		/* Change in velocity due to acceleration. */
		sm->v = sm->v + a * 0.001;
		/* Change in displacement due to velocity. */
		sm->x = sm->x + sm->v * 0.001;
	}
	return sm->x;
}

static float get_force(float amplitude) {
	float force = logf(amplitude) / logf(SIX_DBA) + 8;
	if (force < 0) {
		force = 0;
	}
	return force / 8;
}

static float get_max_amplitude(Sint16 *audio_buf, int len, int channel) {
	int idx, amp, max_amp = 0;
	for (idx = 0; idx < len; idx += 2) {
		amp = audio_buf[idx+channel];
		if (amp < 0) {
			amp = -amp;
		}
		if (amp > max_amp) {
			max_amp = amp;
		}
	}
	return max_amp / 32768.0;
}

static Uint32 timer_callback(Uint32 interval, void *param) {
	SDL_Event event;
	int now = SDL_GetTicks();
	/* Update model, assuming callback is regularly called. */
	model(&left_mass, left_force, 0, 1, now-time);
	model(&right_mass, right_force, 0, 1, now-time);
	time = now;
	/* Push redraw event. */
	event.type = SDL_WINDOWEVENT;
	event.user.code = 0;
	event.user.data1 = event.user.data2 = NULL;
	SDL_PushEvent(&event);
	return interval;
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
	/* Calculate force on meter springs. */
	left_force = get_force(get_max_amplitude((Sint16*)stream, len/2, 0));
	right_force = get_force(get_max_amplitude((Sint16*)stream, len/2, 1));
}

static void draw_gradient(SDL_Renderer *renderer, Uint32 colour1, Uint32 colour2, int width, int height) {
	int r1 = (colour1 >> 16) & 0xFF;
	int r2 = (colour2 >> 16) & 0xFF;
	int g1 = (colour1 >> 8) & 0xFF;
	int g2 = (colour2 >> 8) & 0xFF;
	int b1 = colour1 & 0xFF;
	int b2 = colour2 & 0xFF;
	int y, r, g, b;
	SDL_Rect rect;
	for (y = 0; y < height; y++) {
		r = r1 + (r2 - r1) * y / height;
		g = g1 + (g2 - g1) * y / height;
		b = b1 + (b2 - b1) * y / height;
		SDL_SetRenderDrawColor(renderer, r, g, b, 255);
		rect.x = 0;
		rect.y = y;
		rect.w = width;
		rect.h = 1;
		SDL_RenderFillRect(renderer, &rect);
	}
}

static void set_colour(SDL_Renderer *renderer, Uint32 colour) {
	SDL_SetRenderDrawColor(renderer, (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF, 255);
}

static void draw_rect(SDL_Renderer *renderer, int x, int y, int width, int height) {
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = width;
	rect.h = height;
	SDL_RenderDrawRect(renderer, &rect);
}

static void fill_rect(SDL_Renderer *renderer, int x, int y, int width, int height) {
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = width;
	rect.h = height;
	SDL_RenderFillRect(renderer, &rect);
}

static void draw_ruler(SDL_Renderer *renderer, int x, int width, Uint32 scale_colour, Uint32 peak_colour) {
	int n;
	int y = width / 8;
	int w = width * 14 / 16;
	int h = width / 16;
	int segment_width = w / 7;
	x = x + width / 16;
	/* First black segment. */
	set_colour(renderer, scale_colour);
	fill_rect(renderer, x, y, segment_width, h);
	/* Ruler segments. */
	for (n = x+segment_width; n < x+segment_width*6; n += segment_width){
		fill_rect(renderer, n, y, segment_width, 1);
		/* Big marks. */
		fill_rect(renderer, n, y, 1, h);
		/* Small marks. */
		if (segment_width >= 12) {
			int v;
			for (v = 1; v < 6; v++) {
				fill_rect(renderer, n+segment_width*v/6, y, 1, h/2);
			}
		}
	}
	/* End red segment. */
	set_colour(renderer, peak_colour);
	fill_rect(renderer, x+segment_width*6, y, segment_width, h);
}

static void draw_needle(SDL_Renderer *renderer, int x, int width, Uint32 colour, float deflection) {
	float angle = deflection * M_PI / 2 - M_PI / 4;
	int x1 = width / 2 + width * 8 / 16 * sinf(angle);
	int y1 = width * 9 / 16 - width * 8 / 16 * cosf(angle);
	int x2 = width / 2 + width * 3 / 16 * tanf(angle);
	int y2 = width * 3 / 8;
	set_colour(renderer, colour);
	SDL_RenderDrawLine(renderer, x+x1, y1, x+x2, y2);
}

static void draw_meter(SDL_Renderer *renderer, Uint32 scale_colour, Uint32 peak_colour, int x, int width, int height) {
	set_colour(renderer, scale_colour);
	draw_rect(renderer, x+width*5/16, width*3/8, width*6/16, width/16);
	draw_ruler(renderer, x, width, scale_colour, peak_colour);
}

static void sdl_check(int ok, const char *msg) {
	if (!ok) {
		fprintf(stderr, "Unable to %s: %s\n", msg, SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
}

static struct SDL_Renderer *renderer;
static struct SDL_Texture *target, *background = NULL;

static void setup() {
	struct SDL_Window *window;
	SDL_AudioSpec audiospec = {0};
	SDL_AudioDeviceID audiodev;
	SDL_TimerID timer;
	/* Initialize SDL. */
	if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER) == 0) {
		window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, 0);
		sdl_check(window != NULL, "create window");
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
		sdl_check(renderer != NULL, "create renderer");
		target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
		sdl_check(target != NULL, "create texture (target)");
		background = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
		sdl_check(background != NULL, "create texture (background)");
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		/* Draw background. */
		SDL_SetRenderTarget(renderer, background);
		draw_gradient(renderer, 0x806633, 0xFFCC66, WIDTH, HEIGHT);
		draw_meter(renderer, 0, 0xAA0000, 0, WIDTH/2, HEIGHT);
		draw_meter(renderer, 0, 0xAA0000, WIDTH/2, WIDTH/2, HEIGHT);
		SDL_RenderPresent(renderer);
		/* Initialize audio. */
		audiospec.freq = 48000;
		audiospec.format = AUDIO_S16SYS;
		audiospec.channels = 2;
		audiospec.samples = 1024;
		audiospec.callback = audio_callback;
		audiodev = SDL_OpenAudioDevice(NULL, 1, &audiospec, &audiospec, 0);
		sdl_check(audiodev >= 0, "open audio device");
		SDL_PauseAudioDevice(audiodev, 0);
		time = SDL_GetTicks();
		timer = SDL_AddTimer(12, timer_callback, NULL);
		sdl_check(timer, "start timer");
	} else {
		fprintf(stderr, "Unable to initialise SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			fprintf(stderr, "%s %s\n", TITLE, VERSION);
			exit(EXIT_SUCCESS);
		}
	}
	setup();
	SDL_Event event;
	while(1) {
		SDL_WaitEvent(&event);
		if (event.type == SDL_QUIT) {
			exit(EXIT_SUCCESS);
		} else if (event.type == SDL_WINDOWEVENT) {
			/* Redraw. */
			SDL_SetRenderTarget(renderer, target);
			SDL_RenderCopy(renderer, background, NULL, NULL);
			draw_needle(renderer, 0, WIDTH/2, 0, left_mass.x);
			draw_needle(renderer, WIDTH/2, WIDTH/2, 0, right_mass.x);
			SDL_SetRenderTarget(renderer, NULL);
			SDL_RenderCopy(renderer, target, NULL, NULL);
			SDL_RenderPresent(renderer);
		}
	}
	SDL_Quit();
	return EXIT_SUCCESS;
}
