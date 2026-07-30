// SPDX-License-Identifier: MIT
#include "sf2000_browser_ui.h"

#include <stdint.h>
#include <string.h>

int main(void)
{
	uint16_t pixels[64 * 32];
	struct sf2000_ui_config config;
	struct sf2000_ui ui;
	unsigned changed = 0;
	unsigned i;

	sf2000_ui_config_defaults(&config);
	config.font[0] = 0;
	strcpy(config.language, "ja");
	if (sf2000_ui_init(&ui, pixels, 64, 32, 64, &config) != 0)
		return 1;
	sf2000_ui_clear(&ui, config.background);
	sf2000_ui_round(&ui, 1, 1, 62, 30, 5, config.panel);
	if (sf2000_ui_measure(&ui,
			sf2000_ui_label(&ui, SF2000_UI_LIBRARY)) <= 0)
		return 1;
	(void)sf2000_ui_text(&ui, 2, 4, "UTF-8: 日本語",
		config.text, 60);
	for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
		if (pixels[i] != config.background)
			changed++;
	sf2000_ui_close(&ui);
	return changed < 100 ? 1 : 0;
}
