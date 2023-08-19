/* Author: Peter Sovietov */

#include <math.h>
#include <stdlib.h>
#include "zvon_mixer.h"

void sfx_box_set_vol(struct sfx_box *box, double vol) {
    box->vol = vol;
}

void chan_set_on(struct chan_state *c, int is_on) {
    c->is_on = is_on;
}

void chan_set_vol(struct chan_state *c, double vol) {
    c->vol = vol;
}

void chan_set_pan(struct chan_state *c, double pan) {
    pan = (pan + 1) * 0.5;
    c->pan_left = sqrt(1 - pan);
    c->pan_right = sqrt(pan);
}

void chan_drop(struct chan_state *c) {
    for (int i = 0; i < c->stack_size; i++) {
        if (c->stack[i].proto->free) {
            c->stack[i].proto->free(c->stack[i].state);
        }
        free(c->stack[i].state);
        c->stack[i].state = NULL;
    }
    c->stack_size = 0;
}

struct sfx_box *chan_push(struct chan_state *c, struct sfx_proto *proto) {
    if (c->stack_size < SFX_MAX_BOXES) {
        struct sfx_box *box = &c->stack[c->stack_size];
        box->proto = proto;
        box->state = calloc(1, proto->state_size);
        if (box->state || !box->proto->state_size) {
            proto->init(box->state);
            sfx_box_set_vol(box, 1);
            c->stack_size++;
            return box;
        }
    }
    return NULL;
}

static void chan_process(struct sfx_box *stack, int stack_size, double *l, double *r) {
    for (int i = 0; i < stack_size; i++) {
        struct sfx_box *box = &stack[i];
        if (box->proto->stereo) {
            box->proto->stereo(box->state, l, r);
        } else {
            *l = box->proto->mono(box->state, *l);
            *r = *l;
        }
        *l *= box->vol;
        *r *= box->vol;
    }
}

void mix_init(struct chan_state *chans, int num_chans) {
    for (int i = 0; i < num_chans; i++) {
        struct chan_state *c = &chans[i];
        chan_set_on(c, 0);
        chan_set_vol(c, 0);
        chan_set_pan(c, 0);
        c->stack_size = 0;
    }
}
double mix_process(struct chan_state *chans, int num_chans, double vol, float *samps, int num_samps) {
    double max_sample = 0;
    for (; num_samps; num_samps--, samps += 2) {
        double left = 0, right = 0;
        for (int i = 0; i < num_chans; i++) {
            struct chan_state *c = &chans[i];
            if (c->is_on) {
                double l = 0, r = 0;
                chan_process(c->stack, c->stack_size, &l, &r);
                left += c->vol * l * c->pan_left;
                right += c->vol * r * c->pan_right;
            }
        }
        samps[0] = vol * left;
        samps[1] = vol * right;
        max_sample = MAX(max_sample, MAX(fabs(samps[0]), fabs(samps[1])));
    }
    return max_sample;
}

void sfx_box_change(struct sfx_box *box, int param, int elem, double val) {
    switch (param) {
    case SFX_BOX_VOLUME:
        sfx_box_set_vol(box, val);
        break;
    default:
        if (box->state) {
            box->proto->change(box->state, param, elem, val);
        }
    }
}

void chan_change(struct chan_state *c, int param, int elem, double val) {
    for (int i = 0; i < c->stack_size; i++) {
        sfx_box_change(&c->stack[i], param, elem, val);
    }
}
