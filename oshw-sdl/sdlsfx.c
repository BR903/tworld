/* sdlsfx.c: Creating the program's sound effects.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"
#include	"../state.h"

/* The requested settings for the sound output. The sample buffer is
 * set especially low so that sounds can come and go on every tick.
 */
#define DEFAULT_SND_FMT		AUDIO_S16LSB
#define DEFAULT_SND_FREQ	22050
#define	DEFAULT_SND_SAMP	512
#define	DEFAULT_SND_CHAN	1

typedef	struct sfxinfo {
    Uint8	       *wave;
    Uint32		len;
    int			pos;
    int			playing;
    char const	       *textsfx;
} sfxinfo;

static SDL_AudioSpec	spec;
static sfxinfo		sounds[SND_COUNT];

/*
 *
 */

static void displaysoundeffects(unsigned long sfx)
{
    static char const  *playing = NULL;
    char const	       *play;
    unsigned long	flag;
    int			i;

    play = NULL;
    for (flag = 1, i = 0 ; flag ; flag <<= 1, ++i) {
	if (sfx & flag) {
	    play = sounds[i].textsfx;
	    break;
	}
    }
    if (!play && playing) {
	play = playing;
	playing = NULL;
    } else
	playing = play;

    if (!play)
	play = "";
    puttext(&sdlg.textsfxrect, play, -1, 0);
}

/*
 *
 */

static void sfxcallback(void *data, Uint8 *wave, int len)
{
    int	i, n;

    (void)data;
    for (i = 0 ; i < SND_COUNT ; ++i) {
	if (!sounds[i].wave)
	    continue;
	if (!sounds[i].playing && !sounds[i].pos)
	    continue;
	n = sounds[i].len - sounds[i].pos;
	if (n > len) {
	    SDL_MixAudio(wave, sounds[i].wave + sounds[i].pos, len,
			 SDL_MIX_MAXVOLUME);
	    sounds[i].pos += len;
	} else {
	    SDL_MixAudio(wave, sounds[i].wave + sounds[i].pos, n,
			 SDL_MIX_MAXVOLUME);
	    sounds[i].pos = 0;
	    if (i < SND_ONESHOT_COUNT) {
		sounds[i].playing = FALSE;
	    } else if (sounds[i].playing) {
		sounds[i].pos = len - n;
		SDL_MixAudio(wave + n, sounds[i].wave, sounds[i].pos,
			     SDL_MIX_MAXVOLUME);
	    }
	}
    }
}

/*
 *
 */

int setaudiosystem(int active)
{
    SDL_AudioSpec	des;

    if (!active) {
	if (spec.freq) {
	    SDL_PauseAudio(TRUE);
	    SDL_CloseAudio();
	    spec.freq = 0;
	}
	return TRUE;
    }

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
	    warn("Cannot initialize audio output: %s", SDL_GetError());
	    return FALSE;
	}
    }

    if (spec.freq)
	return TRUE;
    des.freq = DEFAULT_SND_FREQ;
    des.format = DEFAULT_SND_FMT;
    des.samples = DEFAULT_SND_SAMP;
    des.channels = DEFAULT_SND_CHAN;
    des.callback = sfxcallback;
    des.userdata = NULL;
    if (SDL_OpenAudio(&des, &spec) < 0) {
	warn("can't access audio output: %s\n", SDL_GetError());
	spec.freq = 0;
	return FALSE;
    }
    SDL_PauseAudio(FALSE);

    return TRUE;
}

int loadsfxfromfile(int index, char const *filename)
{
    SDL_AudioSpec	specin;
    SDL_AudioCVT	convert;
    Uint8	       *wavein;
    Uint8	       *wavecvt;
    Uint32		lengthin;

    if (!filename) {
	freesfx(index);
	return TRUE;
    }
    if (!spec.freq)
	return FALSE;

    if (!SDL_LoadWAV(filename, &specin, &wavein, &lengthin)) {
	warn("can't load %s: %s\n", filename, SDL_GetError());
	return FALSE;
    }

    if (SDL_BuildAudioCVT(&convert,
			  specin.format, specin.channels, specin.freq,
			  spec.format, spec.channels, spec.freq) < 0) {
	warn("can't create converter for %s: %s\n", filename, SDL_GetError());
	return FALSE;
    }
    wavecvt = malloc(lengthin * convert.len_mult);
    if (!wavecvt)
	memerrexit();
    memcpy(wavecvt, wavein, lengthin);
    SDL_FreeWAV(wavein);
    convert.buf = wavecvt;
    convert.len = lengthin;
    if (SDL_ConvertAudio(&convert) < 0) {
	warn("can't convert %s: %s\n", filename, SDL_GetError());
	return FALSE;
    }

    freesfx(index);
    SDL_LockAudio();
    sounds[index].wave = convert.buf;
    sounds[index].len = convert.len * convert.len_ratio;
    sounds[index].pos = 0;
    sounds[index].playing = FALSE;
    SDL_UnlockAudio();

    return TRUE;
}

