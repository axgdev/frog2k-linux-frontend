// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "libretro_min.h"
#include "ge_api.h"
#include "hc15xx_resampler.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <limits.h>
#include <signal.h>
#include <sound/asound.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#ifdef __mips__
#include <sys/cachectl.h>
extern int cacheflush(void *address, int bytes, int cache);
#endif

#define READY_MARKER "/run/sf2000-frontend-ready"
#define ACTIVE_MARKER "/run/sf2000-frontend-active"
#define AUDIO_BUFFER_SAMPLES 4096u
#define AUDIO_DROP_SAMPLES 1024u

struct host {
	int fb_fd, input_fd;
	uint16_t *fb;
	size_t fb_bytes;
	unsigned fb_width, fb_height, fb_stride;
	uint32_t fb_phys;
	hcge_context ge_storage;
	hcge_context *ge;
	uint16_t *ge_source[2];
	uint32_t ge_source_phys[2], ge_source_handle[2];
	size_t ge_source_bytes;
	unsigned ge_width, ge_height, ge_buffers, ge_next, ge_pending;
	int pcm_fd;
	unsigned audio_rate, audio_head, audio_count;
	struct hc15xx_resampler resampler;
	int16_t audio_buffer[AUDIO_BUFFER_SAMPLES];
	uint32_t keys;
	enum retro_pixel_format format;
	double fps;
	const char *system_dir;
	const char *save_dir;
};

static struct host host = { .fb_fd = -1, .input_fd = -1, .pcm_fd = -1,
	.format = RETRO_PIXEL_FORMAT_0RGB1555, .fps = 60.0 };
static volatile sig_atomic_t stopping;
static int first_frame;
static unsigned video_callbacks;
static struct timespec metrics_start;
static int kmsg_fd = -1;
static int metrics_logging;
extern uint32_t reg[] __attribute__((weak));
static struct {
	unsigned generated, submitted, dropped;
	unsigned peak, eagain, xruns;
} audio_metrics;

static void mark_active(void)
{
	char text[24];
	int active = open(ACTIVE_MARKER,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	int length;

	if (active < 0)
		return;
	length = snprintf(text, sizeof(text), "%ld\n", (long)getpid());
	if (length > 0)
		(void)write(active, text, (size_t)length);
	close(active);
}

static void log_kmsg(const char *message)
{
	char line[384];
	int fd;
	int length;

	if (metrics_logging) {
		if (kmsg_fd < 0)
			kmsg_fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
		fd = kmsg_fd;
	} else {
		fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	}
	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line), "<6>sf2000-frontend: %s", message);
	if (length > 0)
		(void)write(fd, line, (size_t)length);
	if (!metrics_logging)
		close(fd);
}

static void start_metrics_logging(void)
{
	/*
	 * Linux rate-limits writes per open /dev/kmsg file.  Keep startup
	 * messages on short-lived files, then retain one low-rate descriptor
	 * for the timed loop so metric collection cannot add open/close stalls.
	 */
	metrics_logging = 1;
	kmsg_fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
}

void unifrog_core_load_progress(const char *stage, unsigned current,
	unsigned total)
{
	char message[160];
	snprintf(message, sizeof(message), "load %s %u/%u\n", stage, current,
		total);
	log_kmsg(message);
}

static void stop_signal(int signal_number)
{
	(void)signal_number;
	stopping = 1;
}

static char *append_hex(char *output, unsigned long long value)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	*output++ = '0';
	*output++ = 'x';
	for (shift = 60; shift >= 0; shift -= 4)
		*output++ = digits[(value >> shift) & 15];
	return output;
}

