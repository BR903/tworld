/* sdlsfx.c: Creating the program's sound effects.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"
#include	"../state.h"

/* Some generic default settings for the audio output.
 */
#define DEFAULT_SND_FMT		AUDIO_S16LSB
#define DEFAULT_SND_FREQ	22050
#define	DEFAULT_SND_CHAN	1

/* The data needed for each sound.
 */
typedef	struct sfxinfo {
    Uint8	       *wave;		/* the actual wave data */
    Uint32		len;		/* size of the wave data */
    int			pos;		/* how much has been played already */
    int			playing;	/* is the wave currently playing? */
    char const	       *textsfx;	/* the onomatopoeia string */
} sfxinfo;

/* The data needed to talk to the sound output device.
 */
static SDL_AudioSpec	spec;

/* All of the sound effects.
 */
static sfxinfo		sounds[SND_COUNT];

/* TRUE if sound-playing has been enabled.
 */
static int		enabled = FALSE;

/* TRUE if there is currently a sound device to talk to.
 */
static int		hasaudio = FALSE;

/* The sound's volume level.
 */
static int		volume = SDL_MIX_MAXVOLUME;


/* Initialize the textual sound effects.
 */
static void initonomatopoeia(void)
{
    sounds[SND_CHIP_LOSES].textsfx      = "\"Bummer\"";
    sounds[SND_CHIP_WINS].textsfx       = "Tadaa!";
    sounds[SND_TIME_OUT].textsfx        = "Clang!";
    sounds[SND_TIME_LOW].textsfx        = "Ktick!";
    sounds[SND_DEREZZ].textsfx		= "Bzont!";
    sounds[SND_CANT_MOVE].textsfx       = "Mnphf!";
    sounds[SND_IC_COLLECTED].textsfx    = "Chack!";
    sounds[SND_ITEM_COLLECTED].textsfx  = "Slurp!";
    sounds[SND_BOOTS_STOLEN].textsfx    = "Flonk!";
    sounds[SND_TELEPORTING].textsfx     = "Bamff!";
    sounds[SND_DOOR_OPENED].textsfx     = "Spang!";
    sounds[SND_SOCKET_OPENED].textsfx   = "Clack!";
    sounds[SND_BUTTON_PUSHED].textsfx   = "Click!";
    sounds[SND_BOMB_EXPLODES].textsfx   = "Booom!";
    sounds[SND_WATER_SPLASH].textsfx    = "Plash!";
    sounds[SND_TILE_EMPTIED].textsfx    = "Whisk!";
    sounds[SND_WALL_CREATED].textsfx    = "Chunk!";
    sounds[SND_TRAP_ENTERED].textsfx    = "Shunk!";
    sounds[SND_SKATING_TURN].textsfx    = "Whing!";
    sounds[SND_SKATING_FORWARD].textsfx = "Whizz ...";
    sounds[SND_SLIDING].textsfx         = "Drrrr ...";
    sounds[SND_BLOCK_MOVING].textsfx    = "Scrrr ...";
    sounds[SND_SLIDEWALKING].textsfx    = "slurp slurp ...";
    sounds[SND_ICEWALKING].textsfx      = "snick snick ...";
    sounds[SND_WATERWALKING].textsfx    = "plip plip ...";
    sounds[SND_FIREWALKING].textsfx     = "crackle crackle ...";
}

/* Display the onomatopoeia for the currently playing sound effect.
 */
static void displaysoundeffects(unsigned long sfx, int display)
{
    unsigned long	flag;
    int			i;

    if (!display) {
	setdisplaymsg(NULL, 0, 0);
	return;
    }

    for (flag = 1, i = 0 ; flag ; flag <<= 1, ++i) {
	if (sfx & flag) {
	    setdisplaymsg(sounds[i].textsfx, 500, 10);
	    return;
	}
    }
}

/* The function that is called by the sound driver to supply the
 * latest sound effects.
 */
static void sfxcallback(void *data, Uint8 *wave, int len)
{
    int	i, n;

    (void)data;
    for (i = 0 ; i < SND_COUNT ; ++i) {
	if (!sounds[i].wave)
	    continue;
	if (!sounds[i].playing)
	    if (!sounds[i].pos || i >= SND_ONESHOT_COUNT)
		continue;
	n = sounds[i].len - sounds[i].pos;
	if (n > len) {
	    SDL_MixAudio(wave, sounds[i].wave + sounds[i].pos, len, volume);
	    sounds[i].pos += len;
	} else {
	    SDL_MixAudio(wave, sounds[i].wave + sounds[i].pos, n, volume);
	    sounds[i].pos = 0;
	    if (i < SND_ONESHOT_COUNT) {
		sounds[i].playing = FALSE;
	    } else if (sounds[i].playing) {
		while (len - n >= (int)sounds[i].len) {
		    SDL_MixAudio(wave + n, sounds[i].wave, sounds[i].len,
				 volume);
		    n += sounds[i].len;
		}
		sounds[i].pos = len - n;
		SDL_MixAudio(wave + n, sounds[i].wave, sounds[i].pos, volume);
	    }
	}
    }
}

