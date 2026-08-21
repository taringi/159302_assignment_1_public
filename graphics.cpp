////////////////////////////////////////////////////////////////////////
//
//   Program Name:  Graphics Engine (Implementation File)
//      Description:  graphics2 package extracted from:
//                  http://csci.biola.edu/csci105/using_winbgi.html  
//
//                  *slightly modified to run for gcc by N.Reyes              
//
//
////////////////////////////////////////////////////////////////////////
// You don't need to edit this file, or print it out.

#ifdef _WIN32

#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include "graphics.h"

#define MAX_PAGES 16

static HDC hdc[4];

static HPEN hPen;
static HRGN hRgn;
static HFONT hFont;
static NPLOGPALETTE pPalette;
static PAINTSTRUCT ps;
static HWND hWnd;
static HBRUSH hBrush[USER_FILL+1];
static HBRUSH hBackgroundBrush;

static HPALETTE hPalette;
static HBITMAP hBitmap[MAX_PAGES];
static HBITMAP hPutimageBitmap;

static int timeout_expired;

#define PEN_CACHE_SIZE   8
#define FONT_CACHE_SIZE  8 
#define BG               64
#define TIMER_ID         1

//
// When XOR or NOT write modes are used for drawing high BG bit is cleared, so
// drawing colors should be adjusted to preserve this bit
// 
#define ADJUSTED_MODE(mode) ((mode) == XOR_PUT || (mode) == NOT_PUT)

int bgiemu_handle_redraw = TRUE;
int bgiemu_default_mode = VGAHI; //VGAMAX;

static int screen_width;
static int screen_height;
static int window_width;
static int window_height;

//Mouse info    (Added 1-Oct-2000, Matthew Weathers)
static bool bMouseUp = false;
static bool bMouseDown = false;
static int iCurrentMouseX = 0;
static int iCurrentMouseY = 0;
static int iClickedMouseX = 0;
static int iClickedMouseY = 0;
static int iWhichMouseButton = LEFT_BUTTON;

static int line_style_cnv[] = {
    PS_SOLID, PS_DOT, PS_DASHDOT, PS_DASH, 
    PS_DASHDOTDOT /* if user style lines are not supported */
}; 
static int write_mode_cnv[] = 
  {R2_COPYPEN, R2_XORPEN, R2_MERGEPEN, R2_MASKPEN, R2_NOTCOPYPEN};
static int bitblt_mode_cnv[] = 
  {SRCCOPY, SRCINVERT, SRCPAINT, SRCAND, NOTSRCCOPY};

static int font_weight[] = 
{ 
    FW_BOLD,    // DefaultFont
    FW_NORMAL,  // TriplexFont
    FW_NORMAL,  // SmallFont
    FW_NORMAL,  // SansSerifFont
    FW_NORMAL,  // GothicFont
    FW_NORMAL,  // ScriptFont
    FW_NORMAL,  // SimplexFont
    FW_NORMAL,  // TriplexScriptFont
    FW_NORMAL,  // ComplexFont
    FW_NORMAL,  // EuropeanFont
    FW_BOLD     // BoldFont
};

static int font_family[] = 
{
    FIXED_PITCH|FF_DONTCARE,     // DefaultFont
    VARIABLE_PITCH|FF_ROMAN,     // TriplexFont
    VARIABLE_PITCH|FF_MODERN,    // SmallFont
    VARIABLE_PITCH|FF_DONTCARE,  // SansSerifFont
    VARIABLE_PITCH|FF_SWISS,     // GothicFont
    VARIABLE_PITCH|FF_SCRIPT,    // ScriptFont
    VARIABLE_PITCH|FF_DONTCARE,  // SimplexFont
    VARIABLE_PITCH|FF_SCRIPT,    // TriplexScriptFont
    VARIABLE_PITCH|FF_DONTCARE,  // ComplexFont
    VARIABLE_PITCH|FF_DONTCARE,  // EuropeanFont
    VARIABLE_PITCH|FF_DONTCARE   // BoldFont
  };

//static char* font_name[] =  //old
static const char* font_name[] =
{
    "Console",          // DefaultFont
    "Times New Roman",  // TriplexFont
    "Small Fonts",      // SmallFont
    "MS Sans Serif",    // SansSerifFont
    "Arial",            // GothicFont
    "Script",           // ScriptFont
    "Times New Roman",  // SimplexFont
    "Script",           // TriplexScriptFont
    "Courier New",      // ComplexFont
    "Times New Roman",  // EuropeanFont
    "Courier New Bold", // BoldFont
};

static int text_halign_cnv[] = {TA_LEFT, TA_CENTER, TA_RIGHT};  
static int text_valign_cnv[] = {TA_BOTTOM, TA_BASELINE, TA_TOP};

static palettetype current_palette;

static struct { int width; int height; } font_metrics[][11] = { 
{{0,0},{8,8},{16,16},{24,24},{32,32},{40,40},{48,48},{56,56},{64,64},{72,72},{80,80}}, // DefaultFont
{{0,0},{13,18},{14,20},{16,23},{22,31},{29,41},{36,51},{44,62},{55,77},{66,93},{88,124}}, // TriplexFont
{{0,0},{3,5},{4,6},{4,6},{6,9},{8,12},{10,15},{12,18},{15,22},{18,27},{24,36}}, // SmallFont
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}}, // SansSerifFont 
{{0,0},{13,19},{14,21},{16,24},{22,32},{29,42},{36,53},{44,64},{55,80},{66,96},{88,128}}, // GothicFont
// I am not sure about metrics of following fonts
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}}, // ScriptFont
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}}, // SimplexFont
{{0,0},{13,18},{14,20},{16,23},{22,31},{29,41},{36,51},{44,62},{55,77},{66,93},{88,124}}, // TriplexScriptFont
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}}, // ComplexFont
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}}, // EuropeanFont
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}} // BoldFont
}; 

struct BGIimage { 
    short width;
    short height;
    int   reserved; // let bits be aligned to DWORD boundary
    char  bits[1];
};

struct BGIbitmapinfo { 
    BITMAPINFOHEADER hdr;
    short            color_table[64];
};
    
static BGIbitmapinfo bminfo = {
    {sizeof(BITMAPINFOHEADER), 0, 0, 1, 4, BI_RGB}
};

static int* image_bits; 

