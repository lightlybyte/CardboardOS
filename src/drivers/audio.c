// drivers/audio.c - Audio Driver Implementation
#include "audio.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04

// Maximum audio devices
#define MAX_AUDIO_DEVICES 8// drivers/audio.c - Audio Driver Implementation
#include "audio.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04

// Maximum audio devices
#define MAX_AUDIO_DEVICES 8

// Audio device list
static audio_device_t audio_devices[MAX_AUDIO_DEVICES];
static int audio_device_count = 0;

// Sound buffer
static uint8_t sound_buffer[65536];

// AC97 Vendor IDs
#define AC97_VENDOR_INTEL       0x8086
#define AC97_VENDOR_REALTEK     0x10EC
#define AC97_VENDOR_VIA         0x1106
#define AC97_VENDOR_SIS         0x1039
#define AC97_VENDOR_ALI         0x10B9
#define AC97_VENDOR_AMD         0x1022
#define AC97_VENDOR_NVIDIA      0x10DE
#define AC97_VENDOR_ATI         0x1002

// Convert number to string
static void uint32_to_str(uint32_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[16];
    int idx = 0;
    while (num > 0) {
        temp[idx++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (int i = 0; i < idx; i++) {
        str[i] = temp[idx - 1 - i];
    }
    str[idx] = '\0';
}

static void uint16_to_hex(uint16_t num, char* str) {
    const char* hex = "0123456789ABCDEF";
    int idx = 0;
    
    int started = 0;
    for (int i = 12; i >= 0; i -= 4) {
        uint8_t digit = (num >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            str[idx++] = hex[digit];
            started = 1;
        }
    }
    if (!started) {
        str[idx++] = '0';
    }
    str[idx] = '\0';
}

// AC97 Functions
static void ac97_write_reg(uint16_t io_base, uint8_t reg, uint16_t value) {
    // Wait for ready
    while (inb(io_base + 0x2C) & 0x01);
    
    // Write register
    outb(io_base + 0x2C, reg);
    outb(io_base + 0x2E, value & 0xFF);
    outb(io_base + 0x2F, (value >> 8) & 0xFF);
}

static uint16_t ac97_read_reg(uint16_t io_base, uint8_t reg) {
    // Wait for ready
    while (inb(io_base + 0x2C) & 0x01);
    
    // Read register
    outb(io_base + 0x2C, reg);
    
    // Wait for valid data
    while (!(inb(io_base + 0x2C) & 0x02));
    
    return inb(io_base + 0x2E) | (inb(io_base + 0x2F) << 8);
}

static void ac97_reset(uint16_t io_base) {
    // Reset AC97
    ac97_write_reg(io_base, AC97_RESET, 0x0000);
    
    // Wait for reset
    for (volatile int i = 0; i < 1000; i++);
}

static void ac97_set_rate(uint16_t io_base, uint32_t rate) {
    ac97_write_reg(io_base, AC97_PCM_FRONT_DAC_RATE, (uint16_t)rate);
    ac97_write_reg(io_base, AC97_PCM_LR_ADC_RATE, (uint16_t)rate);
}

static void ac97_set_volume(uint16_t io_base, uint8_t volume) {
    uint16_t vol = 0x1F - (volume * 0x1F / 100);
    uint16_t val = (vol << 8) | vol;
    ac97_write_reg(io_base, AC97_MASTER_VOLUME, val);
    ac97_write_reg(io_base, AC97_PCM_OUT_VOLUME, val);
}

// HDA Functions
static void hda_write_reg(uint32_t mmio_base, uint32_t reg, uint32_t value) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    *addr = value;
}

static uint32_t hda_read_reg(uint32_t mmio_base, uint32_t reg) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    return *addr;
}

static void hda_reset(uint32_t mmio_base) {
    // Reset HDA controller
    uint32_t gctl = hda_read_reg(mmio_base, HDA_GCTL);
    gctl = (gctl & ~0x01) | 0x02;  // Reset
    hda_write_reg(mmio_base, HDA_GCTL, gctl);
    
    // Wait for reset
    for (volatile int i = 0; i < 100000; i++);
    
    // Take out of reset
    gctl = hda_read_reg(mmio_base, HDA_GCTL);
    gctl &= ~0x02;
    hda_write_reg(mmio_base, HDA_GCTL, gctl);
}