static void fault_signal(int signal_number, siginfo_t *info, void *context)
{
	char message[320];
	ucontext_t *machine = context;
	char *cursor = message;
	unsigned long long pc = machine ? machine->uc_mcontext.pc : 0;
	unsigned long long cause = 0, badvaddr = 0, sp = 0, ra = 0;
	int fd;

#ifdef __mips__
	if (machine) {
		cause = machine->uc_mcontext.hi1;
		badvaddr = machine->uc_mcontext.lo1;
		sp = machine->uc_mcontext.gregs[29];
		ra = machine->uc_mcontext.gregs[31];
	}
#endif
	memcpy(cursor, "<3>sf2000-frontend: fatal signal=", 33);
	cursor += 33;
	*cursor++ = (char)('0' + signal_number / 10);
	*cursor++ = (char)('0' + signal_number % 10);
	memcpy(cursor, " pc=", 4);
	cursor += 4;
	cursor = append_hex(cursor, pc);
	memcpy(cursor, " insn=", 6);
	cursor += 6;
	/*
	 * A signal handler cannot safely dereference the reported PC: SIGBUS
	 * may describe an unmapped or unaligned instruction address, and a
	 * second fault here hides the exception that we are trying to report.
	 * The kernel exception trace retains the instruction when it is safe
	 * to fetch; keep the user-space record focused on stable registers.
	 */
	cursor = append_hex(cursor, 0);
	memcpy(cursor, " cause=", 7);
	cursor += 7;
	cursor = append_hex(cursor, cause);
	memcpy(cursor, " badvaddr=", 10);
	cursor += 10;
	cursor = append_hex(cursor, badvaddr);
	memcpy(cursor, " si_addr=", 9);
	cursor += 9;
	cursor = append_hex(cursor, info ? (uintptr_t)info->si_addr : 0);
	memcpy(cursor, " si_code=", 9);
	cursor += 9;
	cursor = append_hex(cursor,
		info ? (unsigned int)info->si_code : 0);
	memcpy(cursor, " sp=", 4);
	cursor += 4;
	cursor = append_hex(cursor, sp);
	memcpy(cursor, " ra=", 4);
	cursor += 4;
	cursor = append_hex(cursor, ra);
	memcpy(cursor, " gba_pc=", 8);
	cursor += 8;
	cursor = append_hex(cursor, reg ? reg[15] : 0);
	*cursor++ = '\n';
	fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	if (fd >= 0) {
		(void)write(fd, message, (size_t)(cursor - message));
		close(fd);
	}
	_exit(128 + signal_number);
}

static void core_log(enum retro_log_level level, const char *format, ...)
{
	char message[128];
	va_list arguments;

	(void)level;
	va_start(arguments, format);
	(void)vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);
	log_kmsg(message);
}

static bool environment(unsigned command, void *data)
{
	switch (command) {
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable *variable = data;
		if (variable && variable->key &&
				strcmp(variable->key, "gpsp_drc") == 0) {
			variable->value = "enabled";
			return true;
		}
		if (variable && variable->key &&
				strcmp(variable->key, "gpsp_sound_rate") == 0) {
			/*
			 * Match the 32 kHz hardware PCM path.  Current gpSP keeps
			 * timing exact at this rate while doing half as much mixer
			 * work as its legacy 65536 Hz output.
			 */
			variable->value = "32768";
			return true;
		}
		return false;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		*(bool *)data = true;
		return true;
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
		/*
		 * Accept the structured v1 option table.  Returning false here
		 * makes cores synthesize the deprecated string table at startup,
		 * allocating and concatenating every option even though this
		 * appliance frontend selects its few platform settings directly
		 * through GET_VARIABLE.
		 */
		*(unsigned *)data = 1;
		return true;
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		host.format = *(enum retro_pixel_format *)data;
		return host.format == RETRO_PIXEL_FORMAT_RGB565 ||
			host.format == RETRO_PIXEL_FORMAT_XRGB8888;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char **)data = host.system_dir;
		return true;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		*(const char **)data = host.save_dir;
		return true;
	case RETRO_ENVIRONMENT_SET_GEOMETRY:
		return true;
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *)data)->log = core_log;
		return true;
	default:
		return false;
	}
}

static uint16_t xrgb8888_to_565(uint32_t pixel)
{
	return (uint16_t)(((pixel >> 8) & 0xf800u) |
		((pixel >> 5) & 0x07e0u) | ((pixel >> 3) & 0x001fu));
}

static uint32_t frame_hash(const void *data, unsigned height, size_t pitch)
{
	const uint32_t *words = data;
	uint32_t hash = 2166136261u;
	size_t length = (size_t)height * pitch;
	size_t i;

	for (i = 0; i < length / sizeof(*words); i++) {
		hash ^= words[i];
		hash *= 16777619u;
	}
	return hash ^ (uint32_t)length;
}