static int normal_font_size[] = { 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

static linesettingstype line_settings;
static fillsettingstype fill_settings;

static int color;
static int bkcolor;
static int text_color;

static int aspect_ratio_x, aspect_ratio_y;

static textsettingstype text_settings;

static viewporttype view_settings;

static int font_mul_x, font_div_x, font_mul_y, font_div_y;

static enum { ALIGN_NOT_SET, UPDATE_CP, NOT_UPDATE_CP } text_align_mode; 

#define BORDER_WIDTH  8 
#define BORDER_HEIGHT 27

static int write_mode;

static int visual_page;
static int active_page;

static arccoordstype ac;

class char_queue { 
  protected:
    char* buf; 
    int   put_pos;
    int   get_pos;
    int   buf_size; 
  public:
    void  put(char ch) { 
	buf[put_pos] = ch; 
	if (++put_pos == buf_size) { 
	    put_pos = 0;
	}
	if (put_pos == get_pos) { // queue is full
	    (void)get(); // loose one character
	}
    }
    char get() { 
	char ch = buf[get_pos]; 
	if (++get_pos == buf_size) { 
	    get_pos = 0;
	}
	return ch;
    }
    bool is_empty() { 
	return get_pos == put_pos; 
    }
    char_queue(int buf_size = 256) { 
	put_pos = get_pos = 0;
	this->buf_size = buf_size;
	buf = new char[buf_size]; 
    }
    ~char_queue() { 
	delete[] buf; 
    }
};

static char_queue kbd_queue;

inline int convert_userbits(DWORD buf[32], unsigned pattern)
{
    int i = 0, j;
    pattern &= 0xFFFF;

    while (true) { 
	for (j = 0; pattern & 1; j++) pattern >>= 1;
	buf[i++] = j;
	if (pattern == 0) { 
	    buf[i++] = 16 - j;
	    return i;
	}
	for (j = 0; !(pattern & 1); j++) pattern >>= 1;
	buf[i++] = j;
    }
} 


class l2elem { 
  public: 
    l2elem* next;
    l2elem* prev;

    void link_after(l2elem* after) { 
	(next = after->next)->prev = this;
	(prev = after)->next = this;
    }
    void unlink() { 
	prev->next = next;
	next->prev = prev;
    }
    void prune() { 
	next = prev = this;
    }
};

class l2list : public l2elem { 
  public:
    l2list() { prune(); }
};

class pen_cache : public l2list { 
    class pen_cache_item : public l2elem {
      public:
    	HPEN pen;
        int  color;
	int  width;
	int  style;
	unsigned pattern;
    };  
    pen_cache_item* free;
    pen_cache_item  cache[PEN_CACHE_SIZE];

  public: 
    pen_cache() { 
	for (int i = 0; i < PEN_CACHE_SIZE-1; i++) { 
	    cache[i].next = &cache[i+1];
	}
	cache[PEN_CACHE_SIZE-1].next = NULL;
	free = cache;
    }
    void select(int color) 
    {
	for (l2elem* elem = next; elem != this; elem = elem->next) { 
	    pen_cache_item* ci = (pen_cache_item*)elem;
	    if (ci->color == color &&
		ci->style == line_settings.linestyle &&
		ci->width == line_settings.thickness &&
		(line_settings.linestyle != USERBIT_LINE 
		 || line_settings.upattern == ci->pattern))
	    {
		ci->unlink(); // LRU discipline
		ci->link_after(this); 

		if (hPen != ci->pen) { 
		    hPen = ci->pen;
		    SelectObject(hdc[0], hPen);
		    SelectObject(hdc[1], hPen);
		}
		return;	    
	    }
	}
	hPen = NULL;
	if (line_settings.linestyle == USERBIT_LINE) { 
	    LOGBRUSH lb;
	    lb.lbColor = PALETTEINDEX(color);
	    lb.lbStyle = BS_SOLID;
	    DWORD style[32]; 
	    hPen = ExtCreatePen(PS_GEOMETRIC|PS_USERSTYLE, 
				line_settings.thickness, &lb, 
				convert_userbits(style,line_settings.upattern),
				style);
	} 
	if (hPen == NULL) { 
	    hPen = CreatePen(line_style_cnv[line_settings.linestyle], 
			     line_settings.thickness, 
			     PALETTEINDEX(color));
	}
	SelectObject(hdc[0], hPen);
	SelectObject(hdc[1], hPen);
	
	pen_cache_item* p;
	if (free == NULL) {
	    p = (pen_cache_item*)prev; 
	    p->unlink();
	    DeleteObject(p->pen);	    
	} else { 
	    p = free;
	    free = (pen_cache_item*)p->next;
	}
	p->pen   = hPen;
	p->color = color;
	p->width = line_settings.thickness;
	p->style = line_settings.linestyle;
	p->pattern = line_settings.upattern;
	p->link_after(this);
    }  
};	


static pen_cache pcache;



class font_cache : public l2list { 
    class font_cache_item : public l2elem {
      public:
    	HFONT font;
        int   type;
	int   direction;
	int   width, height;
    };  
    font_cache_item* free;
    font_cache_item  cache[FONT_CACHE_SIZE];

  public: 
    font_cache() { 
	for (int i = 0; i < FONT_CACHE_SIZE-1; i++) { 
	    cache[i].next = &cache[i+1];
	}
	cache[FONT_CACHE_SIZE-1].next = NULL;
	free = cache;
    }
    void select(int type, int direction, int width, int height) 
    {
	for (l2elem* elem = next; elem != this; elem = elem->next) { 
	    font_cache_item* ci = (font_cache_item*)elem;
	    if (ci->type == type &&
		ci->direction == direction &&
		ci->width == width &&
		ci->height == height)
	    {
		ci->unlink();
		ci->link_after(this);

		if (hFont != ci->font) { 
		    hFont = ci->font;
		    SelectObject(hdc[0], hFont);
		    SelectObject(hdc[1], hFont);
		}
		return;	    
	    }
	}
	hFont = CreateFont(-height,
			   width,
			   direction*900,
			   (direction&1)*900,
			   font_weight[type],
			   FALSE,
			   FALSE,
			   FALSE,
			   DEFAULT_CHARSET, 
			   OUT_DEFAULT_PRECIS,
			   CLIP_DEFAULT_PRECIS,
			   DEFAULT_QUALITY,
			   font_family[type], 
			   font_name[type]);
	SelectObject(hdc[0], hFont);
	SelectObject(hdc[1], hFont);
	
	font_cache_item* p;
	if (free == NULL) {
	    p = (font_cache_item*)prev; 
	    p->unlink();
	    DeleteObject(p->font);	    
	} else { 
	    p = free;
	    free = (font_cache_item*)p->next;
	}
	p->font = hFont;
	p->type = type;
	p->width = width;
	p->height = height;
	p->direction = direction;
	p->link_after(this);
    }  
};	


static font_cache fcache;


#define FLAGS         PC_NOCOLLAPSE
#define PALETTE_SIZE  256

static PALETTEENTRY BGIcolor[64] = {
		{ 0, 0, 0, FLAGS },        // 0
		{ 0, 0, 255, FLAGS },        // 1
		{ 0, 255, 0, FLAGS },        // 2
		{ 0, 255, 255, FLAGS },        // 3
		{ 255, 0, 0, FLAGS },        // 4
		{ 255, 0, 255, FLAGS },         // 5
		{ 165, 42, 42, FLAGS },        // 6
		{ 211, 211, 211, FLAGS },         // 7
		{ 47, 79, 79, FLAGS },        // 8
		{ 173, 216, 230, FLAGS },        // 9
		{ 32, 178, 170, FLAGS },        // 10
		{ 224, 255, 255, FLAGS },        // 11
		{ 240, 128, 128, FLAGS },        // 12
		{ 219, 112, 147, FLAGS },        // 13
		{ 255, 255, 0, FLAGS },        // 14
		{ 255, 255, 255, FLAGS },        // 15
        { 0xF0, 0xF8, 0xFF, FLAGS },        // 16
        { 0xFA, 0xEB, 0xD7, FLAGS },        // 17
        { 0x22, 0x85, 0xFF, FLAGS },        // 18
        { 0x7F, 0xFF, 0xD4, FLAGS },        // 19
        { 0xF0, 0xFF, 0xFF, FLAGS },        // 20
        { 0xF5, 0xF5, 0xDC, FLAGS },        // 21
        { 0xFF, 0xE4, 0xC4, FLAGS },        // 22
        { 0xFF, 0x7B, 0xCD, FLAGS },        // 23
        { 0x00, 0x00, 0xFF, FLAGS },        // 24
        { 0x8A, 0x2B, 0xE2, FLAGS },        // 25
        { 0xA5, 0x2A, 0x2A, FLAGS },        // 26
        { 0xDE, 0xB8, 0x87, FLAGS },        // 27
        { 0x5F, 0x9E, 0xA0, FLAGS },        // 28
        { 0x7F, 0xFF, 0x00, FLAGS },        // 29
        { 0xD2, 0x50, 0x1E, FLAGS },        // 30
        { 0xFF, 0x7F, 0x50, FLAGS },        // 31
        { 0x64, 0x95, 0xED, FLAGS },        // 32
        { 0xFF, 0xF8, 0xDC, FLAGS },        // 33
        { 0xDC, 0x14, 0x3C, FLAGS },        // 34
        { 0x68, 0xCF, 0xDF, FLAGS },        // 35
        { 0x00, 0x00, 0x8B, FLAGS },        // 36
        { 0x00, 0x8B, 0x8B, FLAGS },        // 37
        { 0xB8, 0x86, 0x0B, FLAGS },        // 38
        { 0xA9, 0xA9, 0xA9, FLAGS },        // 39
        { 0x00, 0x64, 0x00, FLAGS },        // 40
        { 0xBD, 0xB7, 0x6B, FLAGS },        // 41
        { 0x8B, 0x00, 0x8B, FLAGS },        // 42
        { 0x55, 0x6B, 0x2F, FLAGS },        // 43
        { 0xFF, 0x8C, 0x00, FLAGS },        // 44
        { 0xB9, 0x82, 0xFC, FLAGS },        // 45
        { 0x8B, 0x00, 0x00, FLAGS },        // 46
        { 0xE9, 0x96, 0x7A, FLAGS },        // 47
        { 0x8F, 0xBC, 0x8F, FLAGS },        // 48
        { 0x48, 0x3D, 0x8B, FLAGS },        // 49
        { 0x2F, 0x4F, 0x4F, FLAGS },        // 50
        { 0x00, 0xCE, 0xD1, FLAGS },        // 51
        { 0x94, 0x00, 0xD3, FLAGS },        // 52
        { 0xFF, 0x14, 0x93, FLAGS },        // 53
        { 0x00, 0xBF, 0xFF, FLAGS },        // 54
        { 0x69, 0x69, 0x69, FLAGS },        // 55
        { 0x1E, 0x90, 0xFF, FLAGS },        // 56
        { 0xB2, 0x22, 0x22, FLAGS },        // 57
        { 0xFF, 0xFA, 0xF0, FLAGS },        // 58
        { 0x22, 0x8B, 0x22, FLAGS },        // 59
        { 0xFF, 0x00, 0xFF, FLAGS },        // 60
        { 0xDC, 0xDC, 0xDC, FLAGS },        // 61
        { 0xF8, 0xF8, 0xBF, FLAGS },        // 62
        { 0xFF, 0xD7, 0x00, FLAGS },        // 63
};
    
static PALETTEENTRY BGIpalette[64];

static short SolidBrushBitmap[8] = 
  {~0xFF, ~0xFF, ~0xFF, ~0xFF, ~0xFF, ~0xFF, ~0xFF, ~0xFF};  
static short LineBrushBitmap[8] = 
  {~0x00, ~0x00, ~0x00, ~0x00, ~0x00, ~0x00, ~0x00, ~0xFF};
static short LtslashBrushBitmap[8] = 
  {~0x01, ~0x02, ~0x04, ~0x08, ~0x10, ~0x20, ~0x40, ~0x80};
static short SlashBrushBitmap[8] = 
  {~0x81, ~0x03, ~0x06, ~0x0C, ~0x18, ~0x30, ~0x60, ~0xC0};
static short BkslashBrushBitmap[8] = 
  {~0xC0, ~0x60, ~0x30, ~0x18, ~0x0C, ~0x06, ~0x03, ~0x81};
static short LtbkslashBrushBitmap[8] = 
  {~0x80, ~0x40, ~0x20, ~0x10, ~0x08, ~0x04, ~0x02, ~0x01};
static short HatchBrushBitmap[8] = 
  {~0x01, ~0x01, ~0x01, ~0x01, ~0x01, ~0x01, ~0x01, ~0xFF};
static short XhatchBrushBitmap[8] = 
  {~0x81, ~0x42, ~0x24, ~0x18, ~0x18, ~0x24, ~0x42, ~0x81};
static short InterleaveBrushBitmap[8] = 
  {~0x55, ~0xAA, ~0x55, ~0xAA, ~0x55, ~0xAA, ~0x55, ~0xAA};
static short WidedotBrushBitmap[8] = 
  {~0x00, ~0x00, ~0x00, ~0x00, ~0x00, ~0x00, ~0x00, ~0x01};
static short ClosedotBrushBitmap[8] = 
  {~0x44, ~0x00, ~0x11, ~0x00, ~0x44, ~0x00, ~0x11, ~0x00};
	
char* grapherrormsg(int code) {	
    static char buf[256];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 
    	          NULL, code, 0, 
    	          buf, sizeof buf, NULL);
    return buf;
}

static int gdi_error_code;

int graphresult()
{
    return gdi_error_code; 
}

void setcolor(int c)
{
    c &= MAXCOLORS;
    color = c;
    SetTextColor(hdc[0], PALETTEINDEX(c+BG));
    SetTextColor(hdc[1], PALETTEINDEX(c+BG));
}

int getmaxcolor()
{
    return WHITE;
}

int getmaxmode()
{
    return VGAMAX;
}

char* getmodename(int mode)
{
    static char mode_str[32];
    sprintf(mode_str, "%d x %d %s", window_width, window_height, 
	    mode < 2 ? "EGA" : "VGA");
    return mode_str;
}

int getx()
{
    POINT pos;
    GetCurrentPositionEx(hdc[active_page == visual_page ? 0 : 1], &pos);
    return pos.x;
}

int gety()
{
    POINT pos;
    GetCurrentPositionEx(hdc[active_page == visual_page ? 0 : 1], &pos);
    return pos.y;
}

int getmaxx()
{
    return window_width-1;
}

int getmaxy()
{
    return window_height-1;
}


int getcolor()
{
    return color;
}

//char* getdrivername() //old
const char* getdrivername()
{
    return "EGAVGA";
}

void setlinestyle(int style, unsigned int pattern, int thickness)
{
    line_settings.linestyle = style;
    line_settings.thickness = thickness;
    line_settings.upattern  = pattern;
}

void getlinesettings(linesettingstype* ls)
{
    *ls = line_settings;
}

void setwritemode(int mode)
{
    write_mode = mode;
}

void setpalette(int index, int color)
{
    color &= MAXCOLORS;
    BGIpalette[index] = BGIcolor[color];
    current_palette.colors[index] = color;
    SetPaletteEntries(hPalette, BG+index, 1, &BGIpalette[index]);
    RealizePalette(hdc[0]);
    if (index == 0) { 
	bkcolor = 0;
    }
}

void setrgbpalette(int index, int red, int green, int blue)
{
    BGIpalette[index].peRed = red & 0xFC;
    BGIpalette[index].peGreen = green & 0xFC;
    BGIpalette[index].peBlue = blue & 0xFC;
    SetPaletteEntries(hPalette, BG+index, 1, &BGIpalette[index]);
    RealizePalette(hdc[0]);
    if (index == 0) { 
	bkcolor = 0;
    }
}

void setallpalette(palettetype* pal)
{
    for (int i = 0; i < pal->size; i++) { 
	current_palette.colors[i] = pal->colors[i] & MAXCOLORS;
	BGIpalette[i] = BGIcolor[pal->colors[i] & MAXCOLORS];
    }
    SetPaletteEntries(hPalette, BG, pal->size, BGIpalette);
    RealizePalette(hdc[0]);
    bkcolor = 0;
}

palettetype* getdefaultpalette()
{
    static palettetype default_palette = { 64,
      { BLACK, BLUE, GREEN, CYAN, RED, MAGENT, BROWN, LIGHTGRAY, DARKGRAY, 
        LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
		33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
		50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
      }};
    return &default_palette;
}

void getpalette(palettetype* pal)
{
    *pal = current_palette;
}

int getpalettesize() 
{
    return MAXCOLORS+1;
}

void setbkcolor(int color)
{
    color &= MAXCOLORS;
    BGIpalette[0] = BGIcolor[color];
    SetPaletteEntries(hPalette, BG, 1, &BGIpalette[0]);
    RealizePalette(hdc[0]);
    bkcolor = color;
}

int getbkcolor()
{
    return bkcolor;
}

void setfillstyle(int style, int color)
{
    fill_settings.pattern = style;
    fill_settings.color = color & MAXCOLORS;
    SelectObject(hdc[0], hBrush[style]);
    SelectObject(hdc[1], hBrush[style]);
}

void getfillsettings(fillsettingstype* fs)
{
    *fs = fill_settings;
}

static fillpatterntype userfillpattern = 
{-1, -1, -1, -1, -1, -1, -1, -1};

void setfillpattern(char const* upattern, int color)
{
    static HBITMAP hFillBitmap;
    static short bitmap_data[8];
    for (int i = 0; i < 8; i++) { 
	bitmap_data[i] = (unsigned char)~upattern[i];
	userfillpattern[i] = upattern[i];
    }
    HBITMAP h = CreateBitmap(8, 8, 1, 1, bitmap_data);
    HBRUSH hb = CreatePatternBrush(h);
    DeleteObject(hBrush[USER_FILL]);
    if (hFillBitmap) { 
	DeleteObject(hFillBitmap);
    }
    hFillBitmap = h;
    hBrush[USER_FILL] = hb;
    SelectObject(hdc[0], hb);
    SelectObject(hdc[1], hb);
    fill_settings.color = color & MAXCOLORS;
    fill_settings.pattern = USER_FILL;
}