// Initialize AC97 device
void audio_init_ac97(audio_device_t* dev) {
    if (!dev || !dev->pci_dev) return;
    
    uint16_t io_base = 0;
    uint32_t mmio_base = 0;
    
    // Find BAR
    for (int i = 0; i < 6; i++) {
        if (dev->pci_dev->bar[i]) {
            uint32_t bar = dev->pci_dev->bar[i];
            if (bar & 0x1) {
                io_base = bar & ~0x01;
                dev->ac97_io_base = io_base;
            } else {
                mmio_base = bar & ~0x0F;
                dev->mmio_base = mmio_base;
            }
        }
    }
    
    if (io_base == 0 && mmio_base == 0) {
        return;
    }
    
    dev->type = AUDIO_TYPE_AC97;
    dev->channels = AUDIO_STEREO;
    dev->sample_rate = AUDIO_RATE_44100;
    dev->format = AUDIO_FORMAT_S16_LE;
    
    // Reset AC97
    ac97_reset(io_base);
    
    // Set volume
    ac97_set_volume(io_base, 50);
    
    // Set sample rate
    ac97_set_rate(io_base, dev->sample_rate);
    
    // Read vendor ID
    dev->vendor_id = ac97_read_reg(io_base, AC97_VENDOR_ID1);
    dev->device_id = ac97_read_reg(io_base, AC97_VENDOR_ID2);
    
    dev->present = 1;
}

// Initialize HDA device
void audio_init_hda(audio_device_t* dev) {
    if (!dev || !dev->pci_dev) return;
    
    uint32_t mmio_base = 0;
    
    // Find BAR
    for (int i = 0; i < 6; i++) {
        if (dev->pci_dev->bar[i]) {
            uint32_t bar = dev->pci_dev->bar[i];
            if (!(bar & 0x1)) {
                mmio_base = bar & ~0x0F;
                dev->hda_mmio_base = mmio_base;
                break;
            }
        }
    }
    
    if (mmio_base == 0) {
        return;
    }
    
    dev->type = AUDIO_TYPE_HDA;
    dev->channels = AUDIO_STEREO;
    dev->sample_rate = AUDIO_RATE_48000;
    dev->format = AUDIO_FORMAT_S16_LE;
    
    // Reset HDA
    hda_reset(mmio_base);
    
    dev->present = 1;
}

// Detect audio devices
void audio_detect_devices(void) {
    audio_device_count = 0;
    
    // First, try to find AC97/PCIe audio devices
    for (int i = 0; i < pcie_get_device_count(); i++) {
        pci_device_t* pci_dev = pcie_get_device(i);
        if (!pci_dev) continue;
        
        // Check for audio class
        if (pci_dev->class_code == 0x04) { // Multimedia
            audio_device_t* dev = &audio_devices[audio_device_count];
            dev->pci_dev = pci_dev;
            dev->present = 0;
            
            // Check subclass
            if (pci_dev->subclass == 0x01) { // Audio
                // Could be AC97 or HDA
                if (pci_dev->vendor_id == AC97_VENDOR_INTEL ||
                    pci_dev->vendor_id == AC97_VENDOR_REALTEK ||
                    pci_dev->vendor_id == AC97_VENDOR_VIA ||
                    pci_dev->vendor_id == AC97_VENDOR_SIS ||
                    pci_dev->vendor_id == AC97_VENDOR_ALI) {
                    // Try AC97
                    audio_init_ac97(dev);
                } else {
                    // Try HDA
                    audio_init_hda(dev);
                }
                
                if (dev->present) {
                    audio_device_count++;
                    if (audio_device_count >= MAX_AUDIO_DEVICES) break;
                }
            }
        }
    }
}

// Initialize audio subsystem
void audio_init(void) {
    // Initialize PCIe first
    static int pcie_initialized = 0;
    if (!pcie_initialized) {
        pcie_init();
        pcie_initialized = 1;
    }
    
    // Detect audio devices
    audio_detect_devices();
}