void playsoundeffects(unsigned long sfx)
{
    unsigned long	flag;
    int			i;

    if (!spec.freq) {
	displaysoundeffects(sfx);
	return;
    }
    SDL_LockAudio();
    for (i = 0, flag = 1 ; i < SND_COUNT ; ++i, flag <<= 1) {
	if (sfx & flag) {
	    if (sounds[i].playing && i < SND_ONESHOT_COUNT)
		sounds[i].pos = 0;
	    sounds[i].playing = TRUE;
	} else
	    sounds[i].playing = FALSE;
    }
    SDL_UnlockAudio();
}

void selectsoundset(int ruleset)
{
    if (ruleset == Ruleset_MS) {
	sounds[SND_CHIP_LOSES].textsfx      = "\"Bummer\"";
	sounds[SND_CHIP_WINS].textsfx       = "Tadaa!";
	sounds[SND_TIME_OUT].textsfx        = "Clang!";
	sounds[SND_TIME_LOW].textsfx        = "Ktick!";
	sounds[SND_CANT_MOVE].textsfx       = "Mnphf!";
	sounds[SND_IC_COLLECTED].textsfx    = "Chack!";
	sounds[SND_ITEM_COLLECTED].textsfx  = "Slurp!";
	sounds[SND_BOOTS_STOLEN].textsfx    = "Flonk!";
	sounds[SND_TELEPORTING].textsfx     = "Whing!";
	sounds[SND_DOOR_OPENED].textsfx     = "Spang!";
	sounds[SND_SOCKET_OPENED].textsfx   = "Chack!";
	sounds[SND_BUTTON_PUSHED].textsfx   = "Click!";
	sounds[SND_BOMB_EXPLODES].textsfx   = "Booom!";
	sounds[SND_WATER_SPLASH].textsfx    = "Plash!";
    } else {
	sounds[SND_CHIP_LOSES].textsfx      = "Splat!";
	sounds[SND_CHIP_WINS].textsfx       = "Tadaa!";
	sounds[SND_CANT_MOVE].textsfx       = "Thunk!";
	sounds[SND_IC_COLLECTED].textsfx    = "Slurp!";
	sounds[SND_ITEM_COLLECTED].textsfx  = "Slurp!";
	sounds[SND_BOOTS_STOLEN].textsfx    = "Flonk!";
	sounds[SND_TELEPORTING].textsfx     = "Bamff!";
	sounds[SND_DOOR_OPENED].textsfx     = "Spang!";
	sounds[SND_SOCKET_OPENED].textsfx   = "Slurp!";
	sounds[SND_BUTTON_PUSHED].textsfx   = "Click!";
	sounds[SND_TILE_EMPTIED].textsfx    = "Whisk!";
	sounds[SND_WALL_CREATED].textsfx    = "Chunk!";
	sounds[SND_TRAP_ENTERED].textsfx    = "Shunk!";
	sounds[SND_BOMB_EXPLODES].textsfx   = "Booom!";
	sounds[SND_WATER_SPLASH].textsfx    = "Plash!";
	sounds[SND_SKATING_TURN].textsfx    = "Whing!";
	sounds[SND_SKATING_FORWARD].textsfx = "Whizz ...";
	sounds[SND_SLIDING].textsfx         = "Drrrr ...";
	sounds[SND_BLOCK_MOVING].textsfx    = "Scrrr ...";
	sounds[SND_SLIDEWALKING].textsfx    = "slurp slurp ...";
	sounds[SND_ICEWALKING].textsfx      = "snick snick ...";
	sounds[SND_WATERWALKING].textsfx    = "plip plip ...";
	sounds[SND_FIREWALKING].textsfx     = "crackle crackle ...";
    }
}

void freesfx(int index)
{
    if (sounds[index].wave) {
	SDL_LockAudio();
	SDL_FreeWAV(sounds[index].wave);
	sounds[index].wave = NULL;
	sounds[index].pos = 0;
	sounds[index].playing = FALSE;
	SDL_UnlockAudio();
    }
}

static void shutdown(void)
{
    setaudiosystem(FALSE);
    if (SDL_WasInit(SDL_INIT_AUDIO))
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

int _sdlsfxinitialize(int silence)
{
    atexit(shutdown);
    if (!silence)
	return setaudiosystem(TRUE);
    return TRUE;
}