void getfillpattern(fillpatterntype fp)
{
    memcpy(fp, userfillpattern, sizeof userfillpattern);
}


inline void select_fill_color()
{
    if (text_color != fill_settings.color) { 
	text_color = fill_settings.color;
	SetTextColor(hdc[0], PALETTEINDEX(text_color+BG));
	SetTextColor(hdc[1], PALETTEINDEX(text_color+BG));
    }
}

void setusercharsize(int multx, int divx, int multy, int divy)
{
    font_mul_x = multx;
    font_div_x = divx;
    font_mul_y = multy;
    font_div_y = divy;
    text_settings.charsize = 0;
}

void moveto(int x, int y)
{
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	MoveToEx(hdc[1], x, y, NULL);
    }
    if (visual_page == active_page) { 
	MoveToEx(hdc[0], x, y, NULL);
    } 
}

void moverel(int dx, int dy)
{
    POINT pos;
    GetCurrentPositionEx(hdc[1], &pos);
    moveto(pos.x + dx, pos.y + dy);
}

static void select_font()
{
    if (text_settings.charsize == 0) { 
	fcache.select(text_settings.font, text_settings.direction, 
		      font_metrics[text_settings.font]
		                  [normal_font_size[text_settings.font]].width
		                  *font_mul_x/font_div_x,
		      font_metrics[text_settings.font]
		                  [normal_font_size[text_settings.font]].height
		                  *font_mul_y/font_div_y); 
    } else { 
	fcache.select(text_settings.font, text_settings.direction, 
	    font_metrics[text_settings.font][text_settings.charsize].width,
	    font_metrics[text_settings.font][text_settings.charsize].height);
    }
}

static void text_output(int x, int y, const char* str)
{ 
    select_font();
    if (text_color != color) { 
	text_color = color;
	SetTextColor(hdc[0], PALETTEINDEX(text_color+BG));
	SetTextColor(hdc[1], PALETTEINDEX(text_color+BG));
    }
    if (bgiemu_handle_redraw || visual_page != active_page) { 
        TextOut(hdc[1], x, y, str, strlen(str));
    }
    if (visual_page == active_page) { 
        TextOut(hdc[0], x, y, str, strlen(str));
    } 
}


void settextstyle(int font, int direction, int char_size)
{
    if (char_size > 10) { 
	char_size = 10;
    }
    text_settings.direction = direction;
    text_settings.font = font;
    text_settings.charsize = char_size;
    text_align_mode = ALIGN_NOT_SET;
}

void settextjustify(int horiz, int vert)
{
    text_settings.horiz = horiz;
    text_settings.vert = vert;
    text_align_mode = ALIGN_NOT_SET;
}

void gettextsettings(textsettingstype* ts)
{
    *ts = text_settings;
}


int textheight(const char* str)
{
    SIZE ss;
    select_font();
    GetTextExtentPoint32(hdc[0], str, strlen(str), &ss);
    return ss.cy;
}

int textwidth(const char* str)
{
    SIZE ss;
    select_font();
    GetTextExtentPoint32(hdc[0], str, strlen(str), &ss);
    return ss.cx;
}

void outtext(const char* str)
{
    if (text_align_mode != UPDATE_CP) {
	text_align_mode = UPDATE_CP;
	int align = (text_settings.direction == HORIZ_DIR)
	            ? (TA_UPDATECP | 
		       text_halign_cnv[text_settings.horiz] | 
		       text_valign_cnv[text_settings.vert])
	            : (TA_UPDATECP |
		       text_valign_cnv[text_settings.horiz] | 
		       text_halign_cnv[text_settings.vert]);
	SetTextAlign(hdc[0], align); 
	SetTextAlign(hdc[1], align);
    }
    text_output(0, 0, str);
}

void outtextxy(int x, int y, const char* str)
{
    if (text_align_mode != NOT_UPDATE_CP) {
	text_align_mode = NOT_UPDATE_CP;
	int align = (text_settings.direction == HORIZ_DIR)
	            ? (TA_NOUPDATECP | 
		       text_halign_cnv[text_settings.horiz] | 
		       text_valign_cnv[text_settings.vert])
	            : (TA_NOUPDATECP |
		       text_valign_cnv[text_settings.horiz] | 
		       text_halign_cnv[text_settings.vert]);
	SetTextAlign(hdc[0], align);
	SetTextAlign(hdc[1], align);
    }
    text_output(x, y, str);
}

void setviewport(int x1, int y1, int x2, int y2, int clip)
{
    view_settings.left = x1;
    view_settings.top = y1;
    view_settings.right = x2;
    view_settings.bottom = y2;
    view_settings.clip = clip;

    if (hRgn) { 
	DeleteObject(hRgn);
    }
    hRgn = clip ? CreateRectRgn(x1, y1, x2, y2) : NULL;
    SelectClipRgn(hdc[1], hRgn);
    SetViewportOrgEx(hdc[1], x1, y1, NULL);

    SelectClipRgn(hdc[0], hRgn);
    SetViewportOrgEx(hdc[0], x1, y1, NULL);
    
    moveto(0,0);
}

void getviewsettings(viewporttype *viewport)
{
     *viewport = view_settings;
}

const double pi = 3.14159265358979323846;

inline void arc_coords(double angle, double rx, double ry, int& x, int& y)
{ 
    if (rx == 0 || ry == 0) { 
	x = y = 0;
	return;
    }
    double s = sin(angle*pi/180.0);
    double c = cos(angle*pi/180.0);
    if (fabs(s) < fabs(c)) { 
	double tg = s/c;
	double xr = sqrt((double)rx*rx*ry*ry/(ry*ry+rx*rx*tg*tg));
	x = int((c >= 0) ? xr : -xr);
	y = int((s >= 0) ? -xr*tg : xr*tg);
    } else { 
	double ctg = c/s;
	double yr = sqrt((double)rx*rx*ry*ry/(rx*rx+ry*ry*ctg*ctg));
        x = int((c >= 0) ? yr*ctg : -yr*ctg);
	y = int((s >= 0) ? -yr : yr);
    }
}

void ellipse(int x, int y, int start_angle, int end_angle, 
		       int rx, int ry)
{
    ac.x = x;
    ac.y = y;
    arc_coords(start_angle, rx, ry, ac.xstart, ac.ystart);
    arc_coords(end_angle,  rx, ry, ac.xend, ac.yend);
    ac.xstart += x; ac.ystart += y;
    ac.xend += x; ac.yend += y;

    pcache.select(color+BG); 
    if (bgiemu_handle_redraw || visual_page != active_page) { 
        Arc(hdc[1], x-rx, y-ry, x+rx, y+ry, 
	    ac.xstart, ac.ystart, ac.xend, ac.yend); 
    }
    if (visual_page == active_page) { 
	Arc(hdc[0], x-rx, y-ry, x+rx, y+ry, 
	    ac.xstart, ac.ystart, ac.xend, ac.yend); 
    }
}

void fillellipse(int x, int y, int rx, int ry)
{
    pcache.select(color+BG); 
    select_fill_color();
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	Ellipse(hdc[1], x-rx, y-ry, x+rx, y+ry); 
    }
    if (visual_page == active_page) { 
	Ellipse(hdc[0], x-rx, y-ry, x+rx, y+ry); 
    }
}

static void allocate_new_graphic_page(int page)
{
    RECT scr;
    scr.left = -view_settings.left;
    scr.top = -view_settings.top; 
    scr.right = screen_width-view_settings.left-1;
    scr.bottom = screen_height-view_settings.top-1;
    hBitmap[page] = CreateCompatibleBitmap(hdc[0],screen_width,screen_height);
    SelectObject(hdc[1], hBitmap[page]);	    
    SelectClipRgn(hdc[1], NULL);
    FillRect(hdc[1], &scr, hBackgroundBrush);
    SelectClipRgn(hdc[1], hRgn);
}

void setactivepage(int page)
{
    if (hBitmap[page] == NULL) { 
	allocate_new_graphic_page(page);
    } else { 
	SelectObject(hdc[1], hBitmap[page]);	    
    }
    if (!bgiemu_handle_redraw && active_page == visual_page) {
	POINT pos;
	GetCurrentPositionEx(hdc[0], &pos);
	MoveToEx(hdc[1], pos.x, pos.y, NULL);
    }
    active_page = page;
}


void setvisualpage(int page)
{
    POINT pos;
    if (hdc[page] == NULL) { 
	allocate_new_graphic_page(page);
    }
    if (!bgiemu_handle_redraw && active_page == visual_page) { 
	SelectObject(hdc[1], hBitmap[visual_page]);	    
	SelectClipRgn(hdc[1], NULL);
        BitBlt(hdc[1], -view_settings.left, -view_settings.top, 
	       window_width, window_height, 
	       hdc[0], -view_settings.left, -view_settings.top, 
	       SRCCOPY);
	SelectClipRgn(hdc[1], hRgn);
	GetCurrentPositionEx(hdc[0], &pos);
	MoveToEx(hdc[1], pos.x, pos.y, NULL);
    }
    SelectClipRgn(hdc[0], NULL);
    SelectClipRgn(hdc[1], NULL);
    SelectObject(hdc[1], hBitmap[page]);	    
    BitBlt(hdc[0], -view_settings.left, 
	   -view_settings.top, window_width, window_height, 
	   hdc[1], -view_settings.left, -view_settings.top, SRCCOPY);
    SelectClipRgn(hdc[0], hRgn);
    SelectClipRgn(hdc[1], hRgn);

    if (page != active_page) { 
	SelectObject(hdc[1], hBitmap[active_page]);	    
    }
    if (active_page != visual_page) { 
	GetCurrentPositionEx(hdc[1], &pos);
	MoveToEx(hdc[0], pos.x, pos.y, NULL);
    }
    visual_page = page;
}


void setaspectratio(int ax, int ay)
{
    aspect_ratio_x = ax;
    aspect_ratio_y = ay;
}

void getaspectratio(int* ax, int* ay)
{
    *ax = aspect_ratio_x;
    *ay = aspect_ratio_y;
}

void circle(int x, int y, int radius)
{
    pcache.select(color+BG); 
    int ry = (unsigned)radius*aspect_ratio_x/aspect_ratio_y;
    int rx = radius;
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	Arc(hdc[1], x-rx, y-ry, x+rx, y+ry, x+rx, y, x+rx, y);
    }
    if (visual_page == active_page) { 
        Arc(hdc[0], x-rx, y-ry, x+rx, y+ry, x+rx, y, x+rx, y);
    }    
}

void arc(int x, int y, int start_angle, int end_angle, int radius)
{
    ac.x = x;
    ac.y = y;
    ac.xstart = x + int(radius*cos(start_angle*pi/180.0));
    ac.ystart = y - int(radius*sin(start_angle*pi/180.0));
    ac.xend = x + int(radius*cos(end_angle*pi/180.0));
    ac.yend = y - int(radius*sin(end_angle*pi/180.0));

    if (bgiemu_handle_redraw || visual_page != active_page) { 
        Arc(hdc[1], x-radius, y-radius, x+radius, y+radius, 
	ac.xstart, ac.ystart, ac.xend, ac.yend);
    }
    if (visual_page == active_page) { 
	Arc(hdc[0], x-radius, y-radius, x+radius, y+radius, 
	    ac.xstart, ac.ystart, ac.xend, ac.yend);
    }
}

void getarccoords(arccoordstype *arccoords)
{
    *arccoords = ac;
}