// Play sound
int audio_play_sound(audio_device_t* dev, const uint8_t* data, uint32_t size, uint32_t rate) {
    if (!dev || !dev->present || !data) return -1;
    
    // This is a simplified implementation
    // In a real driver, you'd set up DMA buffers and stream the data
    
    if (dev->type == AUDIO_TYPE_AC97) {
        // For AC97, we'd write to the PCM output
        // This is simplified - just acknowledge the playback
        return 0;
    } else if (dev->type == AUDIO_TYPE_HDA) {
        // For HDA, we'd set up stream descriptors
        return 0;
    }
    
    return -1;
}

// Simple sinf approximation (since we don't have math.h)
static float sinf_approx(float x) {
    // Taylor series approximation: sin(x) = x - x^3/6 + x^5/120 - x^7/5040
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    return x - x3/6.0f + x5/120.0f - x7/5040.0f;
}

// Generate sine wave
void audio_generate_sine(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2; // 16-bit mono
    float step = 2.0f * 3.14159f * (float)frequency / (float)sample_rate;
    float phase = 0.0f;
    
    for (uint32_t i = 0; i < samples; i++) {
        int16_t sample = (int16_t)(32767 * sinf_approx(phase));
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
        phase += step;
        if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
    }
}

// Generate square wave
void audio_generate_square(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2;
    uint32_t half_period = sample_rate / (frequency * 2);
    if (half_period == 0) half_period = 1;
    
    for (uint32_t i = 0; i < samples; i++) {
        int16_t sample = ((i / half_period) % 2) ? 32767 : -32768;
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
    }
}

// Generate triangle wave
void audio_generate_triangle(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2;
    uint32_t period = sample_rate / frequency;
    if (period == 0) period = 1;
    
    for (uint32_t i = 0; i < samples; i++) {
        uint32_t pos = i % period;
        float sample_float;
        if (pos < period / 2) {
            sample_float = (float)pos / (period / 2) * 2.0f - 1.0f;
        } else {
            sample_float = 1.0f - (float)(pos - period / 2) / (period / 2) * 2.0f;
        }
        int16_t sample = (int16_t)(32767 * sample_float);
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
    }
}

// Generate sawtooth wave
void audio_generate_sawtooth(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2;
    uint32_t period = sample_rate / frequency;
    if (period == 0) period = 1;
    
    for (uint32_t i = 0; i < samples; i++) {
        float sample_float = (float)(i % period) / period * 2.0f - 1.0f;
        int16_t sample = (int16_t)(32767 * sample_float);
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
    }
}

// Play tone
int audio_play_tone(audio_device_t* dev, uint32_t frequency, uint32_t duration_ms) {
    if (!dev || !dev->present) return -1;
    
    // Generate tone
    uint32_t sample_rate = dev->sample_rate;
    uint32_t samples = sample_rate * duration_ms / 1000;
    uint32_t buffer_size = samples * 2; // 16-bit samples
    
    if (buffer_size > sizeof(sound_buffer)) {
        buffer_size = sizeof(sound_buffer);
    }
    
    audio_generate_sine(sound_buffer, buffer_size, frequency, sample_rate);
    
    return audio_play_sound(dev, sound_buffer, buffer_size, sample_rate);
}

// Set volume
void audio_set_volume(audio_device_t* dev, uint8_t volume) {
    if (!dev || !dev->present) return;
    
    if (dev->type == AUDIO_TYPE_AC97) {
        ac97_set_volume(dev->ac97_io_base, volume);
    }
}

// Stop audio
void audio_stop(audio_device_t* dev) {
    if (!dev || !dev->present) return;
    // Stop playback
}

// Get device count
int audio_get_device_count(void) {
    return audio_device_count;
}

// Get device
audio_device_t* audio_get_device(uint8_t index) {
    if (index < audio_device_count) {
        return &audio_devices[index];
    }
    return NULL;
}

