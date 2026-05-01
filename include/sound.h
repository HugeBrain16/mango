#ifndef SOUND_H
#define SOUND_H

#include "pci.h"

typedef struct {
	uint32_t addr;
	uint16_t samples;
	uint16_t flags;
} bdl_entry_t;

extern pci_device_t *sound_device;
extern int sound_irq;

extern int sound_init();
extern int sound_reset();
extern int sound_get_volume(float *left, float *right);
extern int sound_set_volume(float left, float right);
extern int sound_play(uint16_t *buffer, uint32_t samples);
extern int sound_play_pcm(int16_t *buffer, uint32_t samples);
extern void sound_handle();

#endif