void pieslice(int x, int y, int start_angle, int end_angle, 
	      int radius)
{
    pcache.select(color+BG); 
    select_fill_color();
    ac.x = x;
    ac.y = y;
    ac.xstart = x + int(radius*cos(start_angle*pi/180.0));
    ac.ystart = y - int(radius*sin(start_angle*pi/180.0));
    ac.xend = x + int(radius*cos(end_angle*pi/180.0));
    ac.yend = y - int(radius*sin(end_angle*pi/180.0));

    if (bgiemu_handle_redraw || visual_page != active_page) { 
	Pie(hdc[1], x-radius, y-radius, x+radius, y+radius, 
	    ac.xstart, ac.ystart, ac.xend, ac.yend); 
    }
    if (visual_page == active_page) { 
	Pie(hdc[0], x-radius, y-radius, x+radius, y+radius, 
    	    ac.xstart, ac.ystart, ac.xend, ac.yend); 
    }
}


void sector(int x, int y, int start_angle, int end_angle, 
		      int rx, int ry)
{
    ac.x = x;
    ac.y = y;
    arc_coords(start_angle, rx, ry, ac.xstart, ac.ystart);
    arc_coords(end_angle, rx, ry, ac.xend, ac.yend);
    ac.xstart += x; ac.ystart += y;
    ac.xend += x; ac.yend += y;

    pcache.select(color+BG); 
    if (bgiemu_handle_redraw || visual_page != active_page) { 
        Pie(hdc[1], x-rx, y-ry, x+rx, y+ry, 
	    ac.xstart, ac.ystart, ac.xend, ac.yend); 
    }
    if (visual_page == active_page) { 
	Pie(hdc[0], x-rx, y-ry, x+rx, y+ry, 
    	    ac.xstart, ac.ystart, ac.xend, ac.yend); 
    }
}

void bar(int left, int top, int right, int bottom)
{
    RECT r;
    if (left > right) {	/* Turbo C corrects for badly ordered corners */   
	r.left = right;
	r.right = left;
    } else {
	r.left = left;
	r.right = right;
    }
    if (bottom < top) {	/* Turbo C corrects for badly ordered corners */   
	r.top = bottom;
	r.bottom = top;
    } else {
	r.top = top;
	r.bottom = bottom;
    }
    select_fill_color();
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	FillRect(hdc[1], &r, hBrush[fill_settings.pattern]);
    }
    if (visual_page == active_page) { 
	FillRect(hdc[0], &r, hBrush[fill_settings.pattern]);
    }
}


void bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    int temp;
    const double tan30 = 1.0/1.73205080756887729352;
    if (left > right) {     /* Turbo C corrects for badly ordered corners */
	temp = left;
	left = right;
	right = temp;
    }
    if (bottom < top) {
	temp = bottom;
	bottom = top;
	top = temp;
    }
    bar(left+line_settings.thickness, top+line_settings.thickness, 
	right-line_settings.thickness+1, bottom-line_settings.thickness+1);

    if (write_mode != COPY_PUT) { 
	SetROP2(hdc[0], write_mode_cnv[write_mode]);
	SetROP2(hdc[1], write_mode_cnv[write_mode]);
    } 
    pcache.select(ADJUSTED_MODE(write_mode) ? color : color + BG);
    int dy = int(depth*tan30);
    POINT p[11];
    p[0].x = right, p[0].y = bottom;
    p[1].x = right, p[1].y = top;
    p[2].x = left,  p[2].y = top;
    p[3].x = left,  p[3].y = bottom;
    p[4].x = right, p[4].y = bottom;
    p[5].x = right+depth, p[5].y = bottom-dy;
    p[6].x = right+depth, p[6].y = top-dy;
    p[7].x = right, p[7].y = top;

    if (topflag) { 
	p[8].x = right+depth, p[8].y = top-dy;
	p[9].x = left+depth, p[9].y = top-dy;
	p[10].x = left, p[10].y = top;	
    }
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	Polyline(hdc[1], p, topflag ? 11 : 8);
    }
    if (visual_page == active_page) { 
	Polyline(hdc[0], p, topflag ? 11 : 8);
    }
    if (write_mode != COPY_PUT) { 
	SetROP2(hdc[0], R2_COPYPEN);
	SetROP2(hdc[1], R2_COPYPEN);
    }
}

void lineto(int x, int y)
{
    if (write_mode != COPY_PUT) { 
	SetROP2(hdc[0], write_mode_cnv[write_mode]);
	SetROP2(hdc[1], write_mode_cnv[write_mode]);
    } 
    pcache.select(ADJUSTED_MODE(write_mode) ? color : color + BG);
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	LineTo(hdc[1], x, y);
    }
    if (visual_page == active_page) { 
	LineTo(hdc[0], x, y);
    }
    if (write_mode != COPY_PUT) { 
	SetROP2(hdc[0], R2_COPYPEN);
	SetROP2(hdc[1], R2_COPYPEN);
    }
}

void linerel(int dx, int dy)
{
    POINT pos;
    GetCurrentPositionEx(hdc[1], &pos);
    lineto(pos.x + dx, pos.y + dy);
}

void drawpoly(int n_points, int* points) 
{ 
    if (write_mode != COPY_PUT) { 
	SetROP2(hdc[0], write_mode_cnv[write_mode]);
	SetROP2(hdc[1], write_mode_cnv[write_mode]);
    } 
    pcache.select(ADJUSTED_MODE(write_mode) ? color : color + BG);

    if (bgiemu_handle_redraw || visual_page != active_page) { 
	Polyline(hdc[1], (POINT*)points, n_points);
    }
    if (visual_page == active_page) { 
	Polyline(hdc[0], (POINT*)points, n_points);
    }

    if (write_mode != COPY_PUT) { 
	SetROP2(hdc[0], R2_COPYPEN);
	SetROP2(hdc[1], R2_COPYPEN);
    }
}

void line(int x0, int y0, int x1, int y1)
{
    POINT line[2];
    line[0].x = x0;
    line[0].y = y0;
    line[1].x = x1;
    line[1].y = y1;
    drawpoly(2, (int*)&line);
}

void rectangle(int left, int top, int right, int bottom)
{
    POINT rect[5];
    rect[0].x = left, rect[0].y = top;
    rect[1].x = right, rect[1].y = top;
    rect[2].x = right, rect[2].y = bottom;
    rect[3].x = left, rect[3].y = bottom;
    rect[4].x = left, rect[4].y = top;
    drawpoly(5, (int*)&rect);
}   
    
void fillpoly(int n_points, int* points)
{
    pcache.select(color+BG);
    select_fill_color();
    if (bgiemu_handle_redraw || visual_page != active_page) { 
        Polygon(hdc[1], (POINT*)points, n_points);
    }
    if (visual_page == active_page) { 
	Polygon(hdc[0], (POINT*)points, n_points);
    }
}

void floodfill(int x, int y, int border)
{
    select_fill_color();
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	FloodFill(hdc[1], x, y, PALETTEINDEX(border+BG));
    }
    if (visual_page == active_page) { 
	FloodFill(hdc[0], x, y, PALETTEINDEX(border+BG));
    } 
}


    
static bool handle_input(bool wait = 0)
{
    MSG lpMsg;
    if (wait ? GetMessage(&lpMsg, NULL, 0, 0) 
	     : PeekMessage(&lpMsg, NULL, 0, 0, PM_REMOVE)) 
    {
	TranslateMessage(&lpMsg);
	DispatchMessage(&lpMsg);
	return true;
    }
    return false;
}


void delay(unsigned msec)
{
    timeout_expired = false;
    SetTimer(hWnd, TIMER_ID, msec, NULL); 
    while (!timeout_expired) handle_input(true);
}
    
// The Mouse functions    (1-Oct-2000, Matthew Weathers)
bool mouseup() {
    while (handle_input(false));
	if (bMouseUp) {
		bMouseUp=false;
		return true;
	} else {
		return false;
	}
}
bool mousedown() {
    while (handle_input(false));
	if (bMouseDown) {
		bMouseDown=false;
		return true;
	} else {
		return false;
	}
}
void clearmouse() {
	iClickedMouseX=0;
	iClickedMouseY=0;
	iCurrentMouseX=0;
	iCurrentMouseY=0;
	bMouseDown=false;
	bMouseUp=false;
}
int mouseclickx() {
	return iClickedMouseX;
}
int mouseclicky(){
	return iClickedMouseY;
}
int mousecurrentx(){
	return iCurrentMouseX;
}
int mousecurrenty(){
	return iCurrentMouseY;
}
int whichmousebutton(){
	return iWhichMouseButton;
}

int kbhit()
{
    while (handle_input(false));
    return !kbd_queue.is_empty();
}

int getch()
{
    while (kbd_queue.is_empty()) handle_input(true);
    return (unsigned char)kbd_queue.get();
}

void cleardevice()
{	    
    RECT scr;
    scr.left = -view_settings.left;
    scr.top = -view_settings.top; 
    scr.right = screen_width-view_settings.left-1;
    scr.bottom = screen_height-view_settings.top-1;

    if (bgiemu_handle_redraw || visual_page != active_page) { 
	if (hRgn != NULL) { 
	    SelectClipRgn(hdc[1], NULL);
	}
	FillRect(hdc[1], &scr, hBackgroundBrush);
	if (hRgn != NULL) { 
	    SelectClipRgn(hdc[1], hRgn);
	}
    }
    if (visual_page == active_page) { 
	if (hRgn != NULL) { 
	    SelectClipRgn(hdc[0], NULL);
	}
	FillRect(hdc[0], &scr, hBackgroundBrush);
	if (hRgn != NULL) { 
	    SelectClipRgn(hdc[0], hRgn);
	}
    }
    moveto(0,0);
}

void clearviewport()
{
    RECT scr;
    scr.left = 0;
    scr.top = 0; 
    scr.right = view_settings.right-view_settings.left;
    scr.bottom = view_settings.bottom-view_settings.top;
    if (bgiemu_handle_redraw || visual_page != active_page) { 
        FillRect(hdc[1], &scr, hBackgroundBrush);
    }
    if (visual_page == active_page) { 
	FillRect(hdc[0], &scr, hBackgroundBrush);
    }
    moveto(0,0);
}

void detectgraph(int *graphdriver, int *graphmode)
{
    *graphdriver = VGA;
    *graphmode = bgiemu_default_mode;
}

int getgraphmode()
{
    return bgiemu_default_mode;
}

void setgraphmode(int) {}

void putimage(int x, int y, void* image, int bitblt)
{
    BGIimage* bi = (BGIimage*)image;
    static int putimage_width, putimage_height;

    if (hPutimageBitmap == NULL ||
	putimage_width < bi->width || putimage_height < bi->height)
    {
	if (putimage_width < bi->width) { 
	    putimage_width = (bi->width+7) & ~7;
	}
	if (putimage_height < bi->height) { 
	    putimage_height = bi->height;
	}
	HBITMAP h = CreateCompatibleBitmap(hdc[0], putimage_width, 
					   putimage_height);
	SelectObject(hdc[2], h);
	if (hPutimageBitmap) { 
	    DeleteObject(hPutimageBitmap);
	}
	hPutimageBitmap = h;
    }
    int mask = ADJUSTED_MODE(bitblt) ? 0 : BG;
    for (int i = 0; i <= MAXCOLORS; i++) { 
	bminfo.color_table[i] = i + mask;
    }
    bminfo.hdr.biHeight = bi->height; 
    bminfo.hdr.biWidth = bi->width; 
    SetDIBits(hdc[2], hPutimageBitmap, 0, bi->height, bi->bits, 
	      (BITMAPINFO*)&bminfo, DIB_PAL_COLORS);
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	BitBlt(hdc[1], x, y, bi->width, bi->height, hdc[2], 0, 0, 
	       bitblt_mode_cnv[bitblt]);
    }
    if (visual_page == active_page) { 
        BitBlt(hdc[0], x, y, bi->width, bi->height, hdc[2], 0, 0, 
	       bitblt_mode_cnv[bitblt]);
    }
}

unsigned int imagesize(int x1, int y1, int x2, int y2)
{
    return 8 + (((x2-x1+8) & ~7) >> 1)*(y2-y1+1); 
}