// Print devices
void audio_print_devices(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Audio Devices ===\n");
    twrite("  Index  Type      Vendor:Device  Channels Rate\n");
    twrite("  --------------------------------------------\n");
    
    for (int i = 0; i < audio_device_count; i++) {
        audio_device_t* dev = &audio_devices[i];
        char idx_str[8];
        char rate_str[16];
        char vendor_str[16];
        char device_str[16];
        
        uint32_to_str(i, idx_str);
        uint32_to_str(dev->sample_rate, rate_str);
        uint16_to_hex(dev->vendor_id, vendor_str);
        uint16_to_hex(dev->device_id, device_str);
        
        tsetcolor(COLOR_WHITE);
        twrite("  ");
        twrite(idx_str);
        twrite("       ");
        
        tsetcolor(COLOR_GREEN);
        if (dev->type == AUDIO_TYPE_AC97) {
            twrite("AC97   ");
        } else if (dev->type == AUDIO_TYPE_HDA) {
            twrite("HDA    ");
        } else if (dev->type == AUDIO_TYPE_PC_SPEAKER) {
            twrite("PC Spkr");
        } else {
            twrite("Unknown");
        }
        
        tsetcolor(COLOR_YELLOW);
        twrite(" ");
        twrite(vendor_str);
        twrite(":");
        twrite(device_str);
        twrite(" ");
        
        tsetcolor(COLOR_WHITE);
        if (dev->channels == AUDIO_MONO) {
            twrite("Mono ");
        } else {
            twrite("Stereo");
        }
        twrite(" ");
        twrite(rate_str);
        twrite("\n");
    }
    
    twrite("  --------------------------------------------\n");
    char count_str[8];
    uint32_to_str(audio_device_count, count_str);
    twrite("  Total devices: ");
    twrite(count_str);
    twrite("\n");
    twrite("=========================\n");
}

// PC Speaker Functions
void audio_pc_speaker_on(uint32_t frequency) {
    // Set PIT channel 2
    outb(0x43, 0xB6);
    uint32_t divisor = 1193180 / frequency;
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);
    
    // Enable speaker
    uint8_t port = inb(0x61);
    outb(0x61, port | 0x03);
}

void audio_pc_speaker_off(void) {
    uint8_t port = inb(0x61);
    outb(0x61, port & ~0x03);
}

void audio_pc_speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    audio_pc_speaker_on(frequency);
    
    // Wait
    for (volatile uint32_t i = 0; i < duration_ms * 1000; i++);
    
    audio_pc_speaker_off();
}

// Audio device list
static audio_device_t audio_devices[MAX_AUDIO_DEVICES];
static int audio_device_count = 0;

// Sound buffer
static uint8_t sound_buffer[65536];

// AC97 Vendor IDs
#define AC97_VENDOR_INTEL       0x8086
#define AC97_VENDOR_REALTEK     0x10EC
#define AC97_VENDOR_VIA         0x1106
#define AC97_VENDOR_SIS         0x1039
#define AC97_VENDOR_ALI         0x10B9
#define AC97_VENDOR_AMD         0x1022
#define AC97_VENDOR_NVIDIA      0x10DE
#define AC97_VENDOR_ATI         0x1002