static void cpu_present(const void *data, unsigned width, unsigned height,
	size_t pitch, unsigned out_w, unsigned out_h, unsigned left, unsigned top)
{
	unsigned x_step = (width << 16) / out_w;
	unsigned y;

	memset(host.fb, 0, host.fb_bytes);
	if (host.format == RETRO_PIXEL_FORMAT_RGB565 && width == host.fb_width &&
			height == host.fb_height && pitch == host.fb_stride * 2u) {
		memcpy(host.fb, data, (size_t)height * pitch);
	} else for (y = 0; y < out_h; y++) {
		uint16_t *dst = host.fb + (top + y) * host.fb_stride + left;
		unsigned src_y = (y * ((height << 16) / out_h)) >> 16;
		unsigned x;

		if (host.format == RETRO_PIXEL_FORMAT_RGB565 && out_w == width) {
			memcpy(dst, (const uint8_t *)data + src_y * pitch,
				out_w * sizeof(*dst));
			continue;
		}
		for (x = 0; x < out_w; x++) {
			unsigned src_x = (x * x_step) >> 16;
			if (host.format == RETRO_PIXEL_FORMAT_RGB565)
				dst[x] = *(const uint16_t *)((const uint8_t *)data +
					src_y * pitch + src_x * 2u);
			else
				dst[x] = xrgb8888_to_565(*(const uint32_t *)(
					(const uint8_t *)data + src_y * pitch + src_x * 4u));
		}
	}
#ifdef __mips__
	(void)cacheflush(host.fb, (int)host.fb_bytes, BCACHE);
#endif
}

static int ge_present(const void *data, unsigned width, unsigned height,
	size_t pitch, unsigned out_w, unsigned out_h, unsigned left, unsigned top)
{
	hcge_state *state;
	HCGERectangle source;
	HCGERectangle destination;
	uint16_t *source_buffer;
	unsigned source_index;
	unsigned y;

	if (!host.ge || !host.ge_buffers || width > host.fb_width ||
			height > host.fb_height ||
			(size_t)width * height * sizeof(uint16_t) >
				host.ge_source_bytes)
		return -1;
	/*
	 * Keep one complete source surface available while the GE consumes the
	 * other.  The barrier before reuse is the ownership boundary: the CPU
	 * never modifies a surface referenced by an outstanding node.  A
	 * single-buffer allocation remains a correct synchronous fallback.
	 */
	if (host.ge_pending >= host.ge_buffers) {
		if (hcge_engine_sync(host.ge) < 0)
			return -1;
		host.ge_pending = 0;
	}
	source_index = host.ge_next;
	source_buffer = host.ge_source[source_index];
	if (!first_frame)
		log_kmsg("GE first present copy begin\n");
	if (host.format == RETRO_PIXEL_FORMAT_RGB565) {
		for (y = 0; y < height; y++)
			memcpy(source_buffer + y * width,
				(const uint8_t *)data + y * pitch,
				width * sizeof(uint16_t));
	} else {
		for (y = 0; y < height; y++) {
			const uint32_t *input = (const uint32_t *)
				((const uint8_t *)data + y * pitch);
			uint16_t *output = source_buffer + y * width;
			unsigned x;

			for (x = 0; x < width; x++)
				output[x] = xrgb8888_to_565(input[x]);
		}
	}
	if (hcge_linux_cache_clean(host.ge, source_buffer,
			width * height * sizeof(uint16_t)) < 0)
		return -1;
	if (!first_frame)
		log_kmsg("GE first present cache clean\n");
	state = &host.ge->state;
	memset(state, 0, sizeof(*state));
	state->render_options = HCGE_DSRO_NONE;
	state->drawingflags = HCGE_DSDRAW_NOFX;
	state->blittingflags = HCGE_DSBLIT_NOFX;
	state->destination.config.format = HCGE_DSPF_RGB16;
	state->destination.config.size.w = (int)host.fb_width;
	state->destination.config.size.h = (int)host.fb_height;
	state->source.config.format = HCGE_DSPF_RGB16;
	state->source.config.size.w = (int)width;
	state->source.config.size.h = (int)height;
	state->dst.phys = host.fb_phys;
	state->dst.pitch = host.fb_stride * sizeof(uint16_t);
	state->src.phys = host.ge_source_phys[source_index];
	state->src.pitch = width * sizeof(uint16_t);
	state->accel = HCGE_DFXL_STRETCHBLIT;
	hcge_set_state(host.ge, state, state->accel);
	source = (HCGERectangle){ 0, 0, (int)width, (int)height };
	destination = (HCGERectangle){ (int)left, (int)top,
		(int)out_w, (int)out_h };
	if (!hcge_stretch_blit(host.ge, &source, &destination))
		return -1;
	if (!first_frame)
		log_kmsg("GE first present submitted\n");
	host.ge_pending++;
	host.ge_next = (host.ge_next + 1u) % host.ge_buffers;
	/* Make the first frame observable before publishing READY_MARKER. */
	if (!first_frame) {
		if (hcge_engine_sync(host.ge) < 0)
			return -1;
		host.ge_pending = 0;
		log_kmsg("GE first present synchronized\n");
	}
	host.ge_width = width;
	host.ge_height = height;
	return 0;
}