void getimage(int x1, int y1, int x2, int y2, void* image)
{
    BGIimage* bi = (BGIimage*)image;
    int* image_bits; 
    bi->width = x2-x1+1;
    bi->height = y2-y1+1;
    bminfo.hdr.biHeight = bi->height; 
    bminfo.hdr.biWidth = bi->width; 
    for (int i = 0; i <= MAXCOLORS; i++) { 
	bminfo.color_table[i] = i + BG;
    }
    HBITMAP hb = CreateDIBSection(hdc[3], (BITMAPINFO*)&bminfo, 
	DIB_PAL_COLORS, (void**)&image_bits, 0, 0); 
    HBITMAP hdb = (HBITMAP__*) SelectObject(hdc[3], hb);
    BitBlt(hdc[3], 0, 0, bi->width, bi->height, 
	   hdc[visual_page != active_page || bgiemu_handle_redraw ? 1 : 0], 
	   x1, y1, SRCCOPY);
    GdiFlush();
    memcpy(bi->bits, image_bits, (((bi->width+7) & ~7) >> 1)*bi->height);
    SelectObject(hdc[3], hdb);
    DeleteObject(hb);
}

unsigned int getpixel(int x, int y)
{ 
    int color;
    COLORREF rgb = GetPixel(hdc[visual_page != active_page 
			       || bgiemu_handle_redraw ? 1 : 0], x, y);

    if (rgb == CLR_INVALID) { 
	return -1;
    }
    int red = GetRValue(rgb);
    int blue = GetBValue(rgb);
    int green = GetGValue(rgb);
    for (color = 0; color <= MAXCOLORS; color++) { 
	if (BGIpalette[color].peRed == red &&
	    BGIpalette[color].peGreen == green &&
	    BGIpalette[color].peBlue == blue)
	{
	    return color;
	}
    }
    return -1;
}
    	    
void putpixel(int x, int y, int c)
{
    c &= MAXCOLORS;
    if (bgiemu_handle_redraw || visual_page != active_page) { 
	SetPixel(hdc[1], x, y, PALETTEINDEX(c+BG));
    }
    if (visual_page == active_page) { 
	SetPixel(hdc[0], x, y, PALETTEINDEX(c+BG));
    }
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT messg, 
			 WPARAM wParam, LPARAM lParam)
{
    int i;
    static bool palette_changed = false;

    switch (messg) { 
      case WM_PAINT: 
	if (hdc[0] == 0) {
	    hdc[0] = BeginPaint(hWnd, &ps);
            SelectPalette(hdc[0], hPalette, FALSE);
	    RealizePalette(hdc[0]);
	    hdc[1] = CreateCompatibleDC(hdc[0]);
            SelectPalette(hdc[1], hPalette, FALSE);
	    hdc[2] = CreateCompatibleDC(hdc[0]);
            SelectPalette(hdc[2], hPalette, FALSE);
	    hdc[3] = CreateCompatibleDC(hdc[0]);
            SelectPalette(hdc[3], hPalette, FALSE);

	    screen_width = GetDeviceCaps(hdc[0], HORZRES);
	    screen_height = GetDeviceCaps(hdc[0], VERTRES);
	    hBitmap[active_page] = 
		CreateCompatibleBitmap(hdc[0], screen_width, screen_height);
	    SelectObject(hdc[1], hBitmap[active_page]);	    

	    SetTextColor(hdc[0], PALETTEINDEX(text_color+BG));
	    SetTextColor(hdc[1], PALETTEINDEX(text_color+BG));
	    SetBkColor(hdc[0], PALETTEINDEX(BG));
	    SetBkColor(hdc[1], PALETTEINDEX(BG));

	    SelectObject(hdc[0], hBrush[fill_settings.pattern]);
	    SelectObject(hdc[1], hBrush[fill_settings.pattern]);

	    RECT scr;
	    scr.left = -view_settings.left;
	    scr.top = -view_settings.top; 
	    scr.right = screen_width-view_settings.left-1;
	    scr.bottom = screen_height-view_settings.top-1;
	    FillRect(hdc[1], &scr, hBackgroundBrush);
	}
	if (hRgn != NULL) { 
	    SelectClipRgn(hdc[0], NULL);
	}
	if (visual_page != active_page) { 
	    SelectObject(hdc[1], hBitmap[visual_page]); 
	} 
        BitBlt(hdc[0], -view_settings.left, 
	       -view_settings.top, window_width, window_height, 
	       hdc[1], -view_settings.left, -view_settings.top, 
	       SRCCOPY);
	if (hRgn != NULL) { 
	    SelectClipRgn(hdc[0], hRgn);
	}
	if (visual_page != active_page) { 
	    SelectObject(hdc[1], hBitmap[active_page]); 
	} 
	ValidateRect(hWnd, NULL);
	break;
      case WM_SETFOCUS:
	if (palette_changed) { 
	    HPALETTE new_palette = CreatePalette(pPalette);
	    SelectPalette(hdc[0], new_palette, FALSE);
	    RealizePalette(hdc[0]);
	    SelectPalette(hdc[1], new_palette, FALSE);
	    SelectPalette(hdc[2], new_palette, FALSE);
	    SelectPalette(hdc[3], new_palette, FALSE);
	    DeleteObject(hPalette);
	    hPalette = new_palette;
	    palette_changed = false;
	}
	break;
      case WM_PALETTECHANGED: 
	RealizePalette(hdc[0]);
	UpdateColors(hdc[0]);
	palette_changed = true;
	break;
      case WM_DESTROY: 
        EndPaint(hWnd, &ps);
	hdc[0] = 0;
	DeleteObject(hdc[1]);
	DeleteObject(hdc[2]);
	DeleteObject(hdc[3]);
	if (hPutimageBitmap) { 
	    DeleteObject(hPutimageBitmap);
	    hPutimageBitmap = NULL;
	}
	for (i = 0; i < MAX_PAGES; i++) { 
	    if (hBitmap[i] != NULL) {
		DeleteObject(hBitmap[i]);
		hBitmap[i] = 0;
	    }
	}
	DeleteObject(hPalette);
	hPalette = 0;
	PostQuitMessage(0);
	break;
      case WM_SIZE: 
	window_width = LOWORD(lParam);
	window_height = HIWORD(lParam);
	break;
      case WM_TIMER:
	KillTimer(hWnd, TIMER_ID);
	timeout_expired = true;
	break;
      case WM_CHAR:
	kbd_queue.put((TCHAR) wParam);
	break;

		// Handle some mouse events, too (1-Oct-2000, Matthew Weathers, Erik Habbestad)
	  case WM_LBUTTONDOWN:
		  iClickedMouseX = LOWORD(lParam);
		  iClickedMouseY = HIWORD(lParam);
		  bMouseDown = true;
		  iWhichMouseButton = LEFT_BUTTON;
		  break;
	  case WM_LBUTTONUP:
		  iClickedMouseX = LOWORD(lParam);
		  iClickedMouseY = HIWORD(lParam);
		  bMouseUp = true;
		  iWhichMouseButton = LEFT_BUTTON;
		  break;
	  case WM_RBUTTONDOWN:
		  iClickedMouseX = LOWORD(lParam);
		  iClickedMouseY = HIWORD(lParam);
		  bMouseDown = true;
		  iWhichMouseButton = RIGHT_BUTTON;
		  break;
	  case WM_RBUTTONUP:
		  iClickedMouseX = LOWORD(lParam);
		  iClickedMouseY = HIWORD(lParam);
		  bMouseUp = true;
		  iWhichMouseButton = RIGHT_BUTTON;
		  break;
	  case WM_MOUSEMOVE:
		  iCurrentMouseX = LOWORD(lParam);
		  iCurrentMouseY = HIWORD(lParam);
		  break;

      default:
	return DefWindowProc(hWnd, messg, wParam, lParam);
    }
    return 0;
}

void closegraph()
{
    DestroyWindow(hWnd);
    while(handle_input(true));
}


static void detect_mode(int* gd, int* gm)
{
    switch (*gd) {
      case CGA:
	window_height = 200;
	switch (*gm) {
  	  case CGAC0:
	  case CGAC1:
	  case CGAC2:
	  case CGAC3:
	    window_width = 320;
	    break;
	  case CGAHI:
	    window_width = 640;
	    break;
	  default:
	    window_width = 320;
	    break;
	}
	break;
      case MCGA:
	window_height = 200;
	switch (*gm) {
	  case MCGAC0:
	  case MCGAC1:
	  case MCGAC2:
	  case MCGAC3:
	    window_width = 320;
	    break;
	  case MCGAMED:
  	    window_width = 640;
	    break;
	  case MCGAHI:
	    window_width = 640;
	    window_height = 480;
	    break;
	  default:
	    window_width = 320;
	    break;
	}
	break;
      case EGA:
	window_width = 640;
	switch (*gm) {
	  case EGALO:
	    window_height = 200;
	    break;
	  case EGAHI:
	    window_height = 350;
	    break;
	  default:
	    window_height = 350;
	    break;
	}
	break;
      case EGA64:
        window_width = 640;
	switch (*gm) {
	  case EGA64LO:
	    window_height = 200;
	    break;
	  case EGA64HI:
	    window_height = 350;
	    break;
	  default:
	    window_height = 350;
	    break;
	}
	break;
      case EGAMONO:
	window_width = 640;
	window_height = 350;
	break;
      case HERCMONO:
	window_width = 720;
	window_height = 348;
	break;
      case ATT400:
	window_height = 200;
	switch (*gm) {
	  case ATT400C0:
	  case ATT400C1:
	  case ATT400C2:
	  case ATT400C3:
	    window_width = 320;
	    break;
	  case ATT400MED:
	    window_width = 640;
	    break;
	  case ATT400HI:
	    window_width = 640;
	    window_height = 400;
	    break;
	  default:
	    window_width = 320;
	    break;
	}
	break;
      default:
      case DETECT:
	*gd = VGA;
	*gm = bgiemu_default_mode;		   
      case VGA:
        window_width = 900; //Default WIDTH
        switch (*gm) {
          case VGALO:
	    window_height = 200;
	    break;
	  case VGAMED:
	    window_height = 350;
	    break;
	  case VGAHI:
	    window_height = 480;
	    break;
	  default:
	    window_height = 640; //DEFAULT HEIGHT
	    break;
	}
	break;
      case PC3270:
	window_width = 720;
	window_height = 350;
	break;
      case IBM8514:
	switch (*gm) {
  	  case IBM8514LO:
 	    window_width = 640;
	    window_height = 480;
	    break;
	  case IBM8514HI:
	    window_width = 1024;
	    window_height = 768;
	    break;
	  default:
	    window_width = 1024;
	    window_height = 768;
	    break;
	}
	break;	
    } 
}

static void set_defaults()
{
    color = text_color = WHITE;
    bkcolor = 0;
    line_settings.thickness = 1;
    line_settings.linestyle = SOLID_LINE;
    line_settings.upattern = ~0;
    fill_settings.pattern = SOLID_FILL;
    fill_settings.color = WHITE;
    write_mode = COPY_PUT;

    text_settings.direction = HORIZ_DIR;
    text_settings.font = DEFAULT_FONT;
    text_settings.charsize = 1;
    text_settings.horiz = LEFT_TEXT;
    text_settings.vert = TOP_TEXT;
    text_align_mode = ALIGN_NOT_SET;

    active_page = visual_page = 0;
    
    view_settings.left = 0;
    view_settings.top = 0;
    view_settings.right = window_width-1;
    view_settings.bottom = window_height-1;
    
    aspect_ratio_x = aspect_ratio_y = 10000;
}