// Convert number to string
static void uint32_to_str(uint32_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[16];
    int idx = 0;
    while (num > 0) {
        temp[idx++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (int i = 0; i < idx; i++) {
        str[i] = temp[idx - 1 - i];
    }
    str[idx] = '\0';
}

static void uint16_to_hex(uint16_t num, char* str) {
    const char* hex = "0123456789ABCDEF";
    int idx = 0;
    
    int started = 0;
    for (int i = 12; i >= 0; i -= 4) {
        uint8_t digit = (num >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            str[idx++] = hex[digit];
            started = 1;
        }
    }
    if (!started) {
        str[idx++] = '0';
    }
    str[idx] = '\0';
}

// AC97 Functions
static void ac97_write_reg(uint16_t io_base, uint8_t reg, uint16_t value) {
    // Wait for ready
    while (inb(io_base + 0x2C) & 0x01);
    
    // Write register
    outb(io_base + 0x2C, reg);
    outb(io_base + 0x2E, value & 0xFF);
    outb(io_base + 0x2F, (value >> 8) & 0xFF);
}

static uint16_t ac97_read_reg(uint16_t io_base, uint8_t reg) {
    // Wait for ready
    while (inb(io_base + 0x2C) & 0x01);
    
    // Read register
    outb(io_base + 0x2C, reg);
    
    // Wait for valid data
    while (!(inb(io_base + 0x2C) & 0x02));
    
    return inb(io_base + 0x2E) | (inb(io_base + 0x2F) << 8);
}

static void ac97_reset(uint16_t io_base) {
    // Reset AC97
    ac97_write_reg(io_base, AC97_RESET, 0x0000);
    
    // Wait for reset
    for (volatile int i = 0; i < 1000; i++);
}

static void ac97_set_rate(uint16_t io_base, uint32_t rate) {
    ac97_write_reg(io_base, AC97_PCM_FRONT_DAC_RATE, rate);
    ac97_write_reg(io_base, AC97_PCM_LR_ADC_RATE, rate);
}

static void ac97_set_volume(uint16_t io_base, uint8_t volume) {
    uint16_t vol = 0x1F - (volume * 0x1F / 100);
    uint16_t val = (vol << 8) | vol;
    ac97_write_reg(io_base, AC97_MASTER_VOLUME, val);
    ac97_write_reg(io_base, AC97_PCM_OUT_VOLUME, val);
}

// HDA Functions
static void hda_write_reg(uint32_t mmio_base, uint32_t reg, uint32_t value) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    *addr = value;
}

static uint32_t hda_read_reg(uint32_t mmio_base, uint32_t reg) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    return *addr;
}

static void hda_reset(uint32_t mmio_base) {
    // Reset HDA controller
    uint32_t gctl = hda_read_reg(mmio_base, HDA_GCTL);
    gctl = (gctl & ~0x01) | 0x02;  // Reset
    hda_write_reg(mmio_base, HDA_GCTL, gctl);
    
    // Wait for reset
    for (volatile int i = 0; i < 100000; i++);
    
    // Take out of reset
    gctl = hda_read_reg(mmio_base, HDA_GCTL);
    gctl &= ~0x02;
    hda_write_reg(mmio_base, HDA_GCTL, gctl);
}

// Initialize AC97 device
void audio_init_ac97(audio_device_t* dev) {
    if (!dev || !dev->pci_dev) return;
    
    uint16_t io_base = 0;
    uint32_t mmio_base = 0;
    
    // Find BAR
    for (int i = 0; i < 6; i++) {
        if (dev->pci_dev->bar[i]) {
            uint32_t bar = dev->pci_dev->bar[i];
            if (bar & 0x1) {
                io_base = bar & ~0x01;
                dev->ac97_io_base = io_base;
            } else {
                mmio_base = bar & ~0x0F;
                dev->mmio_base = mmio_base;
            }
        }
    }
    
    if (io_base == 0 && mmio_base == 0) {
        return;
    }
    
    dev->type = AUDIO_TYPE_AC97;
    dev->channels = AUDIO_STEREO;
    dev->sample_rate = AUDIO_RATE_44100;
    dev->format = AUDIO_FORMAT_S16_LE;
    
    // Reset AC97
    ac97_reset(io_base);
    
    // Set volume
    ac97_set_volume(io_base, 50);
    
    // Set sample rate
    ac97_set_rate(io_base, dev->sample_rate);
    
    // Read vendor ID
    dev->vendor_id = ac97_read_reg(io_base, AC97_VENDOR_ID1);
    dev->device_id = ac97_read_reg(io_base, AC97_VENDOR_ID2);
    
    dev->present = 1;
}

// Initialize HDA device
void audio_init_hda(audio_device_t* dev) {
    if (!dev || !dev->pci_dev) return;
    
    uint32_t mmio_base = 0;
    
    // Find BAR
    for (int i = 0; i < 6; i++) {
        if (dev->pci_dev->bar[i]) {
            uint32_t bar = dev->pci_dev->bar[i];
            if (!(bar & 0x1)) {
                mmio_base = bar & ~0x0F;
                dev->hda_mmio_base = mmio_base;
                break;
            }
        }
    }
    
    if (mmio_base == 0) {
        return;
    }
    
    dev->type = AUDIO_TYPE_HDA;
    dev->channels = AUDIO_STEREO;
    dev->sample_rate = AUDIO_RATE_48000;
    dev->format = AUDIO_FORMAT_S16_LE;
    
    // Reset HDA
    hda_reset(mmio_base);
    
    dev->present = 1;
}