static void video(const void *data, unsigned width, unsigned height,
	size_t pitch)
{
	unsigned out_w, out_h, left, top;
	uint32_t hash;

	video_callbacks++;
	if (!data || !host.fb || !width || !height) {
		if (video_callbacks == 1) {
			char details[128];
			snprintf(details, sizeof(details),
				"video callback deferred data=%u size=%ux%u\n",
				data != NULL, width, height);
			log_kmsg(details);
		}
		return;
	}
	hash = first_frame ? 0 : frame_hash(data, height, pitch);
	/*
	 * The SF2000 panel is physically rotated and its display pipeline presents
	 * the 320x240 scanout as the handheld's full viewport.  Use the complete
	 * destination.  Besides matching the requested full-screen game mode, this
	 * follows the vendor GE's proven full-surface stretch contract; a cropped
	 * destination makes the HC15xx scaler continue to the surface boundary.
	 */
	out_w = host.fb_width;
	out_h = host.fb_height;
	left = 0;
	top = 0;
	if (ge_present(data, width, height, pitch, out_w, out_h, left, top) < 0) {
		if (host.ge) {
			unsigned i;

			log_kmsg("GE present failed; using CPU renderer\n");
			(void)hcge_engine_sync(host.ge);
			for (i = 0; i < host.ge_buffers; i++) {
				(void)hcge_linux_free_buffer(host.ge,
					host.ge_source_handle[i]);
				host.ge_source[i] = NULL;
			}
			hcge_close_context(host.ge);
			host.ge = NULL;
			host.ge_buffers = 0;
			host.ge_pending = 0;
		}
		cpu_present(data, width, height, pitch, out_w, out_h, left, top);
	}
	if (!first_frame) {
		char details[160];
		int fd = open(READY_MARKER,
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

		if (fd >= 0)
			close(fd);
		snprintf(details, sizeof(details),
			"first frame %ux%u pitch=%lu format=%u hash=%08x fb=%ux%u stride=%u\n",
			width, height, (unsigned long)pitch, (unsigned)host.format, hash,
			host.fb_width, host.fb_height, host.fb_stride * 2u);
		log_kmsg(details);
		first_frame = 1;
		video_callbacks = 0;
		(void)clock_gettime(CLOCK_MONOTONIC, &metrics_start);
	} else if ((video_callbacks % 300u) == 0) {
		struct timespec now;
		unsigned long elapsed_ms;
		unsigned long fps_milli;
		char details[384];

		(void)clock_gettime(CLOCK_MONOTONIC, &now);
		elapsed_ms = (unsigned long)(now.tv_sec - metrics_start.tv_sec) *
			1000ul;
		if (now.tv_nsec >= metrics_start.tv_nsec)
			elapsed_ms += (unsigned long)
				(now.tv_nsec - metrics_start.tv_nsec) / 1000000ul;
		else
			elapsed_ms -= (unsigned long)
				(metrics_start.tv_nsec - now.tv_nsec) / 1000000ul;
		fps_milli = elapsed_ms ?
			(unsigned long)(((uint64_t)video_callbacks * 1000000ull) /
				elapsed_ms) : 0;
		snprintf(details, sizeof(details),
			"audio metric generated=%u submitted=%u dropped=%u eagain=%u xrun=%u peak=%u queued=%u frames=%u elapsed_ms=%lu fps_milli=%lu presenter=%s gba_pc=%08x\n",
			audio_metrics.generated, audio_metrics.submitted,
			audio_metrics.dropped, audio_metrics.eagain,
			audio_metrics.xruns, audio_metrics.peak,
			host.audio_count, video_callbacks, elapsed_ms, fps_milli,
			host.ge ? "GE" : "CPU", reg ? reg[15] : 0);
		log_kmsg(details);
	}
}

static struct snd_mask *pcm_param_mask(struct snd_pcm_hw_params *parameters,
	unsigned number)
{
	return &parameters->masks[number - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}

static struct snd_interval *pcm_param_interval(
	struct snd_pcm_hw_params *parameters, unsigned number)
{
	return &parameters->intervals[
		number - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

static void pcm_set_mask(struct snd_pcm_hw_params *parameters,
	unsigned number, unsigned value)
{
	struct snd_mask *mask = pcm_param_mask(parameters, number);

	memset(mask, 0, sizeof(*mask));
	mask->bits[value >> 5] = 1u << (value & 31);
}

static void pcm_set_interval(struct snd_pcm_hw_params *parameters,
	unsigned number, unsigned value)
{
	struct snd_interval *interval = pcm_param_interval(parameters, number);

	memset(interval, 0, sizeof(*interval));
	interval->min = value;
	interval->max = value;
	interval->integer = 1;
}

static int open_audio(void)
{
	struct snd_pcm_hw_params hardware;
	struct snd_pcm_sw_params software;
	unsigned i;

	host.pcm_fd = open("/dev/snd/pcmC0D0p",
		O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (host.pcm_fd < 0)
		return -1;
	memset(&hardware, 0, sizeof(hardware));
	for (i = 0; i < sizeof(hardware.masks) / sizeof(hardware.masks[0]); i++)
		memset(&hardware.masks[i], 0xff, sizeof(hardware.masks[i]));
	for (i = 0; i < sizeof(hardware.intervals) /
			sizeof(hardware.intervals[0]); i++) {
		hardware.intervals[i].max = UINT_MAX;
		hardware.intervals[i].integer = 1;
	}
	hardware.rmask = ~0u;
	pcm_set_mask(&hardware, SNDRV_PCM_HW_PARAM_ACCESS,
		SNDRV_PCM_ACCESS_RW_INTERLEAVED);
	pcm_set_mask(&hardware, SNDRV_PCM_HW_PARAM_FORMAT,
		SNDRV_PCM_FORMAT_S16_LE);
	pcm_set_mask(&hardware, SNDRV_PCM_HW_PARAM_SUBFORMAT,
		SNDRV_PCM_SUBFORMAT_STD);
	pcm_set_interval(&hardware, SNDRV_PCM_HW_PARAM_CHANNELS, 1);
	pcm_set_interval(&hardware, SNDRV_PCM_HW_PARAM_RATE, 32000);
	pcm_set_interval(&hardware, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, 1024);
	pcm_set_interval(&hardware, SNDRV_PCM_HW_PARAM_PERIODS, 8);
	pcm_set_interval(&hardware, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, 8192);
	if (ioctl(host.pcm_fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hardware) < 0)
		goto fail;
	memset(&software, 0, sizeof(software));
	software.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
	software.period_step = 1;
	software.avail_min = 1024;
	/*
	 * HC1512 SND0 must be armed when the first complete period is
	 * published.  Deferring START for several periods can leave its
	 * producer/consumer handshake stopped with a full ALSA ring: writes
	 * then return EAGAIN forever without an ALSA xrun.  The 918 MHz
	 * performance state supplies the scheduling margin that was formerly
	 * sought with a larger hardware start threshold.
	 */
	software.start_threshold = 2048;
	software.stop_threshold = 8192;
	software.boundary = 0x40000000u;
	software.proto = SNDRV_PCM_VERSION;
	if (ioctl(host.pcm_fd, SNDRV_PCM_IOCTL_SW_PARAMS, &software) < 0 ||
			ioctl(host.pcm_fd, SNDRV_PCM_IOCTL_PREPARE) < 0)
		goto fail;
	if (hc15xx_resampler_init(&host.resampler, host.audio_rate, 32000u) < 0)
		goto fail;
	log_kmsg("ALSA 32kHz mono DMA presenter ready linear-resampler\n");
	return 0;
fail:
	close(host.pcm_fd);
	host.pcm_fd = -1;
	return -1;
}

static void audio_flush(void)
{
	const unsigned capacity = sizeof(host.audio_buffer) /
		sizeof(host.audio_buffer[0]);

	if (host.pcm_fd < 0)
		return;
	while (host.audio_count) {
		unsigned contiguous = capacity - host.audio_head;
		ssize_t bytes;
		unsigned consumed;

		if (contiguous > host.audio_count)
			contiguous = host.audio_count;
		bytes = write(host.pcm_fd, host.audio_buffer + host.audio_head,
			contiguous * sizeof(host.audio_buffer[0]));
		if (bytes < 0) {
			if (errno == EPIPE) {
				(void)ioctl(host.pcm_fd, SNDRV_PCM_IOCTL_PREPARE);
				audio_metrics.xruns++;
			} else if (errno == EAGAIN)
				audio_metrics.eagain++;
			return;
		}
		consumed = (unsigned)bytes / sizeof(host.audio_buffer[0]);
		if (!consumed)
			return;
		audio_metrics.submitted += consumed;
		host.audio_head = (host.audio_head + consumed) % capacity;
		host.audio_count -= consumed;
	}
}

static size_t audio_batch(const int16_t *samples, size_t frames)
{
	size_t i;

	if (host.pcm_fd < 0 || !host.audio_rate)
		return frames;
	audio_flush();
	for (i = 0; i < frames; i++) {
		const unsigned capacity = sizeof(host.audio_buffer) /
			sizeof(host.audio_buffer[0]);
		int16_t sample;
		int emitted = hc15xx_resampler_push_stereo_s16(&host.resampler,
			samples[i * 2], samples[i * 2 + 1], &sample);
		unsigned magnitude;

		if (emitted <= 0)
			continue;
		magnitude = sample < 0 ? (unsigned)-(int)sample : (unsigned)sample;
		audio_metrics.generated++;
		if (magnitude > audio_metrics.peak)
			audio_metrics.peak = magnitude;
		if (host.audio_count == capacity)
			audio_flush();
		if (host.audio_count == capacity) {
			unsigned discard = AUDIO_DROP_SAMPLES;

			if (discard > host.audio_count)
				discard = host.audio_count;
			host.audio_head = (host.audio_head + discard) % capacity;
			host.audio_count -= discard;
			audio_metrics.dropped += discard;
		}
		host.audio_buffer[(host.audio_head + host.audio_count) % capacity] =
			sample;
		host.audio_count++;
	}
	if (host.audio_count >= 1024u)
		audio_flush();
	return frames;
}

static void audio_sample(int16_t left, int16_t right)
{
	int16_t pair[2] = { left, right };
	(void)audio_batch(pair, 1);
}

static unsigned key_bit(unsigned code)
{
	switch (code) {
	case BTN_SOUTH: return 1u << RETRO_DEVICE_ID_JOYPAD_B;
	case BTN_WEST: return 1u << RETRO_DEVICE_ID_JOYPAD_Y;
	case BTN_SELECT: return 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
	case BTN_START: return 1u << RETRO_DEVICE_ID_JOYPAD_START;
	case BTN_DPAD_UP: return 1u << RETRO_DEVICE_ID_JOYPAD_UP;
	case BTN_DPAD_DOWN: return 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
	case BTN_DPAD_LEFT: return 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
	case BTN_DPAD_RIGHT: return 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
	case BTN_EAST: return 1u << RETRO_DEVICE_ID_JOYPAD_A;
	case BTN_NORTH: return 1u << RETRO_DEVICE_ID_JOYPAD_X;
	case BTN_TL: return 1u << RETRO_DEVICE_ID_JOYPAD_L;
	case BTN_TR: return 1u << RETRO_DEVICE_ID_JOYPAD_R;
	default: return 0;
	}
}

static void input_poll(void)
{
	struct input_event event;

	while (host.input_fd >= 0 && read(host.input_fd, &event, sizeof(event)) ==
			(ssize_t)sizeof(event)) {
		unsigned bit;
		if (event.type != EV_KEY || !(bit = key_bit(event.code)))
			continue;
		if (event.value)
			host.keys |= bit;
		else
			host.keys &= ~bit;
	}
	if ((host.keys & (1u << RETRO_DEVICE_ID_JOYPAD_START)) &&
			(host.keys & (1u << RETRO_DEVICE_ID_JOYPAD_L)))
		stopping = 1;
}

static int16_t input_state(unsigned port, unsigned device, unsigned index,
	unsigned id)
{
	(void)index;
	if (port || device != RETRO_DEVICE_JOYPAD || id >= 32)
		return 0;
	return (host.keys & (1u << id)) != 0;
}

static int open_platform(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;

	host.fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (host.fb_fd < 0 || ioctl(host.fb_fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
			ioctl(host.fb_fd, FBIOGET_VSCREENINFO, &var) < 0)
		return -1;
	if (var.bits_per_pixel != 16) {
		errno = ENOTSUP;
		return -1;
	}
	host.fb_width = var.xres;
	host.fb_height = var.yres;
	host.fb_stride = fix.line_length / sizeof(uint16_t);
	host.fb_bytes = fix.smem_len;
	host.fb_phys = (uint32_t)fix.smem_start;
	host.fb = mmap(NULL, host.fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED,
		host.fb_fd, 0);
	if (host.fb == MAP_FAILED) {
		host.fb = NULL;
		return -1;
	}
	host.input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (host.input_fd < 0)
		return -1;
	if (host.fb_phys && hcge_open_context(&host.ge_storage) == 0) {
		unsigned i;

		host.ge = &host.ge_storage;
		host.ge_source_bytes = (size_t)host.fb_width * host.fb_height *
			sizeof(uint16_t);
		for (i = 0; i < 2; i++) {
			host.ge_source[i] = hcge_linux_alloc_buffer(host.ge,
				(unsigned int)host.ge_source_bytes,
				&host.ge_source_phys[i], &host.ge_source_handle[i]);
			if (!host.ge_source[i])
				break;
			host.ge_buffers++;
		}
		if (!host.ge_buffers) {
			hcge_close_context(host.ge);
			host.ge = NULL;
		} else {
			char details[192];

			memset(host.fb, 0, host.fb_bytes);
#ifdef __mips__
			(void)cacheflush(host.fb, (int)host.fb_bytes, BCACHE);
#endif
			snprintf(details, sizeof(details),
				"GE RGB565 stretch presenter ready fb_phys=%08x source0=%08x source1=%08x bytes=%lu buffers=%u\n",
				host.fb_phys, host.ge_source_phys[0],
				host.ge_source_phys[1],
				(unsigned long)host.ge_source_bytes, host.ge_buffers);
			log_kmsg(details);
		}
	}
	if (!host.ge)
		log_kmsg("GE unavailable; CPU presenter active\n");
	return 0;
}

static void close_platform(void)
{
	if (host.pcm_fd >= 0) {
		(void)ioctl(host.pcm_fd, SNDRV_PCM_IOCTL_DROP);
		close(host.pcm_fd);
		host.pcm_fd = -1;
	}
	if (host.ge) {
		unsigned i;

		(void)hcge_engine_sync(host.ge);
		for (i = 0; i < host.ge_buffers; i++)
			if (host.ge_source_handle[i])
				(void)hcge_linux_free_buffer(host.ge,
					host.ge_source_handle[i]);
		hcge_close_context(host.ge);
		host.ge = NULL;
	}
	if (host.fb)
		munmap(host.fb, host.fb_bytes);
	if (host.fb_fd >= 0)
		close(host.fb_fd);
	if (host.input_fd >= 0)
		close(host.input_fd);
}

extern int sf2000_load_content(const char *path, struct retro_game_info *game);

int main(int argc, char **argv)
{
	struct retro_system_info info;
	struct retro_system_av_info av;
	struct retro_game_info game = { 0 };
	struct timespec deadline;
	long frame_ns;
	struct sigaction fault_action;

	if (argc != 2) {
		fprintf(stderr, "usage: %s ROM\n", argv[0]);
		return 2;
	}
	log_kmsg("entry\n");
	mark_active();
	memset(&fault_action, 0, sizeof(fault_action));
	fault_action.sa_sigaction = fault_signal;
	fault_action.sa_flags = SA_SIGINFO;
	sigemptyset(&fault_action.sa_mask);
	(void)sigaction(SIGILL, &fault_action, NULL);
	(void)sigaction(SIGBUS, &fault_action, NULL);
	(void)sigaction(SIGSEGV, &fault_action, NULL);
	if (open_platform() < 0) {
		log_kmsg("platform open failed\n");
		perror("sf2000-frontend: platform");
		close_platform();
		return 1;
	}
	host.system_dir = "/mnt/sd/bios";
	host.save_dir = "/mnt/sd/saves";
	log_kmsg("platform ready\n");
	retro_set_environment(environment);
	retro_set_video_refresh(video);
	retro_set_audio_sample(audio_sample);
	retro_set_audio_sample_batch(audio_batch);
	retro_set_input_poll(input_poll);
	retro_set_input_state(input_state);
	if (retro_api_version() != RETRO_API_VERSION) {
		fprintf(stderr, "sf2000-frontend: incompatible libretro API\n");
		close_platform();
		return 1;
	}
	memset(&info, 0, sizeof(info));
	retro_get_system_info(&info);
	log_kmsg("core init begin\n");
	retro_init();
	log_kmsg("core init complete\n");
	game.path = argv[1];
	if (!info.need_fullpath && sf2000_load_content(argv[1], &game) < 0) {
		log_kmsg("ROM read failed\n");
		perror("sf2000-frontend: ROM read");
		retro_deinit();
		close_platform();
		return 1;
	}
	log_kmsg("ROM load begin\n");
	if (!retro_load_game(&game)) {
		log_kmsg("core rejected game\n");
		fprintf(stderr, "sf2000-frontend: core rejected %s\n", argv[1]);
		retro_deinit();
		close_platform();
		free((void *)game.data);
		return 1;
	}
	log_kmsg("ROM load complete\n");
	retro_get_system_av_info(&av);
	host.fps = av.timing.fps > 1.0 ? av.timing.fps : 60.0;
	host.audio_rate = av.timing.sample_rate > 1.0 ?
		(unsigned)av.timing.sample_rate : 32000u;
	if (open_audio() < 0)
		log_kmsg("ALSA unavailable; audio disabled\n");
	frame_ns = (long)(1000000000.0 / host.fps);
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	{
		char details[96];

		snprintf(details, sizeof(details),
			"timing frame_ns=%ld audio_core_hz=%u audio_output_hz=32000\n",
			frame_ns, host.audio_rate);
		log_kmsg(details);
	}
	log_kmsg("frontend running START+L exits\n");
	start_metrics_logging();
	signal(SIGINT, stop_signal);
	signal(SIGTERM, stop_signal);
	while (!stopping) {
		retro_run();
		deadline.tv_nsec += frame_ns;
		while (deadline.tv_nsec >= 1000000000L) {
			deadline.tv_nsec -= 1000000000L;
			deadline.tv_sec++;
		}
		(void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
	}
	retro_unload_game();
	retro_deinit();
	close_platform();
	free((void *)game.data);
	log_kmsg("returned cleanly\n");
	(void)unlink(ACTIVE_MARKER);
	/* The process owns the core and all of its mappings.  Avoid running
	 * C++ static destructors after retro_deinit(); they duplicate core
	 * teardown and are not part of the libretro lifecycle. */
	_exit(0);
}
