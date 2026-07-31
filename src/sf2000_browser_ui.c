// SPDX-License-Identifier: MIT
#include <stdlib.h>

static unsigned ui_allocation_failures;

static void *ui_stb_malloc(size_t bytes)
{
	void *memory = calloc(1, bytes);

	if (!memory)
		ui_allocation_failures++;
	return memory;
}

#define STB_TRUETYPE_IMPLEMENTATION
/*
 * stb_truetype can return a bitmap after an internal temporary allocation
 * fails.  The bitmap allocation itself succeeds, but the rasterizer then
 * leaves it untouched.  malloc() makes that look like a glyph made from
 * stale heap contents, which is especially visible on the small NOMMU
 * target.  Zero every stb allocation so an incomplete rasterization is
 * transparent rather than displaying old pixels.
 */
#define STBTT_malloc(x, u) ((void)(u), ui_stb_malloc(x))
#define STBTT_free(x, u) ((void)(u), free(x))
#include "stb_truetype.h"

#include "sf2000_browser_ui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define FONT_LIMIT (8u * 1024u * 1024u)
#define GLYPH_CACHE 192u

struct glyph {
	uint32_t codepoint;
	unsigned char *bitmap;
	int width;
	int height;
	int xoff;
	int yoff;
	int advance;
	unsigned valid;
	unsigned age;
};

struct font {
	unsigned char *data;
	stbtt_fontinfo info;
	float scale;
	int baseline;
	unsigned age;
	struct glyph glyphs[GLYPH_CACHE];
};

struct translation {
	const char *language;
	const char *labels[SF2000_UI_FRAMESKIP + 1];
};

static const struct translation translations[] = {
	{ "en", { "LIBRARY", "NO FILES", "OPEN", "BACK", "EXIT",
		"LOADING EMULATOR", "PLEASE WAIT - SYSTEM IS ACTIVE",
		"NOT ENOUGH MEMORY", "CLOSE OTHER APPLICATIONS",
		"CORE IS NOT INSTALLED", "COPY THE CORE PACKAGE TO THE SD CARD",
		"HOME", "SETTINGS", "RESET", "SAFE SHUTDOWN", "SELECT EMULATOR",
		"RESUME", "FAST FORWARD", "FRAMESKIP" } },
	{ "es", { "BIBLIOTECA", "SIN ARCHIVOS", "ABRIR", "ATRÁS", "SALIR",
		"CARGANDO EMULADOR", "ESPERA - EL SISTEMA ESTÁ ACTIVO",
		"MEMORIA INSUFICIENTE", "CIERRA OTRAS APLICACIONES",
		"EL NÚCLEO NO ESTÁ INSTALADO", "COPIA EL NÚCLEO A LA TARJETA SD",
		"INICIO", "AJUSTES", "REINICIAR", "APAGADO SEGURO",
		"ELIGE EMULADOR", "CONTINUAR", "AVANCE RÁPIDO", "SALTO DE FOTOGRAMAS" } },
	{ "pt", { "BIBLIOTECA", "SEM ARQUIVOS", "ABRIR", "VOLTAR", "SAIR",
		"CARREGANDO EMULADOR", "AGUARDE - SISTEMA ATIVO",
		"MEMÓRIA INSUFICIENTE", "FECHE OUTROS APLICATIVOS",
		"NÚCLEO NÃO INSTALADO", "COPIE O NÚCLEO PARA O CARTÃO SD",
		"INÍCIO", "CONFIGURAÇÕES", "REINICIAR", "DESLIGAMENTO SEGURO",
		"ESCOLHA O EMULADOR", "CONTINUAR", "AVANÇO RÁPIDO", "PULAR QUADROS" } },
	{ "pl", { "BIBLIOTEKA", "BRAK PLIKÓW", "OTWÓRZ", "WSTECZ", "WYJDŹ",
		"ŁADOWANIE EMULATORA", "CZEKAJ - SYSTEM DZIAŁA",
		"ZA MAŁO PAMIĘCI", "ZAMKNIJ INNE APLIKACJE",
		"RDZEŃ NIE JEST ZAINSTALOWANY", "SKOPIUJ RDZEŃ NA KARTĘ SD",
		"START", "USTAWIENIA", "RESTART", "BEZPIECZNE WYŁĄCZENIE",
		"WYBIERZ EMULATOR", "WZNÓW", "PRZEWIJANIE", "POMIJANIE KLATEK" } },
	{ "vi", { "THƯ VIỆN", "KHÔNG CÓ TỆP", "MỞ", "QUAY LẠI", "THOÁT",
		"ĐANG TẢI TRÌNH GIẢ LẬP", "VUI LÒNG ĐỢI - HỆ THỐNG ĐANG CHẠY",
		"KHÔNG ĐỦ BỘ NHỚ", "ĐÓNG CÁC ỨNG DỤNG KHÁC",
		"CHƯA CÀI ĐẶT LÕI", "CHÉP GÓI LÕI VÀO THẺ SD",
		"TRANG CHỦ", "CÀI ĐẶT", "KHỞI ĐỘNG LẠI", "TẮT AN TOÀN",
		"CHỌN TRÌNH GIẢ LẬP", "TIẾP TỤC", "TUA NHANH", "BỎ QUA KHUNG" } },
	{ "ja", { "ライブラリ", "ファイルなし", "ひらく", "もどる", "おわる",
		"エミュレータをよみこみちゅう", "しばらくおまちください",
		"メモリがたりません", "ほかのアプリをとじてください",
		"コアがインストールされていません", "コアをSDカードへコピーしてください",
		"ホーム", "せってい", "さいきどう", "あんぜんにでんげんをきる",
		"エミュレータをえらぶ", "つづける", "はやおくり", "フレームスキップ" } },
};