void initgraph(int* device, int* mode, char const* /*pathtodriver*/, 
			   int size_width, int size_height)
{
    int index;
    static WNDCLASS wcApp;

    gdi_error_code = grOk;

    if (wcApp.lpszClassName == NULL) { 
	wcApp.lpszClassName = "BGIlibrary";
	wcApp.hInstance = 0;
	wcApp.lpfnWndProc = WndProc;
	wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcApp.hIcon = 0;
	wcApp.lpszMenuName = 0;
	wcApp.hbrBackground = (HBRUSH__ *) GetStockObject(BLACK_BRUSH);
	wcApp.style = CS_SAVEBITS;
	wcApp.cbClsExtra = 0;
	wcApp.cbWndExtra = 0;
	
	if (!RegisterClass(&wcApp)) { 
	    gdi_error_code = GetLastError();
	    return;
	}
	
	pPalette = (NPLOGPALETTE)LocalAlloc(LMEM_FIXED,
	    sizeof(LOGPALETTE)+sizeof(PALETTEENTRY)*PALETTE_SIZE);
	
	pPalette->palVersion = 0x300;
	pPalette->palNumEntries = PALETTE_SIZE;
	memset(pPalette->palPalEntry, 0, sizeof(PALETTEENTRY)*PALETTE_SIZE); 
	for (index = 0; index < BG; index++) {
	    pPalette->palPalEntry[index].peFlags = PC_EXPLICIT;
	    pPalette->palPalEntry[index].peRed = index;
	    pPalette->palPalEntry[PALETTE_SIZE-BG+index].peFlags = PC_EXPLICIT;
	    pPalette->palPalEntry[PALETTE_SIZE-BG+index].peRed = 
		PALETTE_SIZE-BG+index;
	}		
	hBackgroundBrush = CreateSolidBrush(PALETTEINDEX(BG));
	hBrush[EMPTY_FILL] = (HBRUSH__*) GetStockObject(NULL_BRUSH);
	hBrush[SOLID_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, SolidBrushBitmap));
	hBrush[LINE_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, LineBrushBitmap));
	hBrush[LTSLASH_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, LtslashBrushBitmap));
	hBrush[SLASH_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, SlashBrushBitmap));
	hBrush[BKSLASH_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, BkslashBrushBitmap));
	hBrush[LTBKSLASH_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, LtbkslashBrushBitmap));
	hBrush[HATCH_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, HatchBrushBitmap));
	hBrush[XHATCH_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, XhatchBrushBitmap));
	hBrush[INTERLEAVE_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, InterleaveBrushBitmap));
	hBrush[WIDE_DOT_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, WidedotBrushBitmap));
	hBrush[CLOSE_DOT_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, ClosedotBrushBitmap));
	hBrush[USER_FILL] = 
	    CreatePatternBrush(CreateBitmap(8, 8, 1, 1, SolidBrushBitmap));
    }
    memcpy(BGIpalette, BGIcolor, sizeof BGIpalette);
    current_palette.size = MAXCOLORS+1;
    for (index = 10; index <= MAXCOLORS; index++) {
	pPalette->palPalEntry[index] = BGIcolor[0];
    }
    for (index = 0; index <= MAXCOLORS; index++) {
	current_palette.colors[index] = index;
	pPalette->palPalEntry[index+BG] = BGIcolor[index];
    }
    hPalette = CreatePalette(pPalette);
    detect_mode(device, mode);
    set_defaults();

	if (size_width) window_width=size_width;
	if (size_height) window_height=size_height;

    hWnd = CreateWindow("BGIlibrary", "Windows BGI", 
			WS_OVERLAPPEDWINDOW,
		        0, 0, window_width+BORDER_WIDTH, 
			window_height+BORDER_HEIGHT,
			(HWND)NULL,  (HMENU)NULL,
	    		0, NULL);
    if (hWnd == NULL) { 
	gdi_error_code = GetLastError();
	return;
    }
    ShowWindow(hWnd, *mode == VGAMAX ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    UpdateWindow(hWnd);
}


void graphdefaults()
{
    set_defaults();

    for (int i = 0; i <= MAXCOLORS; i++) { 
	current_palette.colors[i] = i;
	BGIpalette[i] = BGIcolor[i];
    }
    SetPaletteEntries(hPalette, BG, MAXCOLORS+1, BGIpalette);
    RealizePalette(hdc[0]);

    SetTextColor(hdc[0], PALETTEINDEX(text_color+BG));
    SetTextColor(hdc[1], PALETTEINDEX(text_color+BG));
    SetBkColor(hdc[0], PALETTEINDEX(BG));
    SetBkColor(hdc[1], PALETTEINDEX(BG));

    SelectClipRgn(hdc[0], NULL);
    SelectClipRgn(hdc[1], NULL);
    SetViewportOrgEx(hdc[0], 0, 0, NULL);
    SetViewportOrgEx(hdc[1], 0, 0, NULL);

    SelectObject(hdc[0], hBrush[fill_settings.pattern]);
    SelectObject(hdc[1], hBrush[fill_settings.pattern]);

    moveto(0,0);
}

void restorecrtmode() {}

#else // !_WIN32 -- macOS / Linux backend built on SDL2 + SDL2_ttf
////////////////////////////////////////////////////////////////////////
//   SDL2-based re-implementation of the WinBGIm API above, so that the
//   same graphics.h calls used by main.cpp work unchanged on macOS/Linux.
//   Requires: brew install sdl2 sdl2_ttf (macOS) / libsdl2-dev libsdl2-ttf-dev (Linux)
////////////////////////////////////////////////////////////////////////

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <utility>
#include <algorithm>
#include "graphics.h"

int bgiemu_handle_redraw = 1;
int bgiemu_default_mode = VGAHI;

namespace {

struct RGB { unsigned char r, g, b; };

RGB g_palette_rgb[64] = {
    {0,0,0},       {0,0,255},     {0,255,0},     {0,255,255},
    {255,0,0},     {255,0,255},   {165,42,42},   {211,211,211},
    {47,79,79},    {173,216,230}, {32,178,170},  {224,255,255},
    {240,128,128}, {219,112,147}, {255,255,0},   {255,255,255},
    {0xF0,0xF8,0xFF}, {0xFA,0xEB,0xD7}, {0x22,0x85,0xFF}, {0x7F,0xFF,0xD4},
    {0xF0,0xFF,0xFF}, {0xF5,0xF5,0xDC}, {0xFF,0xE4,0xC4}, {0xFF,0x7B,0xCD},
    {0x00,0x00,0xFF}, {0x8A,0x2B,0xE2}, {0xA5,0x2A,0x2A}, {0xDE,0xB8,0x87},
    {0x5F,0x9E,0xA0}, {0x7F,0xFF,0x00}, {0xD2,0x50,0x1E}, {0xFF,0x7F,0x50},
    {0x64,0x95,0xED}, {0xFF,0xF8,0xDC}, {0xDC,0x14,0x3C}, {0x68,0xCF,0xDF},
    {0x00,0x00,0x8B}, {0x00,0x8B,0x8B}, {0xB8,0x86,0x0B}, {0xA9,0xA9,0xA9},
    {0x00,0x64,0x00}, {0xBD,0xB7,0x6B}, {0x8B,0x00,0x8B}, {0x55,0x6B,0x2F},
    {0xFF,0x8C,0x00}, {0xB9,0x82,0xFC}, {0x8B,0x00,0x00}, {0xE9,0x96,0x7A},
    {0x8F,0xBC,0x8F}, {0x48,0x3D,0x8B}, {0x2F,0x4F,0x4F}, {0x00,0xCE,0xD1},
    {0x94,0x00,0xD3}, {0xFF,0x14,0x93}, {0x00,0xBF,0xFF}, {0x69,0x69,0x69},
    {0x1E,0x90,0xFF}, {0xB2,0x22,0x22}, {0xFF,0xFA,0xF0}, {0x22,0x8B,0x22},
    {0xFF,0x00,0xFF}, {0xDC,0xDC,0xDC}, {0xF8,0xF8,0xBF}, {0xFF,0xD7,0x00},
};
int g_palette_map[64];

struct { int width; int height; } font_metrics[][11] = {
{{0,0},{8,8},{16,16},{24,24},{32,32},{40,40},{48,48},{56,56},{64,64},{72,72},{80,80}},
{{0,0},{13,18},{14,20},{16,23},{22,31},{29,41},{36,51},{44,62},{55,77},{66,93},{88,124}},
{{0,0},{3,5},{4,6},{4,6},{6,9},{8,12},{10,15},{12,18},{15,22},{18,27},{24,36}},
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}},
{{0,0},{13,19},{14,21},{16,24},{22,32},{29,42},{36,53},{44,64},{55,80},{66,96},{88,128}},
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}},
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}},
{{0,0},{13,18},{14,20},{16,23},{22,31},{29,41},{36,51},{44,62},{55,77},{66,93},{88,124}},
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}},
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}},
{{0,0},{11,19},{12,21},{14,24},{19,32},{25,42},{31,53},{38,64},{47,80},{57,96},{76,128}}
};
int normal_font_size[] = {1,4,4,4,4,4,4,4,4,4,4};

const char* kFontCandidates[] = {
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
};

class char_queue {
    std::vector<char> buf;
    int put_pos = 0, get_pos = 0;
  public:
    void put(char ch) {
        buf[put_pos] = ch;
        if (++put_pos == (int)buf.size()) put_pos = 0;
        if (put_pos == get_pos && !is_empty()) (void)get();
    }
    char get() {
        char ch = buf[get_pos];
        if (++get_pos == (int)buf.size()) get_pos = 0;
        return ch;
    }
    bool is_empty() { return get_pos == put_pos; }
    char_queue(int size = 256) : buf(size) {}
};

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
SDL_Surface* g_pages[16] = {nullptr};
int g_window_width = 0, g_window_height = 0;
int g_active_page = 0, g_visual_page = 0;

int g_color = WHITE, g_bkcolor = BLACK;
int g_cur_x = 0, g_cur_y = 0;
int g_write_mode = COPY_PUT;
int g_font_mul_x = 1, g_font_div_x = 1, g_font_mul_y = 1, g_font_div_y = 1;
int g_gdi_error_code = grOk;

linesettingstype g_line_settings = {SOLID_LINE, 0xFFFF, NORM_WIDTH};
fillsettingstype g_fill_settings = {SOLID_FILL, WHITE};
textsettingstype g_text_settings = {DEFAULT_FONT, HORIZ_DIR, 1, LEFT_TEXT, TOP_TEXT};
viewporttype g_view_settings = {0, 0, 0, 0, CLIP_OFF};
arccoordstype g_ac;
palettetype g_current_palette;
fillpatterntype g_user_fill_pattern = {-1,-1,-1,-1,-1,-1,-1,-1};

char_queue g_kbd_queue;
bool g_mouse_up = false, g_mouse_down = false;
int g_mouse_cur_x = 0, g_mouse_cur_y = 0;
int g_mouse_click_x = 0, g_mouse_click_y = 0;
int g_which_mouse_button = LEFT_BUTTON;

std::vector<std::pair<int, TTF_Font*>> g_font_cache;

SDL_Surface* active_surface() {
    return (g_active_page >= 0 && g_active_page < 16) ? g_pages[g_active_page] : nullptr;
}

void ensure_page(int page) {
    if (page < 0 || page >= 16 || g_pages[page] || g_window_width <= 0) return;
    g_pages[page] = SDL_CreateRGBSurfaceWithFormat(0, g_window_width, g_window_height, 32, SDL_PIXELFORMAT_RGBA32);
    RGB rgb = g_palette_rgb[g_palette_map[g_bkcolor & 63] & 63];
    SDL_FillRect(g_pages[page], nullptr, SDL_MapRGB(g_pages[page]->format, rgb.r, rgb.g, rgb.b));
}

Uint32 map_color(SDL_Surface* s, int c) {
    RGB rgb = g_palette_rgb[g_palette_map[c & 63] & 63];
    return SDL_MapRGB(s->format, rgb.r, rgb.g, rgb.b);
}

Uint32* pixel_ptr(SDL_Surface* s, int x, int y) {
    return (Uint32*)((Uint8*)s->pixels + y * s->pitch) + x;
}

void raw_putpixel(SDL_Surface* s, int x, int y, Uint32 col) {
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    *pixel_ptr(s, x, y) = col;
}

Uint32 raw_getpixel(SDL_Surface* s, int x, int y) {
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return 0;
    return *pixel_ptr(s, x, y);
}

