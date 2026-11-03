/*
 * Minimal text console on the ABL splash framebuffer. With the MMU off
 * every access is Device memory, so stay aligned (-mstrict-align).
 */
#include "console.h"
#include "font.h"

#define SCALE		3
#define CH_W		(8 * SCALE)
#define CH_H		(16 * SCALE)
#define MARGIN		24
#define LABEL_COLS	8

volatile uint32_t *con_fb = (volatile uint32_t *)FB_BASE;
#ifdef LAVENDER
/* Mirror the console as text into the ramoops console zone (lavender: 0xa0000000,
   2 MiB), so the next Linux boot shows it in /sys/fs/pstore/console-ramoops-0. */
#define PS_BASE	0xa0000000UL
static void ps_putc(char c)
{
	volatile uint32_t *h = (volatile uint32_t *)PS_BASE;
	volatile uint8_t *d = (volatile uint8_t *)(PS_BASE + 12);
	uint32_t n = h[2];
	*(volatile uint32_t *)0x17817004UL = 1;	/* pet the APSS watchdog (WDT0_RST) */
	if (n >= 0x10000)
		return;
	d[n] = c;
	h[1] = n + 1;	/* start: next write position */
	h[2] = n + 1;	/* size: bytes in use */
}
static void ps_puts(const char *s)
{
	while (*s)
		ps_putc(*s++);
}
void ps_init(void)
{
	volatile uint32_t *h = (volatile uint32_t *)PS_BASE;
	h[1] = 0;
	h[2] = 0;
	h[0] = 0x43474244;	/* PERSISTENT_RAM_SIG "DBGC" */
}
#else
#define ps_putc(c) ((void)0)
#define ps_puts(s) ((void)0)
void ps_init(void) {}
#endif
static uint32_t con_bg;
static int row;

void con_clear(uint32_t bg)
{
	for (int y = 0; y < FB_H; y++) {
		volatile uint32_t *line = con_fb + y * (FB_STRIDE / 4);
		for (int x = 0; x < FB_W; x++)
			line[x] = bg;
	}
	con_bg = bg;
	row = 0;
}

static void put_char(int col, char ch, uint32_t fg)
{
	if (ch < 0x20 || ch > 0x7e)
		ch = '?';
	const unsigned char *g = font8x16[ch - 0x20];
	int x0 = MARGIN + col * CH_W, y0 = MARGIN + row * CH_H;
	if (x0 + CH_W > FB_W || y0 + CH_H > FB_H)
		return;
	for (int gy = 0; gy < 16; gy++)
		for (int gx = 0; gx < 8; gx++) {
			uint32_t c = (g[gy] & (0x80 >> gx)) ? fg : con_bg;
			for (int sy = 0; sy < SCALE; sy++) {
				volatile uint32_t *p = con_fb + (y0 + gy * SCALE + sy) * (FB_STRIDE / 4)
						       + x0 + gx * SCALE;
				for (int sx = 0; sx < SCALE; sx++)
					p[sx] = c;
			}
		}
}

static int put_str(int col, const char *s, uint32_t fg)
{
	ps_puts(s);
	ps_putc(32);
	while (*s)
		put_char(col++, *s++, fg);
	return col;
}

static void fmt_hex(char *buf, uint64_t v, int digits)
{
	static const char hex[] = "0123456789ABCDEF";
	for (int i = 0; i < digits; i++)
		buf[i] = hex[(v >> (4 * (digits - 1 - i))) & 0xf];
	buf[digits] = 0;
}

void con_skip(void)
{
	ps_putc(10);
	row++;
}

void con_str(const char *label, const char *s)
{
	put_str(0, label, YELLOW);
	put_str(LABEL_COLS, s, WHITE);
	ps_putc(10);
	row++;
}

void con_hex(const char *label, uint64_t v)
{
	char buf[17];
	fmt_hex(buf, v, 16);
	con_str(label, buf);
}

void con_hex2(const char *label, const char *a, uint32_t va, const char *b, uint32_t vb)
{
	char buf[9];
	int col = put_str(0, label, YELLOW);
	col = col < LABEL_COLS ? LABEL_COLS : col + 1;
	col = put_str(col, a, YELLOW);
	fmt_hex(buf, va, 8);
	col = put_str(col, buf, WHITE);
	if (b) {
		col = put_str(col + 1, b, YELLOW);
		fmt_hex(buf, vb, 8);
		put_str(col, buf, WHITE);
	}
	ps_putc(10);
	row++;
}
