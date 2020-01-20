/*
	Accurate VU meter for PulseAudio

	See README.md and LICENSE.

	Descended from https://github.com/martincameron/vumeter.git .
*/

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <SDL.h>
#include <SDL2/SDL_ttf.h>

static const char *TITLE = "VUmeter";

static const char *VERSION = "2.0";

#define CHANNELS 8
static int channels = 2;

static char *LOGO[CHANNELS] = { [0 ... CHANNELS-1] = "Nice VU" };

static const int METER_WIDTH = 400;
static const int HEIGHT = METER_WIDTH / 2;

static const float SIX_DBA = 1.9952623149688795; /* 10^0.3 */

#define PEAK_LEVEL "-1.5 dBFS"
static const float PEAK_DBA = 0.8413951416451951; /* 10^(-1.5/20) */
#define SIGNAL_LEVEL "-56 dBFS"
static const float SIGNAL_DBA = 0.001584893192461114; /* 10^(-56.0/20) */
static const int PEAK_HOLD = 250; /* ms */

#ifndef M_PI
#define M_PI 3.141592653589793
#endif

#ifndef M_1_PI
#define M_1_PI (1/M_PI)
#endif

/* Calibrated meter labels, measured using
	$ play -n -q -t s32 -r 48000 -c 2 - synth -n sine 1000 vol <number> dB

   Be certain that PulseAudio gains are set to unity on source and sink.
*/
static const char *METER_SCALE[8] =
			{"-41", "-35", "-29", "-23", "-17", "-11", "-5", "+1"};

static const char *SCALE_FONT = "/usr/share/fonts/dejavu/DejaVuSans.ttf";
static const char *LOGO_FONT =
			"/usr/share/fonts/dejavu/DejaVuSans-BoldOblique.ttf";

struct sprung_mass {
	float m; /* mass */
	float k; /* spring constant */
	float d; /* damping coefficient */
	float x; /* displacement */
	float v; /* velocity */
};

static int time;
static float force[CHANNELS];

static struct sprung_mass mass[CHANNELS] = {
	/* The natural frequency is 2.1 Hz +/- 10%, as required by spec.
	   Damping lowers the resonant frequency, due to the second factor:
		w = sqrt(k/m)*sqrt(1-(d/(2*sqrt(m/k)))^2)
	   Note that w is angular frequency; f = w/(2*pi). */
	[0 ... CHANNELS-1] = {0.005, 1.0, 0.08, 1}
};

/* Apply the specified force to the sprung mass for the specified number
   of milliseconds. */
static float model(struct sprung_mass *sm, float force, float x_min,
						float x_max, int millis) {
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
	float force = ( logf(amplitude) / logf(SIX_DBA) + 7 ) / 7;
	if (force < 0) {
		force = 0;
	}
	return ( atanf( force * 2 - 1 ) * M_1_PI * 4 + 1 ) / 2;
}

static Uint32 peak[CHANNELS] = {0};
static int signal[CHANNELS] = {0};

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
	float lin = max_amp / 32768.0;
	if (lin > PEAK_DBA) peak[channel] = PEAK_HOLD + SDL_GetTicks();
	signal[channel] = lin > SIGNAL_DBA;
	return lin;
}

static Uint32 timer_callback(Uint32 interval, void *param) {
	SDL_Event event;
	int now = SDL_GetTicks();
	/* Update model, assuming callback is regularly called. */
	int cn;
	for (cn = 0; cn < channels; ++cn) {
		model(&mass[cn], force[cn], 0, 1, now-time);
	}
	time = now;
	/* Push redraw event. */
	event.type = SDL_USEREVENT;
	event.user.code = 0;
	event.user.data1 = event.user.data2 = NULL;
	SDL_PushEvent(&event);
	return interval;
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
	/* Calculate force on meter springs. */
	int cn;
	for (cn = 0; cn < channels; ++cn) {
		force[cn] =
			get_force(get_max_amplitude((Sint16*)stream,
				len/(sizeof(Sint16)/sizeof(Uint8)), cn));
	}
}

static void draw_gradient(SDL_Renderer *renderer, Uint32 colour1,
				Uint32 colour2, int width, int height) {
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
	SDL_SetRenderDrawColor(renderer, (colour>>16)&0xFF,
					(colour>>8)&0xFF, colour&0xFF, 255);
}

static void draw_rect(SDL_Renderer *renderer, int x, int y, int width,
								int height) {
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = width;
	rect.h = height;
	SDL_RenderDrawRect(renderer, &rect);
}

static void fill_rect(SDL_Renderer *renderer, int x, int y, int width,
								int height) {
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = width;
	rect.h = height;
	SDL_RenderFillRect(renderer, &rect);
}