void raw_line(SDL_Surface* s, int x0, int y0, int x1, int y1, Uint32 col) {
    if (!s) return;
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        raw_putpixel(s, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void raw_hline(SDL_Surface* s, int x0, int x1, int y, Uint32 col) {
    if (!s) return;
    if (x0 > x1) std::swap(x0, x1);
    SDL_Rect r{x0, y, x1 - x0 + 1, 1};
    SDL_FillRect(s, &r, col);
}

void raw_rect_fill(SDL_Surface* s, int x0, int y0, int x1, int y1, Uint32 col) {
    if (!s) return;
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    SDL_Rect r{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    SDL_FillRect(s, &r, col);
}

void raw_rect_outline(SDL_Surface* s, int x0, int y0, int x1, int y1, Uint32 col) {
    raw_line(s, x0, y0, x1, y0, col);
    raw_line(s, x1, y0, x1, y1, col);
    raw_line(s, x1, y1, x0, y1, col);
    raw_line(s, x0, y1, x0, y0, col);
}

void raw_circle(SDL_Surface* s, int cx, int cy, int r, Uint32 col) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        raw_putpixel(s, cx + x, cy + y, col); raw_putpixel(s, cx + y, cy + x, col);
        raw_putpixel(s, cx - y, cy + x, col); raw_putpixel(s, cx - x, cy + y, col);
        raw_putpixel(s, cx - x, cy - y, col); raw_putpixel(s, cx - y, cy - x, col);
        raw_putpixel(s, cx + y, cy - x, col); raw_putpixel(s, cx + x, cy - y, col);
        y++;
        if (err <= 0) err += 2 * y + 1;
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

void raw_fill_ellipse(SDL_Surface* s, int cx, int cy, int rx, int ry, Uint32 col) {
    if (!s || rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        int dx = (int)(rx * std::sqrt(1.0 - (double)(y * y) / (double)(ry * ry)));
        raw_hline(s, cx - dx, cx + dx, cy + y, col);
    }
}

void raw_ellipse_arc(SDL_Surface* s, int cx, int cy, int rx, int ry, double a0, double a1, Uint32 col) {
    if (!s) return;
    if (a1 < a0) a1 += 360.0;
    bool first = true;
    int lastx = 0, lasty = 0;
    for (double a = a0; a <= a1 + 0.001; a += 1.0) {
        double rad = a * M_PI / 180.0;
        int x = cx + (int)std::lround(rx * std::cos(rad));
        int y = cy - (int)std::lround(ry * std::sin(rad));
        if (!first) raw_line(s, lastx, lasty, x, y, col);
        lastx = x; lasty = y; first = false;
    }
}

void raw_fill_polygon(SDL_Surface* s, const std::vector<SDL_Point>& pts, Uint32 col) {
    int n = (int)pts.size();
    if (!s || n < 3) return;
    int miny = pts[0].y, maxy = pts[0].y;
    for (auto& p : pts) { miny = std::min(miny, p.y); maxy = std::max(maxy, p.y); }
    for (int y = miny; y <= maxy; y++) {
        std::vector<int> xs;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int y0 = pts[i].y, y1 = pts[j].y;
            if (y0 == y1) continue;
            if ((y >= y0 && y < y1) || (y >= y1 && y < y0)) {
                double t = (double)(y - y0) / (double)(y1 - y0);
                xs.push_back(pts[i].x + (int)std::lround(t * (pts[j].x - pts[i].x)));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) raw_hline(s, xs[k], xs[k + 1], y, col);
    }
}

TTF_Font* get_font(int px_height) {
    if (px_height < 6) px_height = 6;
    for (auto& kv : g_font_cache) if (kv.first == px_height) return kv.second;
    TTF_Font* f = nullptr;
    for (const char* path : kFontCandidates) {
        f = TTF_OpenFont(path, px_height);
        if (f) break;
    }
    g_font_cache.push_back({px_height, f});
    return f;
}

int font_pixel_height() {
    int fnt = g_text_settings.font;
    if (fnt < 0 || fnt > 10) fnt = 0;
    if (g_text_settings.charsize == 0) {
        int base = font_metrics[fnt][normal_font_size[fnt]].height;
        return base * g_font_mul_y / (g_font_div_y ? g_font_div_y : 1);
    }
    int idx = g_text_settings.charsize;
    if (idx > 10) idx = 10;
    return font_metrics[fnt][idx].height;
}

SDL_Color to_sdl_color(int c) {
    RGB rgb = g_palette_rgb[g_palette_map[c & 63] & 63];
    return SDL_Color{rgb.r, rgb.g, rgb.b, 255};
}

void render_text(int x, int y, const char* str) {
    if (!str || !*str || !active_surface()) return;
    TTF_Font* f = get_font(font_pixel_height());
    if (!f) return;
    SDL_Surface* txt = TTF_RenderText_Shaded(f, str, to_sdl_color(g_color), to_sdl_color(g_bkcolor));
    if (!txt) return;
    int w = txt->w, h = txt->h;
    int dx = x, dy = y;
    if (g_text_settings.horiz == CENTER_TEXT) dx -= w / 2;
    else if (g_text_settings.horiz == RIGHT_TEXT) dx -= w;
    if (g_text_settings.vert == CENTER_TEXT) dy -= h / 2;
    else if (g_text_settings.vert == BOTTOM_TEXT) dy -= h;
    SDL_Rect dst{dx, dy, w, h};
    SDL_BlitSurface(txt, nullptr, active_surface(), &dst);
    SDL_FreeSurface(txt);
}

void present_page() {
    if (!g_renderer || !g_pages[g_visual_page]) return;
    if (g_texture) { SDL_DestroyTexture(g_texture); g_texture = nullptr; }
    g_texture = SDL_CreateTextureFromSurface(g_renderer, g_pages[g_visual_page]);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);
}

void process_event(const SDL_Event& e) {
    switch (e.type) {
      case SDL_QUIT:
        exit(0);
      case SDL_TEXTINPUT:
        if (e.text.text[0]) g_kbd_queue.put(e.text.text[0]);
        break;
      case SDL_KEYDOWN:
        switch (e.key.keysym.sym) {
          case SDLK_RETURN: case SDLK_KP_ENTER: g_kbd_queue.put(13); break;
          case SDLK_BACKSPACE: g_kbd_queue.put(8); break;
          case SDLK_ESCAPE: g_kbd_queue.put(27); break;
          case SDLK_TAB: g_kbd_queue.put(9); break;
          default: break;
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        g_mouse_click_x = e.button.x; g_mouse_click_y = e.button.y;
        g_mouse_down = true;
        g_which_mouse_button = (e.button.button == SDL_BUTTON_RIGHT) ? RIGHT_BUTTON : LEFT_BUTTON;
        break;
      case SDL_MOUSEBUTTONUP:
        g_mouse_click_x = e.button.x; g_mouse_click_y = e.button.y;
        g_mouse_up = true;
        g_which_mouse_button = (e.button.button == SDL_BUTTON_RIGHT) ? RIGHT_BUTTON : LEFT_BUTTON;
        break;
      case SDL_MOUSEMOTION:
        g_mouse_cur_x = e.motion.x; g_mouse_cur_y = e.motion.y;
        break;
      case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_EXPOSED) present_page();
        break;
      default: break;
    }
}

bool pump_events(bool wait) {
    SDL_Event e;
    if (wait) {
        if (SDL_WaitEvent(&e)) { process_event(e); return true; }
        return false;
    }
    bool any = false;
    while (SDL_PollEvent(&e)) { process_event(e); any = true; }
    return any;
}

} // namespace

void _graphfreemem(void* ptr, unsigned int) { free(ptr); }
void* _graphgetmem(unsigned int size) { return malloc(size); }

int graphresult() { return g_gdi_error_code; }

char* grapherrormsg(int code) {
    static char buf[256];
    snprintf(buf, sizeof buf, "graphics error %d", code);
    return buf;
}

void setcolor(int c) { g_color = c & MAXCOLORS; }
int getcolor() { return g_color; }
int getmaxcolor() { return WHITE; }
int getmaxmode() { return VGAMAX; }

char* getmodename(int mode) {
    static char buf[32];
    snprintf(buf, sizeof buf, "%d x %d %s", g_window_width, g_window_height, mode < 2 ? "EGA" : "VGA");
    return buf;
}

const char* getdrivername() { return "SDL2"; }

int getx() { return g_cur_x; }
int gety() { return g_cur_y; }
int getmaxx() { return g_window_width - 1; }
int getmaxy() { return g_window_height - 1; }

void setlinestyle(int style, unsigned int pattern, int thickness) {
    g_line_settings.linestyle = style;
    g_line_settings.thickness = thickness;
    g_line_settings.upattern = pattern;
}
void getlinesettings(linesettingstype* ls) { *ls = g_line_settings; }
void setwritemode(int mode) { g_write_mode = mode; }

void setpalette(int index, int color) {
    if (index < 0 || index >= 64) return;
    g_palette_map[index] = color & 63;
    g_current_palette.colors[index] = color & 63;
    if (index == 0) g_bkcolor = 0;
}
void setrgbpalette(int index, int red, int green, int blue) {
    if (index < 0 || index >= 64) return;
    g_palette_rgb[index] = RGB{(unsigned char)(red & 0xFC), (unsigned char)(green & 0xFC), (unsigned char)(blue & 0xFC)};
    if (index == 0) g_bkcolor = 0;
}
void setallpalette(palettetype* pal) {
    for (int i = 0; i < pal->size && i < 64; i++) g_palette_map[i] = pal->colors[i] & MAXCOLORS;
    g_bkcolor = 0;
}
palettetype* getdefaultpalette() {
    static palettetype default_palette = { 64,
      { BLACK, BLUE, GREEN, CYAN, RED, MAGENT, BROWN, LIGHTGRAY, DARKGRAY,
        LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
        50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
      }};
    return &default_palette;
}
void getpalette(palettetype* pal) { *pal = g_current_palette; }
int getpalettesize() { return MAXCOLORS + 1; }

void setbkcolor(int color) { g_bkcolor = color & MAXCOLORS; }
int getbkcolor() { return g_bkcolor; }

void setfillstyle(int style, int color) {
    g_fill_settings.pattern = style;
    g_fill_settings.color = color & MAXCOLORS;
}
void getfillsettings(fillsettingstype* fs) { *fs = g_fill_settings; }
void setfillpattern(char const* upattern, int color) {
    for (int i = 0; i < 8; i++) g_user_fill_pattern[i] = upattern[i];
    g_fill_settings.color = color & MAXCOLORS;
    g_fill_settings.pattern = USER_FILL;
}
void getfillpattern(char const* fp) { memcpy((void*)fp, g_user_fill_pattern, sizeof g_user_fill_pattern); }

void setusercharsize(int multx, int divx, int multy, int divy) {
    g_font_mul_x = multx; g_font_div_x = divx;
    g_font_mul_y = multy; g_font_div_y = divy;
    g_text_settings.charsize = 0;
}

void moveto(int x, int y) { g_cur_x = x; g_cur_y = y; }
void moverel(int dx, int dy) { moveto(g_cur_x + dx, g_cur_y + dy); }

void settextstyle(int font, int direction, int char_size) {
    if (char_size > 10) char_size = 10;
    g_text_settings.direction = direction;
    g_text_settings.font = font;
    g_text_settings.charsize = char_size;
}
void settextjustify(int horiz, int vert) { g_text_settings.horiz = horiz; g_text_settings.vert = vert; }
void gettextsettings(textsettingstype* ts) { *ts = g_text_settings; }

int textheight(char const* str) {
    TTF_Font* f = get_font(font_pixel_height());
    if (!f) return 0;
    int w, h;
    TTF_SizeText(f, str, &w, &h);
    return h;
}
int textwidth(char const* str) {
    TTF_Font* f = get_font(font_pixel_height());
    if (!f) return 0;
    int w, h;
    TTF_SizeText(f, str, &w, &h);
    return w;
}

void outtext(char const* str) { render_text(g_cur_x, g_cur_y, str); }
void outtextxy(int x, int y, char const* str) { render_text(x, y, str); }

void setviewport(int x1, int y1, int x2, int y2, int clip) {
    g_view_settings.left = x1; g_view_settings.top = y1;
    g_view_settings.right = x2; g_view_settings.bottom = y2;
    g_view_settings.clip = clip;
    moveto(0, 0);
}
void getviewsettings(viewporttype* viewport) { *viewport = g_view_settings; }

void ellipse(int x, int y, int start_angle, int end_angle, int rx, int ry) {
    raw_ellipse_arc(active_surface(), x, y, rx, ry, start_angle, end_angle, map_color(active_surface(), g_color));
}
void fillellipse(int x, int y, int rx, int ry) {
    if (g_fill_settings.pattern == EMPTY_FILL) return;
    raw_fill_ellipse(active_surface(), x, y, rx, ry, map_color(active_surface(), g_fill_settings.color));
}
void setactivepage(int page) { ensure_page(page); g_active_page = page; }
void setvisualpage(int page) { ensure_page(page); g_visual_page = page; present_page(); }
void setaspectratio(int, int) {}
void getaspectratio(int* ax, int* ay) { *ax = *ay = 10000; }

void circle(int x, int y, int radius) {
    raw_circle(active_surface(), x, y, radius, map_color(active_surface(), g_color));
}
void arc(int x, int y, int start_angle, int end_angle, int radius) {
    g_ac.x = x; g_ac.y = y;
    g_ac.xstart = x + (int)(radius * cos(start_angle * M_PI / 180.0));
    g_ac.ystart = y - (int)(radius * sin(start_angle * M_PI / 180.0));
    g_ac.xend = x + (int)(radius * cos(end_angle * M_PI / 180.0));
    g_ac.yend = y - (int)(radius * sin(end_angle * M_PI / 180.0));
    raw_ellipse_arc(active_surface(), x, y, radius, radius, start_angle, end_angle, map_color(active_surface(), g_color));
}
void getarccoords(arccoordstype* arccoords) { *arccoords = g_ac; }

void pieslice(int x, int y, int start_angle, int end_angle, int radius) {
    if (g_fill_settings.pattern == EMPTY_FILL) return;
    SDL_Surface* s = active_surface();
    if (!s) return;
    std::vector<SDL_Point> pts;
    pts.push_back(SDL_Point{x, y});
    double a1 = end_angle < start_angle ? end_angle + 360.0 : end_angle;
    for (double a = start_angle; a <= a1 + 0.001; a += 1.0) {
        double rad = a * M_PI / 180.0;
        pts.push_back(SDL_Point{x + (int)std::lround(radius * cos(rad)), y - (int)std::lround(radius * sin(rad))});
    }
    raw_fill_polygon(s, pts, map_color(s, g_fill_settings.color));
}
void sector(int x, int y, int start_angle, int end_angle, int rx, int ry) {
    if (g_fill_settings.pattern == EMPTY_FILL) return;
    SDL_Surface* s = active_surface();
    if (!s) return;
    std::vector<SDL_Point> pts;
    pts.push_back(SDL_Point{x, y});
    double a1 = end_angle < start_angle ? end_angle + 360.0 : end_angle;
    for (double a = start_angle; a <= a1 + 0.001; a += 1.0) {
        double rad = a * M_PI / 180.0;
        pts.push_back(SDL_Point{x + (int)std::lround(rx * cos(rad)), y - (int)std::lround(ry * sin(rad))});
    }
    raw_fill_polygon(s, pts, map_color(s, g_fill_settings.color));
}

void bar(int left, int top, int right, int bottom) {
    if (g_fill_settings.pattern == EMPTY_FILL) return;
    raw_rect_fill(active_surface(), left, top, right, bottom, map_color(active_surface(), g_fill_settings.color));
}
void bar3d(int left, int top, int right, int bottom, int depth, int topflag) {
    bar(left, top, right, bottom);
    SDL_Surface* s = active_surface();
    Uint32 col = map_color(s, g_color);
    raw_rect_outline(s, left, top, right, bottom, col);
    if (depth) {
        raw_line(s, right, top, right + depth, top - depth / 2, col);
        raw_line(s, right, bottom, right + depth, bottom - depth / 2, col);
        if (topflag) raw_line(s, left, top, left + depth, top - depth / 2, col);
    }
}

void lineto(int x, int y) {
    raw_line(active_surface(), g_cur_x, g_cur_y, x, y, map_color(active_surface(), g_color));
    moveto(x, y);
}
void linerel(int dx, int dy) { lineto(g_cur_x + dx, g_cur_y + dy); }
void line(int x0, int y0, int x1, int y1) {
    raw_line(active_surface(), x0, y0, x1, y1, map_color(active_surface(), g_color));
}
void rectangle(int left, int top, int right, int bottom) {
    raw_rect_outline(active_surface(), left, top, right, bottom, map_color(active_surface(), g_color));
}
void drawpoly(int n_points, int* points) {
    SDL_Surface* s = active_surface();
    Uint32 col = map_color(s, g_color);
    for (int i = 0; i + 1 < n_points; i++)
        raw_line(s, points[2*i], points[2*i+1], points[2*i+2], points[2*i+3], col);
}
void fillpoly(int n_points, int* points) {
    if (g_fill_settings.pattern == EMPTY_FILL) return;
    std::vector<SDL_Point> pts(n_points);
    for (int i = 0; i < n_points; i++) pts[i] = SDL_Point{points[2*i], points[2*i+1]};
    raw_fill_polygon(active_surface(), pts, map_color(active_surface(), g_fill_settings.color));
}

void floodfill(int x, int y, int border) {
    SDL_Surface* s = active_surface();
    if (!s) return;
    Uint32 borderPix = map_color(s, border);
    Uint32 fillPix = map_color(s, g_fill_settings.color);
    Uint32 startPix = raw_getpixel(s, x, y);
    if (startPix == borderPix || startPix == fillPix) return;
    std::vector<std::pair<int,int>> stack;
    stack.push_back({x, y});
    while (!stack.empty()) {
        int cx = stack.back().first, cy = stack.back().second;
        stack.pop_back();
        if (cx < 0 || cy < 0 || cx >= s->w || cy >= s->h) continue;
        Uint32 p = raw_getpixel(s, cx, cy);
        if (p == borderPix || p == fillPix) continue;
        raw_putpixel(s, cx, cy, fillPix);
        stack.push_back({cx+1,cy}); stack.push_back({cx-1,cy});
        stack.push_back({cx,cy+1}); stack.push_back({cx,cy-1});
    }
}

bool mouseup() {
    while (pump_events(false));
    if (g_mouse_up) { g_mouse_up = false; return true; }
    return false;
}
bool mousedown() {
    while (pump_events(false));
    if (g_mouse_down) { g_mouse_down = false; return true; }
    return false;
}
void clearmouse() {
    g_mouse_click_x = g_mouse_click_y = g_mouse_cur_x = g_mouse_cur_y = 0;
    g_mouse_down = g_mouse_up = false;
}
int mouseclickx() { return g_mouse_click_x; }
int mouseclicky() { return g_mouse_click_y; }
int mousecurrentx() { return g_mouse_cur_x; }
int mousecurrenty() { return g_mouse_cur_y; }
int whichmousebutton() { return g_which_mouse_button; }

int kbhit() {
    while (pump_events(false));
    return !g_kbd_queue.is_empty();
}
int getch() {
    while (g_kbd_queue.is_empty()) pump_events(true);
    return (unsigned char)g_kbd_queue.get();
}

void delay(unsigned msec) {
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < msec) {
        pump_events(false);
        SDL_Delay(1);
    }
}

