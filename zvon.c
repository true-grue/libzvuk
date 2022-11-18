/* Author: Peter Sovietov */

#include <math.h>
#include <stdlib.h>
#include "zvon_platform.h"
#include "zvon.h"

double midi_note(int m) {
    return 440 * pow(2, (m - 69) * (1 / 12.));
}

int sec(double t) {
    return t * SR;
}

double limit(double x, double low, double high) {
    return MIN(MAX(x, low), high);
}

double lerp(double a, double b, double x) {
    return a * (1 - x) + b * x;
}

double hertz(double t, double freq) {
    return (2 * PI / SR) * freq * t;
}

double dsf(double phase, double mod, double width) {
    double mphase = mod * phase;
    double num = sin(phase) - width * sin(phase - mphase);
    return num / (1 + width * (width - 2 * cos(mphase)));
}

double dsf2(double phase, double mod, double width) {
    double mphase = mod * phase;
    double num = sin(phase) * (1 - width * width);
    return num / (1 + width * (width - 2 * cos(mphase)));
}

double saw(double phase, double width) {
    return dsf(phase, 1, width);
}

double square(double phase, double width) {
    return dsf(phase, 2, width);
}

double pwm(double phase, double offset, double width) {
    return saw(phase, width) - saw(phase + offset, width);
}

unsigned int lfsr(unsigned int state, int bits, int *taps, int taps_size) {
    unsigned int x = 0;
    for (int i = 0; i < taps_size; i++) {
        x ^= state >> taps[i];
    }
    return (state >> 1) | ((~x & 1) << (bits - 1));
}

void phasor_init(struct phasor_state *s) {
    s->phase = 0;
}

void phasor_reset(struct phasor_state *s) {
    s->phase = 0;
}

double phasor_next(struct phasor_state *s, double freq) {
    double p = s->phase;
    s->phase = fmod(s->phase + (2 * PI / SR) * freq, SR * PI);
    return p;
}

void env_init(struct env_state *s, int *deltas, double level_0, double *levels, int size) {
    s->deltas = deltas;
    s->levels = levels;
    s->size = size;
    s->is_loop = 0;
    s->is_full_reset = 0;
    s->sustain_pos = -1;
    s->level_0 = level_0;
    s->level = level_0;
    env_reset(s);
}

void env_reset(struct env_state *s) {
    s->is_end = 0;
    s->t = 0;
    s->pos = 0;
    s->t_at_pos = 0;
    if (s->is_full_reset) {
        s->level = s->level_0;
    }
    s->level_at_pos = s->level;
}

static double env_next_tail(struct env_state *s, env_func func) {
    double dt = s->deltas[s->pos];
    if (s->t < s->t_at_pos + dt) {
        s->level = func(s->level_at_pos, s->levels[s->pos], (s->t - s->t_at_pos) / dt);
        s->t++;
        return s->level;
    }
    if (s->sustain_pos == s->pos) {
        return s->level;
    }
    s->t_at_pos += dt;
    s->level_at_pos = s->levels[s->pos];
    s->pos++;
    return s->level;
}

double env_next_head(struct env_state *s, env_func func) {
    if (s->pos >= s->size) {
        if (!s->is_loop) {
            s->is_end = 1;
            return s->level;
        }
        env_reset(s);
    }
    return env_next_tail(s, func);
}

static double step(double a, double b, double x) {
    (void) a;
    (void) x;
    return b;
}

double env_next(struct env_state *s) {
    return env_next_head(s, lerp);
}

double seq_next(struct env_state *s) {
    return env_next_head(s, step);
}

void delay_init(struct delay_state *s, int size, double level, double fb) {
    for (int i = 0; i < size; i++) {
        s->buf[i] = 0;
    }
    s->size = size;
    s->level = level;
    s->fb = fb;
    s->pos = 0;
}

double delay_next(struct delay_state *s, double x) {
    double y = x + s->buf[s->pos] * s->level;
    s->buf[s->pos] = s->buf[s->pos] * s->fb + x;
    s->pos = (s->pos + 1) % s->size;
    return y;
}

void filter_init(struct filter_state *s) {
    s->y = 0;
}

double filter_lp_next(struct filter_state *s, double x, double width) {
    s->y += width * (x - s->y);
    return s->y;
}

double filter_hp_next(struct filter_state *s, double x, double width) {
    return x - filter_lp_next(s, x, width);
}

void glide_init(struct glide_state *s, double source, double rate) {
    s->source = source;
    s->rate = rate * (1. / SR);
}

double glide_next(struct glide_state *s, double target) {
    double step = s->rate * fabs(s->source - target) + 1e-6;
    if (s->source < target) {
        s->source = MIN(s->source + step, target);
    } else {
        s->source = MAX(s->source - step, target);
    }
    return s->source;
}

void noise_init(struct noise_state *s, int bits, int *taps, int taps_size) {
    s->bits = bits;
    for (int i = 0; i < taps_size; i++) {
        s->taps[i] = taps[i];
    }
    s->taps_size = taps_size;
    s->state = 1;
    s->phase = 0;
}

double noise_next(struct noise_state *s, double freq) {
    s->phase += freq * (1. / SR);
    if (s->phase >= 1) {
        s->phase -= 1;
        s->state = lfsr(s->state, s->bits, s->taps, s->taps_size);
    }
    return 2 * (s->state & 1) - 1;
}

void chan_init(struct chan_state *c) {
    chan_set(c, 0, 0, 0);
    c->stack_depth = 0;
}

void chan_set(struct chan_state *c, int is_on, double vol, double pan) {
    c->is_on = is_on;
    c->vol = vol;
    c->pan = pan;
}

void chan_free(struct chan_state *c) {
    for (int i = 0; i < c->stack_depth; i++) {
        free(c->stack[i].state);
    }
    c->stack_depth = 0;
}

void chan_push(struct chan_state *c, struct box_def *def) {
    struct box_state *box = &c->stack[c->stack_depth];
    box->change = def->change;
    box->next = def->next;
    box->state = calloc(1, def->state_size);
    def->init(box->state);
    c->stack_depth++;
}

static double chan_process(struct box_state *stack, int stack_depth) {
    double y = 0;
    for (int i = 0; i < stack_depth; i++) {
        y = stack[i].next(stack[i].state, y);
    }
    return y;
}

void chan_mix(struct chan_state *channels, int num_channels, double vol, double *samples, int num_samples) {
    for (; num_samples; num_samples--, samples += 2) {
        double left = 0, right = 0;
        for (int i = 0; i < num_channels; i++) {
            struct chan_state *chan = &channels[i];
            if (chan->is_on) {
                double y = chan_process(chan->stack, chan->stack_depth);
                double pan = (chan->pan + 1) * 0.5;
                left += y * (1 - pan);
                right += y * pan;
            }
        }
        samples[0] = vol * left;
        samples[1] = vol * right;
    }
}
