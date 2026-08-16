#include "sound.h"
#include "string.h"
#include "heap.h"
#include "io.h"
#include "kernel.h"
#include "pic.h"

#define VOL_DB(hw) ((hw) * 1.5f)
#define VOL_HW(db) ((db) / 1.5f)

#define CHUNK_SAMPLES 0xFFFE

#define RESET_POLL 0x2
#define RESET_INT 0x3

#define NAM_RESET 0x0
#define NAM_MAS_VOL 0x2
#define NAM_PCM_VOL 0x18
#define NAM_PCM_SR 0x2C

#define NABM_GCR 0x2C
#define NABM_CTRL 0x1B
#define NABM_PCM_CTRL 0x16
#define NABM_GSR 0x30
#define NABM_BDBAR 0x10
#define NABM_LVE 0x15

pci_device_t *sound_device = NULL;
int sound_irq = 0;
static uint16_t nam = 0;
static uint16_t nabm = 0;

static int16_t *pcm_buff = NULL;
static uint32_t pcm_pos = 0;
static uint32_t pcm_len = 0;

static bdl_entry_t bdl[2];
static int bdl_current = 0;

int sound_init() {
	for (int x = 0; x < PCI_MAX_BUS; x++) {
		for (int y = 0; y < PCI_MAX_DEV; y++) {
			pci_device_t dev;

			if (pci_get_device(&dev, x, y) && dev.class_code == 0x04 && dev.subclass == 0x01) {
				sound_device = heap_alloc(sizeof(pci_device_t));
				memcpy(sound_device, &dev, sizeof(pci_device_t));
			}
		}
	}

	if (!sound_device)
		return 0;

	uint32_t io = pci_device_read(sound_device, 0, PCI_REG_IO);
	io |= (1 << 0);
	io |= (1 << 2);
	pci_device_write(sound_device, io, 0, PCI_REG_IO);

	nam = pci_device_read(sound_device, 0, PCI_REG_BAR(0)) & 0xFFFC;
	nabm = pci_device_read(sound_device, 0, PCI_REG_BAR(1)) & 0xFFFC;

	outl(nabm + NABM_GCR, 0x0);
	outl(nabm + NABM_GCR, RESET_INT);
	sound_reset();

	log("[ INFO ] PCM output channels: ");
	uint8_t channels = (inl(nabm + NABM_GCR) >> 20) & 0x03;
	if (channels == 0)
		log("2 channels\n");
	else if (channels == 1)
		log("4 channels\n");
	else if (channels == 2)
		log("6 channels\n");

	log("[ INFO ] PCM output samples: ");
	uint8_t samples = (inl(nabm + NABM_GCR) >> 22) & 0x03;
	if (samples == 0)
		log("16 samples\n");
	else if (samples == 1)
		log("20 samples\n");

	outw(nam + NAM_PCM_VOL, 0);
	outw(nam + NAM_PCM_SR, 48000);
	sound_set_volume(0, 0);

	sound_irq = pci_device_read(sound_device, 0, PCI_REG_INT) & 0xFF;
	pic_unmask(sound_irq);

	return 1;
}

int sound_reset() {
	if (!sound_device) return 0;

	outw(nam + NAM_RESET, 1);
	return 1;
}

int sound_get_volume(float *left, float *right) {
	if (!sound_device) return 0;

	uint16_t volume = inw(nam + NAM_MAS_VOL);

	*left = VOL_DB((volume >> 8) & 0x1F);
	*right = VOL_DB(volume & 0x1F);
	return 1;
}

int sound_set_volume(float left, float right) {
	if (!sound_device) return 0;

	uint16_t volume = inw(nam + NAM_MAS_VOL);

	volume &= ~(1 << 15);
	volume &= ~(0x1F << 8);
	volume &= ~0x1F;

	volume |= ((uint8_t)VOL_HW(left) & 0x1F) << 8;
	volume |= (uint8_t)VOL_HW(right) & 0x1F;

	outw(nam + NAM_MAS_VOL, volume);
	return 1;
}

int sound_play(uint16_t *buffer, uint32_t samples) {
	if (!sound_device) return 0;

	bdl[0].addr = (uint32_t)buffer;
	bdl[0].samples = samples;
	bdl[0].flags = (1 << 15);

	outb(nabm + NABM_CTRL, 0x2);
	outl(nabm + NABM_BDBAR, (uint32_t)bdl);
	outb(nabm + NABM_LVE, 0);
	outb(nabm + NABM_CTRL, 0x1 | (1 << 3));
	return 1;
}

int sound_play_pcm(int16_t *buffer, uint32_t samples) {
	if (!sound_device) return 0;

	uint32_t chunk = samples > CHUNK_SAMPLES ? CHUNK_SAMPLES : samples;
	pcm_buff = buffer;
	pcm_pos = chunk;
	pcm_len = samples;

	sound_play((uint16_t*)buffer, chunk);
	return 1;
}

void sound_handle() {
	outw(nabm + NABM_PCM_CTRL, 0x1C);

	if (!pcm_buff || pcm_pos >= pcm_len) {
		if (pcm_buff) {
			heap_free(pcm_buff);
			pcm_buff = NULL;
		}
		return;
	}
	bdl_current = !bdl_current;

	uint32_t chunk = (pcm_len - pcm_pos) > CHUNK_SAMPLES ? CHUNK_SAMPLES : (pcm_len - pcm_pos);
	bdl[bdl_current].addr = (uint32_t)(pcm_buff + pcm_pos);
	bdl[bdl_current].samples = chunk;
	bdl[bdl_current].flags = (1 << 15);
	pcm_pos += chunk;

	outb(nabm + NABM_LVE, bdl_current);
}