static void draw_ruler(SDL_Renderer *renderer, int x, int width,
				Uint32 scale_colour, Uint32 peak_colour) {
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
				fill_rect(renderer, n+segment_width*v/6,
								y, 1, h/2);
			}
		}
	}
	/* End red segment. */
	set_colour(renderer, peak_colour);
	fill_rect(renderer, x+segment_width*6, y, segment_width, h);
}

static void sdl_check(int ok, const char *msg) {
	if (!ok) {
		fprintf(stderr, "Unable to %s: %s\n", msg, SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
}

static void draw_labels(SDL_Renderer *renderer, int x, int width) {
	TTF_Font *font = TTF_OpenFont(SCALE_FONT, 17);
	sdl_check(font != NULL, "open labels font");
	SDL_Color white = {200, 200, 200};
	int rlx = x + width / 16;
	int y = width / 5;
	int i, w, h;
	SDL_Surface *surface;
	SDL_Texture *texture;
	/* Ruler labels */
	for (i = 0; i < 8; ++i) {
		surface = TTF_RenderUTF8_Solid(font, METER_SCALE[i], white);
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_QueryTexture(texture, NULL, NULL, &w, &h);
		SDL_Rect dstrect = {rlx-w/2, y, w, h};
		SDL_RenderCopy(renderer, texture, NULL, &dstrect);
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(surface);
		rlx = rlx + width / 8;
	}
	SDL_Color white2 = {240, 240, 240};
	/* Signal indicator label */
	surface = TTF_RenderUTF8_Solid(font, "SIGNAL", white2);
	texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_QueryTexture(texture, NULL, NULL, &w, &h);
	SDL_Rect dstrect_s = {x+width/8-w/2, width*6/16, w, h};
	SDL_RenderCopy(renderer, texture, NULL, &dstrect_s);
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
	/* Peak indicator label */
	surface = TTF_RenderUTF8_Solid(font, "PEAK", white2);
	texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_QueryTexture(texture, NULL, NULL, &w, &h);
	SDL_Rect dstrect_p = {x+width*7/8-w/2, width*6/16, w, h};
	SDL_RenderCopy(renderer, texture, NULL, &dstrect_p);
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
	TTF_CloseFont(font);
}

static void draw_logo(SDL_Renderer *renderer, int x, int width, char *logo) {
	TTF_Font *font = TTF_OpenFont(LOGO_FONT, 21);
	sdl_check(font != NULL, "open logo font");
	SDL_Color black = {70, 70, 70};
	SDL_Surface *surface = TTF_RenderUTF8_Solid(font, logo, black);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	x = x + width / 2;
	int y = width *3 / 8;
	int w, h;
	SDL_QueryTexture(texture, NULL, NULL, &w, &h);
	SDL_Rect dstrect = {x-w/2, y, w, h};
	SDL_RenderCopy(renderer, texture, NULL, &dstrect);
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
	TTF_CloseFont(font);
}

static void draw_needle(SDL_Renderer *renderer, int x, int width,
					Uint32 colour, float deflection) {
	float angle = deflection * M_PI / 2 - M_PI / 4;
	int x1 = width / 2 + width * 8 / 16 * sinf(angle);
	int y1 = width * 9 / 16 - width * 8 / 16 * cosf(angle);
	int x2 = width / 2 + width * 3 / 16 * tanf(angle);
	int y2 = width * 3 / 8;
	set_colour(renderer, colour);
	SDL_RenderDrawLine(renderer, x+x1, y1, x+x2, y2);
}

static void draw_indicators(SDL_Renderer *renderer, int x, int width,
						int signal, Uint32 *peak) {
	if (signal) {
		SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
		fill_rect(renderer, x+width*7/64+1, width/3+1,
						width/32-2, width/32-2);
	}
	if (*peak) {
		if (*peak > SDL_GetTicks()) {
			SDL_SetRenderDrawColor(renderer, 255, 255, 50, 255);
		} else {
			*peak = 0;
		}
		fill_rect(renderer, x+width*55/64+1, width/3+1,
						width/32-2, width/32-2);
	}
}

static void draw_meter(SDL_Renderer *renderer, Uint32 scale_colour,
			Uint32 peak_colour, int x, int width, int height,
			char *logo) {
	set_colour(renderer, 0x806633);
	draw_rect(renderer, x+width*7/64, width/3, width/32, width/32);
	draw_rect(renderer, x+width*55/64, width/3, width/32, width/32);
	draw_rect(renderer, x+width*5/16, width*3/8, width*6/16, width/16);
	set_colour(renderer, scale_colour);
	draw_ruler(renderer, x, width, scale_colour, peak_colour);
	draw_labels(renderer, x, width);
	draw_logo(renderer, x, width, logo);
}

static struct SDL_Renderer *renderer;
static struct SDL_Texture *target, *background = NULL;

static void setup(int width) {
	struct SDL_Window *window;
	SDL_AudioSpec audiospec = {0};
	SDL_AudioDeviceID audiodev;
	SDL_TimerID timer;
	/* Initialize SDL. */
	SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
	if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER) == 0) {
		TTF_Init();
		window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED,
				SDL_WINDOWPOS_UNDEFINED, width, HEIGHT, 0);
		sdl_check(window != NULL, "create window");
		renderer = SDL_CreateRenderer(window, -1,
						SDL_RENDERER_TARGETTEXTURE);
		sdl_check(renderer != NULL, "create renderer");
		target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET, width, HEIGHT);
		sdl_check(target != NULL, "create texture (target)");
		background = SDL_CreateTexture(renderer,
				SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET, width, HEIGHT);
		sdl_check(background != NULL, "create texture (background)");
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		/* Draw background. */
		SDL_SetRenderTarget(renderer, background);
		draw_gradient(renderer, 0x806633, 0xFFCC66, width, HEIGHT);
		int cn;
		for (cn = 0; cn < channels; ++cn) {
			draw_meter(renderer, 0, 0xAA0000, cn*METER_WIDTH,
						METER_WIDTH, HEIGHT, LOGO[cn]);
		}
		SDL_RenderPresent(renderer);
		/* Initialize audio. */
		audiospec.freq = 48000;
		audiospec.format = AUDIO_S16SYS;
		audiospec.channels = channels;
		audiospec.samples = 1024;
		audiospec.callback = audio_callback;
		audiodev = SDL_OpenAudioDevice(NULL, 1, &audiospec,
							&audiospec, 0);
		sdl_check(audiodev >= 0, "open audio device");
		SDL_PauseAudioDevice(audiodev, 0);
		time = SDL_GetTicks();
		timer = SDL_AddTimer(12, timer_callback, NULL);
		sdl_check(timer, "start timer");
	} else {
		fprintf(stderr, "Unable to initialise SDL: %s\n",
							SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void usage() {
	puts(
		"Options:\n"
		"  -c #  display # meters (default: 2)\n"
		"  -v    display version and exit\n"
		"  -h    display help and exit\n"
		"\n"
		"Each non-option argument provides logo text for a meter.\n"
		"\n"
		"Reference levels:\n"
		"  0 VU = 0 dBFS\n"
		"  SIGNAL = " SIGNAL_LEVEL "\n"
		"  PEAK = " PEAK_LEVEL "\n"
		"\n"
		"Ballistics (nominal):\n"
		"  300 ms to 99% of FS deflection; 1% overshoot.\n"
	);
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "c:hv")) != -1) {
		switch (opt) {
		case 'c':
			channels = atoi(optarg);
			if (channels < 1 || channels > CHANNELS) {
				fprintf(stderr, "Specify 1 to %d channels\n",
								CHANNELS);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'v':
			fprintf(stderr, "%s %s\n", TITLE, VERSION);
			exit(EXIT_SUCCESS);
		}
	}
	int i, cn = 0;
	for (i = optind; i < argc; ++i) {
		if (cn == channels) break;
		if (*argv[i]) {
			LOGO[cn] = argv[i];
		}
		++cn;
	}
	setup(METER_WIDTH*channels);
	SDL_Event event;
	while(1) {
		SDL_WaitEvent(&event);
		if (event.type == SDL_QUIT) {
			break;
		} else if (event.type == SDL_AUDIODEVICEREMOVED) {
			fprintf(stderr, "audio device removed\n");
			break;
		} else if (event.type == SDL_USEREVENT) {
			/* Redraw. */
			SDL_SetRenderTarget(renderer, target);
			SDL_RenderCopy(renderer, background, NULL, NULL);
			int cn, pos = 0;
			for (cn = 0; cn < channels; ++cn) {
				draw_needle(renderer, pos, METER_WIDTH, 0,
								mass[cn].x);
				draw_indicators(renderer, pos, METER_WIDTH,
							signal[cn], &peak[cn]);
				pos += METER_WIDTH;
			}
			SDL_SetRenderTarget(renderer, NULL);
			SDL_RenderCopy(renderer, target, NULL, NULL);
			SDL_RenderPresent(renderer);
		}
	}
	TTF_Quit();
	SDL_Quit();
	return EXIT_SUCCESS;
}