static uint16_t parse_color(const char *value, uint16_t fallback)
{
	char *end;
	unsigned long color;

	if (*value == '#')
		value++;
	color = strtoul(value, &end, 16);
	if (end == value || *end)
		return fallback;
	if (color <= 0xffffu)
		return (uint16_t)color;
	if (color <= 0xffffffu) {
		unsigned r = (unsigned)(color >> 16) & 0xffu;
		unsigned g = (unsigned)(color >> 8) & 0xffu;
		unsigned b = (unsigned)color & 0xffu;

		return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) |
			(b >> 3));
	}
	return fallback;
}

void sf2000_ui_config_defaults(struct sf2000_ui_config *config)
{
	memset(config, 0, sizeof(*config));
	strcpy(config->language, "en");
	strcpy(config->font, "/mnt/sd/sf2000/ui.ttf");
	config->font_px = 15;
	config->background = 0x0862;
	config->panel = 0x10c4;
	config->header = 0xffff;
	config->text = 0xef7d;
	config->muted = 0x94b2;
	config->accent = 0x35df;
	config->selected_text = 0x0000;
}

static char *trim(char *text)
{
	char *end;

	while (isspace((unsigned char)*text))
		text++;
	end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
		*--end = 0;
	return text;
}

int sf2000_ui_config_load(struct sf2000_ui_config *config, const char *path)
{
	FILE *file;
	char line[512];

	file = fopen(path, "r");
	if (!file)
		return -1;
	while (fgets(line, sizeof(line), file)) {
		char *key = trim(line);
		char *value;
		unsigned long number;

		if (!*key || *key == '#' || *key == ';')
			continue;
		value = strchr(key, '=');
		if (!value)
			continue;
		*value++ = 0;
		key = trim(key);
		value = trim(value);
		if (!strcmp(key, "language")) {
			strncpy(config->language, value,
				sizeof(config->language) - 1u);
			config->language[sizeof(config->language) - 1u] = 0;
		} else if (!strcmp(key, "font")) {
			strncpy(config->font, value, sizeof(config->font) - 1u);
			config->font[sizeof(config->font) - 1u] = 0;
		} else if (!strcmp(key, "font_px")) {
			number = strtoul(value, NULL, 10);
			if (number >= 10u && number <= 24u)
				config->font_px = (unsigned)number;
		} else if (!strcmp(key, "color.background"))
			config->background = parse_color(value, config->background);
		else if (!strcmp(key, "color.panel"))
			config->panel = parse_color(value, config->panel);
		else if (!strcmp(key, "color.header"))
			config->header = parse_color(value, config->header);
		else if (!strcmp(key, "color.text"))
			config->text = parse_color(value, config->text);
		else if (!strcmp(key, "color.muted"))
			config->muted = parse_color(value, config->muted);
		else if (!strcmp(key, "color.accent"))
			config->accent = parse_color(value, config->accent);
		else if (!strcmp(key, "color.selected_text"))
			config->selected_text =
				parse_color(value, config->selected_text);
	}
	fclose(file);
	return 0;
}