void cleardevice() {
    raw_rect_fill(active_surface(), 0, 0, g_window_width - 1, g_window_height - 1, map_color(active_surface(), g_bkcolor));
    moveto(0, 0);
}
void clearviewport() {
    raw_rect_fill(active_surface(), g_view_settings.left, g_view_settings.top,
                  g_view_settings.right, g_view_settings.bottom, map_color(active_surface(), g_bkcolor));
    moveto(0, 0);
}

void detectgraph(int* graphdriver, int* graphmode) { *graphdriver = VGA; *graphmode = bgiemu_default_mode; }
int getgraphmode() { return bgiemu_default_mode; }
void setgraphmode(int) {}
void restorecrtmode() {}
void graphdefaults() {
    g_color = WHITE; g_bkcolor = BLACK;
    g_line_settings = {SOLID_LINE, 0xFFFF, NORM_WIDTH};
    g_fill_settings = {SOLID_FILL, WHITE};
    g_text_settings = {DEFAULT_FONT, HORIZ_DIR, 1, LEFT_TEXT, TOP_TEXT};
    moveto(0, 0);
}
unsigned int setgraphbufsize(unsigned int size) { return size; }
void getmoderange(int, int* lo, int* hi) { *lo = *hi = 0; }
int installuserdriver(char const*, int*) { return -1; }
int installuserfont(char const*) { return -1; }
int registerbgidriver(void*) { return -1; }
int registerbgifont(void*) { return -1; }

unsigned int imagesize(int x1, int y1, int x2, int y2) {
    return (unsigned int)(sizeof(short) * 2 + (x2 - x1 + 1) * (y2 - y1 + 1) * sizeof(Uint32));
}
void getimage(int x1, int y1, int x2, int y2, void* image) {
    SDL_Surface* s = active_surface();
    if (!s) return;
    short w = (short)(x2 - x1 + 1), h = (short)(y2 - y1 + 1);
    short* hdr = (short*)image;
    hdr[0] = w; hdr[1] = h;
    Uint32* bits = (Uint32*)((short*)image + 2);
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            bits[yy * w + xx] = raw_getpixel(s, x1 + xx, y1 + yy);
}
void putimage(int x, int y, void* image, int) {
    SDL_Surface* s = active_surface();
    if (!s) return;
    short* hdr = (short*)image;
    short w = hdr[0], h = hdr[1];
    Uint32* bits = (Uint32*)((short*)image + 2);
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            raw_putpixel(s, x + xx, y + yy, bits[yy * w + xx]);
}

unsigned int getpixel(int x, int y) {
    SDL_Surface* s = active_surface();
    if (!s) return (unsigned int)-1;
    Uint32 pix = raw_getpixel(s, x, y);
    Uint8 r, g, b, a;
    SDL_GetRGBA(pix, s->format, &r, &g, &b, &a);
    for (int c = 0; c <= MAXCOLORS; c++) {
        RGB rgb = g_palette_rgb[c];
        if (rgb.r == r && rgb.g == g && rgb.b == b) return c;
    }
    return (unsigned int)-1;
}
void putpixel(int x, int y, int c) {
    raw_putpixel(active_surface(), x, y, map_color(active_surface(), c));
}

void closegraph() {
    for (auto& kv : g_font_cache) if (kv.second) TTF_CloseFont(kv.second);
    g_font_cache.clear();
    for (auto& p : g_pages) { if (p) { SDL_FreeSurface(p); p = nullptr; } }
    if (g_texture) { SDL_DestroyTexture(g_texture); g_texture = nullptr; }
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = nullptr; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = nullptr; }
    TTF_Quit();
    SDL_Quit();
}

void initgraph(int* device, int* mode, char const* /*pathtodriver*/, int size_width, int size_height) {
    g_gdi_error_code = grOk;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { g_gdi_error_code = grNotDetected; return; }
    if (TTF_Init() != 0) { g_gdi_error_code = grFontNotFound; }

    for (int i = 0; i < 64; i++) g_palette_map[i] = i;
    g_current_palette.size = MAXCOLORS + 1;
    for (int i = 0; i <= MAXCOLORS; i++) g_current_palette.colors[i] = i;

    g_window_width = size_width > 0 ? size_width : 640;
    g_window_height = size_height > 0 ? size_height : 480;

    g_window = SDL_CreateWindow("BGI (SDL2)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 g_window_width, g_window_height, SDL_WINDOW_SHOWN);
    if (!g_window) { g_gdi_error_code = grNoInitGraph; return; }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    SDL_StartTextInput();

    graphdefaults();
    g_active_page = g_visual_page = 0;
    g_view_settings = {0, 0, g_window_width - 1, g_window_height - 1, CLIP_OFF};

    ensure_page(0);
    present_page();

    if (device) *device = VGA;
    if (mode) *mode = VGAMAX;
}

#endif // _WIN32
