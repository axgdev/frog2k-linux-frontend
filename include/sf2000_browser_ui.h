// SPDX-License-Identifier: MIT
#ifndef SF2000_BROWSER_UI_H
#define SF2000_BROWSER_UI_H

#include <stddef.h>
#include <stdint.h>

enum sf2000_ui_label {
	SF2000_UI_LIBRARY,
	SF2000_UI_EMPTY,
	SF2000_UI_OPEN,
	SF2000_UI_BACK,
	SF2000_UI_EXIT,
	SF2000_UI_LOADING,
	SF2000_UI_ACTIVE,
	SF2000_UI_NO_MEMORY,
	SF2000_UI_CLOSE_APPS,
	SF2000_UI_MISSING_CORE,
	SF2000_UI_INSTALL_CORE,
	SF2000_UI_HOME,
	SF2000_UI_SETTINGS,
	SF2000_UI_RESET,
	SF2000_UI_SAFE_SHUTDOWN,
	SF2000_UI_SELECT_CORE,
	SF2000_UI_RESUME,
	SF2000_UI_FAST_FORWARD,
	SF2000_UI_FRAMESKIP,
	SF2000_UI_STATE_SLOT,
	SF2000_UI_SAVE_STATE,
	SF2000_UI_LOAD_STATE,
	SF2000_UI_LABEL_COUNT,
};

struct sf2000_ui_config {
	char language[8];
	char font[256];
	char font_latin[256];
	unsigned font_px;
	uint16_t background;
	uint16_t panel;
	uint16_t header;
	uint16_t text;
	uint16_t muted;
	uint16_t accent;
	uint16_t selected_text;
};

struct sf2000_ui {
	uint16_t *pixels;
	unsigned width;
	unsigned height;
	unsigned stride;
	struct sf2000_ui_config config;
	void *font;
	void *fallback_font;
};

void sf2000_ui_config_defaults(struct sf2000_ui_config *config);
int sf2000_ui_config_load(struct sf2000_ui_config *config, const char *path);
int sf2000_ui_init(struct sf2000_ui *ui, uint16_t *pixels, unsigned width,
	unsigned height, unsigned stride, const struct sf2000_ui_config *config);
void sf2000_ui_close(struct sf2000_ui *ui);
void sf2000_ui_clear(struct sf2000_ui *ui, uint16_t color);
void sf2000_ui_fill(struct sf2000_ui *ui, int x, int y, int width, int height,
	uint16_t color);
void sf2000_ui_round(struct sf2000_ui *ui, int x, int y, int width, int height,
	int radius, uint16_t color);
int sf2000_ui_text(struct sf2000_ui *ui, int x, int y, const char *text,
	uint16_t color, int max_width);
int sf2000_ui_measure(struct sf2000_ui *ui, const char *text);
const char *sf2000_ui_label(const struct sf2000_ui *ui,
	enum sf2000_ui_label label);
unsigned sf2000_ui_allocation_failures(void);
unsigned sf2000_ui_glyph_failures(void);

#endif
