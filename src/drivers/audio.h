// drivers/audio.h - Audio Driver Interface
#ifndef _AUDIO_H
#define _AUDIO_H

#include "pcie.h"

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Basic types - complete definitions
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// Audio Device Types
#define AUDIO_TYPE_PC_SPEAKER  0x01
#define AUDIO_TYPE_AC97        0x02
#define AUDIO_TYPE_HDA         0x03  // High Definition Audio
#define AUDIO_TYPE_SB16        0x04  // Sound Blaster 16

// Audio Sample Rates
#define AUDIO_RATE_8000        8000
#define AUDIO_RATE_11025       11025
#define AUDIO_RATE_22050       22050
#define AUDIO_RATE_44100       44100
#define AUDIO_RATE_48000       48000

// Audio Sample Formats
#define AUDIO_FORMAT_U8        0x01
#define AUDIO_FORMAT_S8        0x02
#define AUDIO_FORMAT_U16_LE    0x03
#define AUDIO_FORMAT_S16_LE    0x04
#define AUDIO_FORMAT_U16_BE    0x05
#define AUDIO_FORMAT_S16_BE    0x06
#define AUDIO_FORMAT_U32_LE    0x07
#define AUDIO_FORMAT_S32_LE    0x08

// Audio Channels
#define AUDIO_MONO             1
#define AUDIO_STEREO           2

// AC97 Registers
#define AC97_RESET             0x00
#define AC97_MASTER_VOLUME     0x02
#define AC97_HEADPHONE_VOLUME  0x04
#define AC97_MASTER_VOLUME_MONO 0x06
#define AC97_PHONE_VOLUME      0x08
#define AC97_MIC_VOLUME        0x0A
#define AC97_LINEIN_VOLUME     0x0C
#define AC97_CD_VOLUME         0x0E
#define AC97_VIDEO_VOLUME      0x10
#define AC97_AUX_VOLUME        0x12
#define AC97_PCM_OUT_VOLUME    0x14
#define AC97_RECORD_SELECT     0x1A
#define AC97_RECORD_GAIN       0x1C
#define AC97_RECORD_GAIN_MIC   0x1E
#define AC97_GENERAL_PURPOSE   0x20
#define AC97_3D_CONTROL        0x22
#define AC97_POWERDOWN         0x26
#define AC97_EXT_AUDIO_ID      0x28
#define AC97_EXT_AUDIO_CTRL    0x2A
#define AC97_PCM_FRONT_DAC_RATE 0x2C
#define AC97_PCM_SURR_DAC_RATE 0x2E
#define AC97_PCM_LFE_DAC_RATE  0x30
#define AC97_PCM_LR_ADC_RATE   0x32
#define AC97_VENDOR_ID1        0x7C
#define AC97_VENDOR_ID2        0x7E

// HDA Registers
#define HDA_GCTL               0x00
#define HDA_WAKEEN             0x04
#define HDA_STATESTS           0x08
#define HDA_LLCH               0x0C
#define HDA_OUTPAY             0x18
#define HDA_INPAY              0x1C

// Audio Device Structure
typedef struct {
    uint8_t type;
    uint8_t present;
    uint8_t channels;
    uint32_t sample_rate;
    uint8_t format;
    uint16_t vendor_id;
    uint16_t device_id;
    
    // PCIe device info
    pci_device_t* pci_dev;
    uint32_t bar_address;
    uint32_t bar_size;
    uint16_t io_base;
    uint32_t mmio_base;
    
    // For AC97
    uint16_t ac97_io_base;
    
    // For HDA
    uint32_t hda_mmio_base;
    uint32_t hda_azalia_base;
} audio_device_t;

// Audio Driver Functions
void audio_init(void);
void audio_detect_devices(void);
void audio_init_ac97(audio_device_t* dev);
void audio_init_hda(audio_device_t* dev);
int audio_play_sound(audio_device_t* dev, const uint8_t* data, uint32_t size, uint32_t rate);
int audio_play_tone(audio_device_t* dev, uint32_t frequency, uint32_t duration_ms);
void audio_stop(audio_device_t* dev);
void audio_set_volume(audio_device_t* dev, uint8_t volume);
void audio_print_devices(void);
audio_device_t* audio_get_device(uint8_t index);
int audio_get_device_count(void);

// Sound Generation
void audio_generate_sine(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate);
void audio_generate_square(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate);
void audio_generate_triangle(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate);
void audio_generate_sawtooth(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate);

// PC Speaker (legacy)
void audio_pc_speaker_beep(uint32_t frequency, uint32_t duration_ms);
void audio_pc_speaker_on(uint32_t frequency);
void audio_pc_speaker_off(void);

#endif // _AUDIO_H