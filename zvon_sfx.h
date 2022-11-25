/* Author: Peter Sovietov */

#ifndef ZVON_SFX_H
#define ZVON_SFX_H

#include "zvon.h"

enum {
    ZV_NOTE_ON,
    ZV_NOTE_OFF,
    ZV_VOLUME,
    ZV_DELAY_TIME,
    ZV_DELAY_FEEDBACK,
    ZV_GAIN,
    ZV_WAVE_TYPE,
    ZV_ATTACK_TIME,
    ZV_DECAY_TIME,
    ZV_SUSTAIN_LEVEL,
    ZV_RELEASE_TIME,
    ZV_GLIDE_MODE,
    ZV_GLIDE_RATE,
    ZV_END
};

extern struct sfx_proto sfx_synth;
extern struct sfx_proto sfx_delay;
extern struct sfx_proto sfx_dist;

#endif