/*
 * The exported functions.
 */

/* Activate or deactivate the sound system.
 */
int setaudiosystem(int active)
{
    SDL_AudioSpec	des;
    int			n;

    if (!enabled)
	return !active;

    if (!active) {
	if (hasaudio) {
	    SDL_PauseAudio(TRUE);
	    SDL_CloseAudio();
	    hasaudio = FALSE;
	}
	return TRUE;
    }

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
	    warn("Cannot initialize audio output: %s", SDL_GetError());
	    return FALSE;
	}
    }

    if (hasaudio)
	return TRUE;

    des.freq = DEFAULT_SND_FREQ;
    des.format = DEFAULT_SND_FMT;
    des.channels = DEFAULT_SND_CHAN;
    des.callback = sfxcallback;
    des.userdata = NULL;
    for (n = 1 ; n <= des.freq / TICKS_PER_SECOND ; n <<= 1) ;
    des.samples = n >> 2;
    if (SDL_OpenAudio(&des, &spec) < 0) {
	warn("can't access audio output: %s\n", SDL_GetError());
	return FALSE;
    }
    hasaudio = TRUE;
    SDL_PauseAudio(FALSE);

    return TRUE;
}

/* Load a wave file into memory. The wave data is converted to the
 * format expected by the sound device.
 */
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

    if (!enabled)
	return FALSE;

    if (!hasaudio)
	if (!setaudiosystem(TRUE))
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
    if (!(wavecvt = malloc(lengthin * convert.len_mult)))
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

/* Select the sounds effects to be played.
 */
void playsoundeffects(unsigned long sfx)
{
    unsigned long	flag;
    int			i;

    if (!hasaudio || !volume) {
	displaysoundeffects(sfx, TRUE);
	return;
    }

    SDL_LockAudio();
    for (i = 0, flag = 1 ; i < SND_COUNT ; ++i, flag <<= 1) {
	if (sfx & flag) {
	    sounds[i].playing = TRUE;
	    if (sounds[i].pos && i < SND_ONESHOT_COUNT)
		sounds[i].pos = 0;
	} else
	    sounds[i].playing = FALSE;
    }
    SDL_UnlockAudio();
}

/* Stop playing all sounds immediately.
 */
void clearsoundeffects(void)
{
    int	i;

    if (!hasaudio || !volume) {
	displaysoundeffects(0, FALSE);
	return;
    }

    SDL_LockAudio();
    for (i = 0 ; i < SND_COUNT ; ++i) {
	sounds[i].playing = FALSE;
	sounds[i].pos = 0;
    }
    SDL_UnlockAudio();
}

/* Release all memory used for the given sound effect.
 */
void freesfx(int index)
{
    if (sounds[index].wave) {
	SDL_LockAudio();
	free(sounds[index].wave);
	sounds[index].wave = NULL;
	sounds[index].pos = 0;
	sounds[index].playing = FALSE;
	SDL_UnlockAudio();
    }
}

/* Change the current volume level.
 */
int changevolume(int delta, int display)
{
    char	buf[16];
    int		v;

    if (!hasaudio)
	return FALSE;
    v = (10 * volume) / SDL_MIX_MAXVOLUME;
    v += delta;
    if (v < 0)
	v = 0;
    else if (v > 10)
	v = 10;
    if (!volume && v)
	displaysoundeffects(0, FALSE);
    volume = (SDL_MIX_MAXVOLUME * v + 9) / 10;
    if (display) {
	sprintf(buf, "Volume: %d", v);
	setdisplaymsg(buf, 1000, -1);
    }
    return TRUE;
}

/* Shut down the sound system.
 */
static void shutdown(void)
{
    setaudiosystem(FALSE);
    if (SDL_WasInit(SDL_INIT_AUDIO))
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
    hasaudio = FALSE;
}

/* Initialize the sound system.
 */
int _sdlsfxinitialize(int silence)
{
    atexit(shutdown);
    enabled = !silence;
    initonomatopoeia();
    if (enabled)
	setaudiosystem(TRUE);
    return TRUE;
}