// Detect audio devices
void audio_detect_devices(void) {
    audio_device_count = 0;
    
    // First, try to find AC97/PCIe audio devices
    for (int i = 0; i < pcie_get_device_count(); i++) {
        pci_device_t* pci_dev = pcie_get_device(i);
        if (!pci_dev) continue;
        
        // Check for audio class
        if (pci_dev->class_code == 0x04) { // Multimedia
            audio_device_t* dev = &audio_devices[audio_device_count];
            dev->pci_dev = pci_dev;
            dev->present = 0;
            
            // Check subclass
            if (pci_dev->subclass == 0x01) { // Audio
                // Could be AC97 or HDA
                if (pci_dev->vendor_id == AC97_VENDOR_INTEL ||
                    pci_dev->vendor_id == AC97_VENDOR_REALTEK ||
                    pci_dev->vendor_id == AC97_VENDOR_VIA ||
                    pci_dev->vendor_id == AC97_VENDOR_SIS ||
                    pci_dev->vendor_id == AC97_VENDOR_ALI) {
                    // Try AC97
                    audio_init_ac97(dev);
                } else {
                    // Try HDA
                    audio_init_hda(dev);
                }
                
                if (dev->present) {
                    audio_device_count++;
                    if (audio_device_count >= MAX_AUDIO_DEVICES) break;
                }
            }
        }
    }
}

// Initialize audio subsystem
void audio_init(void) {
    // Initialize PCIe first
    static int pcie_initialized = 0;
    if (!pcie_initialized) {
        pcie_init();
        pcie_initialized = 1;
    }
    
    // Detect audio devices
    audio_detect_devices();
}

// Play sound
int audio_play_sound(audio_device_t* dev, const uint8_t* data, uint32_t size, uint32_t rate) {
    if (!dev || !dev->present || !data) return -1;
    
    // This is a simplified implementation
    // In a real driver, you'd set up DMA buffers and stream the data
    
    if (dev->type == AUDIO_TYPE_AC97) {
        // For AC97, we'd write to the PCM output
        // This is simplified - just acknowledge the playback
        return 0;
    } else if (dev->type == AUDIO_TYPE_HDA) {
        // For HDA, we'd set up stream descriptors
        return 0;
    }
    
    return -1;
}

// Generate sine wave
void audio_generate_sine(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2; // 16-bit mono
    float step = 2.0f * 3.14159f * frequency / sample_rate;
    float phase = 0.0f;
    
    for (uint32_t i = 0; i < samples; i++) {
        int16_t sample = (int16_t)(32767 * sinf(phase));
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
        phase += step;
        if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
    }
}

// Simple sinf approximation (since we don't have math.h)
static float sinf(float x) {
    // Taylor series approximation: sin(x) = x - x^3/6 + x^5/120 - x^7/5040
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    return x - x3/6.0f + x5/120.0f - x7/5040.0f;
}

// Generate square wave
void audio_generate_square(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2;
    uint32_t half_period = sample_rate / (frequency * 2);
    if (half_period == 0) half_period = 1;
    
    for (uint32_t i = 0; i < samples; i++) {
        int16_t sample = ((i / half_period) % 2) ? 32767 : -32768;
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
    }
}

// Generate triangle wave
void audio_generate_triangle(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2;
    uint32_t period = sample_rate / frequency;
    if (period == 0) period = 1;
    
    for (uint32_t i = 0; i < samples; i++) {
        uint32_t pos = i % period;
        float sample_float;
        if (pos < period / 2) {
            sample_float = (float)pos / (period / 2) * 2.0f - 1.0f;
        } else {
            sample_float = 1.0f - (float)(pos - period / 2) / (period / 2) * 2.0f;
        }
        int16_t sample = (int16_t)(32767 * sample_float);
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
    }
}