static uint32_t utf8_next(const char **text)
{
	const unsigned char *p = (const unsigned char *)*text;
	uint32_t codepoint;

	if (*p < 0x80) {
		*text += *p != 0;
		return *p;
	}
	if ((*p & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
		codepoint = ((uint32_t)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
		*text += 2;
		return codepoint >= 0x80 ? codepoint : 0xfffd;
	}
	if ((*p & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 &&
			(p[2] & 0xc0) == 0x80) {
		codepoint = ((uint32_t)(p[0] & 0x0f) << 12) |
			((uint32_t)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
		*text += 3;
		return codepoint >= 0x800 ? codepoint : 0xfffd;
	}
	if ((*p & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 &&
			(p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
		codepoint = ((uint32_t)(p[0] & 7) << 18) |
			((uint32_t)(p[1] & 0x3f) << 12) |
			((uint32_t)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
		*text += 4;
		return codepoint >= 0x10000 && codepoint <= 0x10ffff ?
			codepoint : 0xfffd;
	}
	(*text)++;
	return 0xfffd;
}

static void glyph_release(struct glyph *glyph)
{
	free(glyph->bitmap);
	memset(glyph, 0, sizeof(*glyph));
}

static struct glyph *font_glyph(struct font *font, uint32_t codepoint)
{
	struct glyph *oldest = &font->glyphs[0];
	unsigned i;
	int index;
	int advance;
	int bearing;

	for (i = 0; i < GLYPH_CACHE; i++) {
		struct glyph *glyph = &font->glyphs[i];

		if (glyph->valid && glyph->codepoint == codepoint) {
			glyph->age = ++font->age;
			return glyph;
		}
		if (!glyph->valid) {
			oldest = glyph;
			break;
		}
		if (glyph->age < oldest->age)
			oldest = glyph;
	}
	glyph_release(oldest);
	oldest->valid = 1;
	oldest->codepoint = codepoint;
	oldest->age = ++font->age;
	index = stbtt_FindGlyphIndex(&font->info, (int)codepoint);
	if (!index && codepoint != 0xfffd)
		index = stbtt_FindGlyphIndex(&font->info, 0xfffd);
	stbtt_GetGlyphHMetrics(&font->info, index, &advance, &bearing);
	(void)bearing;
	oldest->advance = (int)(advance * font->scale + 0.5f);
	oldest->bitmap = stbtt_GetGlyphBitmap(&font->info, 0, font->scale,
		index, &oldest->width, &oldest->height, &oldest->xoff,
		&oldest->yoff);
	return oldest;
}

static struct font *font_open(const char *path, unsigned pixels)
{
	struct font *font;
	FILE *file;
	long length;
	int ascent, descent, gap;

	file = fopen(path, "rb");
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0 ||
			(unsigned long)length > FONT_LIMIT ||
			fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return NULL;
	}
	font = calloc(1, sizeof(*font));
	if (!font) {
		fclose(file);
		return NULL;
	}
	font->data = malloc((size_t)length);
	if (!font->data || fread(font->data, 1, (size_t)length, file) !=
			(size_t)length ||
			!stbtt_InitFont(&font->info, font->data,
				stbtt_GetFontOffsetForIndex(font->data, 0))) {
		fclose(file);
		free(font->data);
		free(font);
		return NULL;
	}
	fclose(file);
	font->scale = stbtt_ScaleForPixelHeight(&font->info, (float)pixels);
	stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &gap);
	(void)descent;
	(void)gap;
	font->baseline = (int)(ascent * font->scale + 0.5f);
	return font;
}

int sf2000_ui_init(struct sf2000_ui *ui, uint16_t *pixels, unsigned width,
	unsigned height, unsigned stride, const struct sf2000_ui_config *config)
{
	memset(ui, 0, sizeof(*ui));
	ui->pixels = pixels;
	ui->width = width;
	ui->height = height;
	ui->stride = stride;
	ui->config = *config;
	ui->font = font_open(config->font, config->font_px);
	return 0;
}

void sf2000_ui_close(struct sf2000_ui *ui)
{
	struct font *font = ui->font;
	unsigned i;

	if (font) {
		for (i = 0; i < GLYPH_CACHE; i++)
			glyph_release(&font->glyphs[i]);
		free(font->data);
		free(font);
	}
	ui->font = NULL;
}

void sf2000_ui_clear(struct sf2000_ui *ui, uint16_t color)
{
	unsigned y, x;

	for (y = 0; y < ui->height; y++)
		for (x = 0; x < ui->width; x++)
			ui->pixels[y * ui->stride + x] = color;
}

void sf2000_ui_fill(struct sf2000_ui *ui, int x, int y, int w, int h,
	uint16_t color)
{
	int row, column;

	for (row = y < 0 ? 0 : y; row < y + h && row < (int)ui->height; row++)
		for (column = x < 0 ? 0 : x;
				column < x + w && column < (int)ui->width; column++)
			ui->pixels[(unsigned)row * ui->stride +
				(unsigned)column] = color;
}

void sf2000_ui_round(struct sf2000_ui *ui, int x, int y, int w, int h,
	int radius, uint16_t color)
{
	int row;

	for (row = 0; row < h; row++) {
		int edge = row < radius ? radius - row :
			row >= h - radius ? row - (h - radius - 1) : 0;
		int horizontal = radius;
		int left;

		while (horizontal > 0 &&
				horizontal * horizontal + edge * edge >
					radius * radius)
			horizontal--;
		left = radius - horizontal;
		sf2000_ui_fill(ui, x + left, y + row, w - left * 2, 1, color);
	}
}

static uint16_t blend565(uint16_t background, uint16_t foreground,
	unsigned alpha)
{
	unsigned inverse;
	unsigned red;
	unsigned green;
	unsigned blue;

	if (alpha >= 255u)
		return foreground;
	if (!alpha)
		return background;
	inverse = 255u - alpha;
	red = ((((background >> 11) & 0x1fu) * inverse) +
		(((foreground >> 11) & 0x1fu) * alpha) + 127u) / 255u;
	green = ((((background >> 5) & 0x3fu) * inverse) +
		(((foreground >> 5) & 0x3fu) * alpha) + 127u) / 255u;
	blue = (((background & 0x1fu) * inverse) +
		((foreground & 0x1fu) * alpha) + 127u) / 255u;
	return (uint16_t)((red << 11) | (green << 5) | blue);
}

static int fallback_advance(uint32_t codepoint)
{
	return codepoint < 0x80 ? 6 : 8;
}

static void fallback_draw(struct sf2000_ui *ui, int x, int y,
	uint32_t codepoint, uint16_t color)
{
	int row, column;
	uint32_t seed = codepoint * 2654435761u;

	if (codepoint == ' ')
		return;
	for (row = 1; row < 8; row++)
		for (column = 1; column < fallback_advance(codepoint) - 1; column++)
			if (row == 1 || row == 7 || column == 1 ||
					column == fallback_advance(codepoint) - 2 ||
					(seed >> ((row + column) & 31)) & 1u)
				sf2000_ui_fill(ui, x + column, y + row, 1, 1,
					color);
}

int sf2000_ui_text(struct sf2000_ui *ui, int x, int y, const char *text,
	uint16_t color, int max_width)
{
	const char *cursor = text;
	int origin = x;
	struct font *font = ui->font;

	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		struct glyph *glyph = font ? font_glyph(font, codepoint) : NULL;
		int advance = glyph ? glyph->advance : fallback_advance(codepoint);
		int row, column;

		if (max_width > 0 && x + advance > origin + max_width)
			break;
		if (glyph && glyph->bitmap) {
			for (row = 0; row < glyph->height; row++)
				for (column = 0; column < glyph->width; column++) {
					unsigned alpha =
						glyph->bitmap[row * glyph->width + column];
					int px = x + glyph->xoff + column;
					int py = y + font->baseline + glyph->yoff + row;

					if (alpha && px >= 0 && py >= 0 &&
							px < (int)ui->width &&
							py < (int)ui->height) {
						uint16_t *pixel =
							&ui->pixels[(unsigned)py *
								ui->stride + (unsigned)px];
						*pixel = blend565(*pixel, color, alpha);
					}
				}
		} else {
			fallback_draw(ui, x, y, codepoint, color);
		}
		x += advance;
	}
	return x - origin;
}

int sf2000_ui_measure(struct sf2000_ui *ui, const char *text)
{
	const char *cursor = text;
	int width = 0;
	struct font *font = ui->font;

	while (*cursor) {
		uint32_t codepoint = utf8_next(&cursor);
		struct glyph *glyph = font ? font_glyph(font, codepoint) : NULL;

		width += glyph ? glyph->advance : fallback_advance(codepoint);
	}
	return width;
}

const char *sf2000_ui_label(const struct sf2000_ui *ui,
	enum sf2000_ui_label label)
{
	unsigned i;

	for (i = 0; i < sizeof(translations) / sizeof(translations[0]); i++)
		if (!strcasecmp(ui->config.language, translations[i].language))
			return translations[i].labels[label];
	return translations[0].labels[label];
}

unsigned sf2000_ui_allocation_failures(void)
{
	return ui_allocation_failures;
}