// Generate sawtooth wave
void audio_generate_sawtooth(uint8_t* buffer, uint32_t size, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples = size / 2;
    uint32_t period = sample_rate / frequency;
    if (period == 0) period = 1;
    
    for (uint32_t i = 0; i < samples; i++) {
        float sample_float = (float)(i % period) / period * 2.0f - 1.0f;
        int16_t sample = (int16_t)(32767 * sample_float);
        buffer[i*2] = sample & 0xFF;
        buffer[i*2+1] = (sample >> 8) & 0xFF;
    }
}

// Play tone
int audio_play_tone(audio_device_t* dev, uint32_t frequency, uint32_t duration_ms) {
    if (!dev || !dev->present) return -1;
    
    // Generate tone
    uint32_t sample_rate = dev->sample_rate;
    uint32_t samples = sample_rate * duration_ms / 1000;
    uint32_t buffer_size = samples * 2; // 16-bit samples
    
    if (buffer_size > sizeof(sound_buffer)) {
        buffer_size = sizeof(sound_buffer);
    }
    
    audio_generate_sine(sound_buffer, buffer_size, frequency, sample_rate);
    
    return audio_play_sound(dev, sound_buffer, buffer_size, sample_rate);
}

// Set volume
void audio_set_volume(audio_device_t* dev, uint8_t volume) {
    if (!dev || !dev->present) return;
    
    if (dev->type == AUDIO_TYPE_AC97) {
        ac97_set_volume(dev->ac97_io_base, volume);
    }
}

// Stop audio
void audio_stop(audio_device_t* dev) {
    if (!dev || !dev->present) return;
    // Stop playback
}

// Get device count
int audio_get_device_count(void) {
    return audio_device_count;
}

// Get device
audio_device_t* audio_get_device(uint8_t index) {
    if (index < audio_device_count) {
        return &audio_devices[index];
    }
    return NULL;
}

// Print devices
void audio_print_devices(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Audio Devices ===\n");
    twrite("  Index  Type      Vendor:Device  Channels Rate\n");
    twrite("  --------------------------------------------\n");
    
    for (int i = 0; i < audio_device_count; i++) {
        audio_device_t* dev = &audio_devices[i];
        char idx_str[8];
        char rate_str[16];
        char vendor_str[16];
        char device_str[16];
        
        uint32_to_str(i, idx_str);
        uint32_to_str(dev->sample_rate, rate_str);
        uint16_to_hex(dev->vendor_id, vendor_str);
        uint16_to_hex(dev->device_id, device_str);
        
        tsetcolor(COLOR_WHITE);
        twrite("  ");
        twrite(idx_str);
        twrite("       ");
        
        tsetcolor(COLOR_GREEN);
        if (dev->type == AUDIO_TYPE_AC97) {
            twrite("AC97   ");
        } else if (dev->type == AUDIO_TYPE_HDA) {
            twrite("HDA    ");
        } else if (dev->type == AUDIO_TYPE_PC_SPEAKER) {
            twrite("PC Spkr");
        } else {
            twrite("Unknown");
        }
        
        tsetcolor(COLOR_YELLOW);
        twrite(" ");
        twrite(vendor_str);
        twrite(":");
        twrite(device_str);
        twrite(" ");
        
        tsetcolor(COLOR_WHITE);
        if (dev->channels == AUDIO_MONO) {
            twrite("Mono ");
        } else {
            twrite("Stereo");
        }
        twrite(" ");
        twrite(rate_str);
        twrite("\n");
    }
    
    twrite("  --------------------------------------------\n");
    char count_str[8];
    uint32_to_str(audio_device_count, count_str);
    twrite("  Total devices: ");
    twrite(count_str);
    twrite("\n");
    twrite("=========================\n");
}

// PC Speaker Functions
void audio_pc_speaker_on(uint32_t frequency) {
    // Set PIT channel 2
    outb(0x43, 0xB6);
    uint32_t divisor = 1193180 / frequency;
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);
    
    // Enable speaker
    uint8_t port = inb(0x61);
    outb(0x61, port | 0x03);
}

void audio_pc_speaker_off(void) {
    uint8_t port = inb(0x61);
    outb(0x61, port & ~0x03);
}

void audio_pc_speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    audio_pc_speaker_on(frequency);
    
    // Wait
    for (volatile uint32_t i = 0; i < duration_ms * 1000; i++);
    
    audio_pc_speaker_off();
}