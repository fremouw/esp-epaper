#include "epd_font.h"
#include "epd.h"
#include <esp_log.h>
#include <string.h>

#define EPD_SWAP(a, b) { int16_t t = a; a = b; b = t; }

extern uint8_t tft_DejavuSans12[];
extern uint8_t tft_Dejavu24[];
extern uint8_t tft_DejavuSansBold42[];

static const char TAG[] = "epd";

static const color_t epd_fg_color = 15;
static const color_t epd_bg_color = 0;

// 
static void epd_get_max_width_height(epd_device_t* device);
// static int epd_get_string_width(epd_device_t* device, char* str);

static uint8_t epd_get_char_ptr(epd_device_t* device, uint8_t c);
static int epd_print_proportional_char(epd_device_t* device, int x, int y);
static void epd_fill_rect(epd_device_t* device, int16_t x, int16_t y, int16_t w, int16_t h, color_t color);
static void epd_push_color_rep(epd_device_t* device, int x1, int y1, int x2, int y2, color_t color);
static void epd_print_char(epd_device_t* device, uint8_t c, int x, int y);
static void epd_draw_rect(epd_device_t* device, uint16_t x1,uint16_t y1,uint16_t w,uint16_t h, color_t color);
static void epd_draw_fast_hline(epd_device_t* device, int16_t x, int16_t y, int16_t w, color_t color);
static void epd_draw_fast_vline(epd_device_t* device, int16_t x, int16_t y, int16_t h, color_t color);
static void epd_bar_vert(epd_device_t* device, int16_t x, int16_t y, int16_t w, int16_t l, color_t color, color_t outline);
static void epd_bar_hor(epd_device_t* device, int16_t x, int16_t y, int16_t w, int16_t l, color_t color, color_t outline);
static void epd_fill_triangle(epd_device_t* device, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color);
static void epd_draw_triangle(epd_device_t* device, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color);
static void epd_draw_line(epd_device_t* device, int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color);

void epd_set_font(epd_device_t* device, epd_font_name_t font) {
    switch(font) {
        case epd_font_deja_vu_sans_24_e:
            device->font.font = tft_Dejavu24;
            break;
        case epd_font_deja_vu_sans_bold_42_e:
            device->font.font = tft_DejavuSansBold42;
        break;
        default:
            device->font.font = tft_DejavuSans12;
    }

    // device->font.bitmap = 1;
    device->font.x_size = device->font.font[0];
    device->font.y_size = device->font.font[1];

    if (device->font.x_size > 0) {
        device->font.offset = device->font.font[2];
        device->font.numchars = device->font.font[3];
        device->font.size = device->font.x_size * device->font.y_size * device->font.numchars;
    } else {
        device->font.offset = 4;
        epd_get_max_width_height(device);
    }
}

int epd_get_font_height(epd_device_t* device) {
    if(device == NULL || device->font.font == NULL) {
        ESP_LOGE(TAG, "no font set.");
        return ESP_FAIL;
    }

    return device->font.y_size;
}

void epd_print(epd_device_t* device, const char *st, int x, int y) {
	int stl, i, tmpw, tmph, fh;
	uint8_t ch;

	// if (device->font.bitmap == 0) {
    //     ESP_LOGE(TAG, "wrong font selected");
    //     return; // wrong font selected
    // }

	// ** Rotated strings cannot be aligned
	// if ((font_rotate != 0) && ((x <= CENTER) || (y <= CENTER))) return;
	// if ((x < LASTX) || (font_rotate == 0)) EPD_OFFSET = 0;

	// if ((x >= LASTX) && (x < LASTY)) x = EPD_X + (x-LASTX);
	// else if (x > CENTER) x += device->disp_win.x1;

	// if (y >= LASTY) y = EPD_Y + (y-LASTY);
	// else if (y > CENTER) y += device->disp_win.y1;

	// ** Get number of characters in string to print
	stl = strlen(st);
    
	// ** Calculate CENTER, RIGHT or BOTTOM position
	tmpw = epd_get_string_width(device, st);	// string width in pixels
	fh = device->font.y_size;			// font height

	if (x == EPD_RIGHT) {
        x = device->disp_win.x2 - tmpw + device->disp_win.x1;
    } else if (x == EPD_CENTER) {
        x = (((device->disp_win.x2 - device->disp_win.x1 + 1) - tmpw) / 2) + device->disp_win.x1;
    }

	if (y == EPD_BOTTOM) {
        y = device->disp_win.y2 - fh + device->disp_win.y1;
    } else if (y==EPD_CENTER) {
        y = (((device->disp_win.y2 - device->disp_win.y1 + 1) - (fh/2)) / 2) + device->disp_win.y1;
    }

	if (x < device->disp_win.x1) {
        x = device->disp_win.x1;
    }
	if (y < device->disp_win.y1) {
        y = device->disp_win.y1;
    }
	if ((x > device->disp_win.x2) || (y > device->disp_win.y2)) {
        return;
    }

    int _x = x;
    int _y = y;

	// ** Adjust y position
	tmph = device->font.y_size; // font height
	// for non-proportional fonts, char width is the same for all chars
	tmpw = device->font.x_size;
	// if (device->font.x_size != 0) {
	// 	if (device->font.bitmap == 2) {	// 7-segment font
	// 		tmpw = epd_7seg_width(&device->font);	// character width
	// 		tmph = epd_7seg_height(&device->font);	// character height
	// 	}
	// }
	// else EPD_OFFSET = 0;	// fixed font; offset not needed

	if (( _y + tmph - 1) > device->disp_win.y2) {
        return;
    }

	// int offset = EPD_OFFSET;

	for (i=0; i<stl; i++) {
		ch = st[i]; // get string character

		if (ch == 0x0d) { // === '\r', erase to eol ====
			if ((!device->font_transparent)) {
                epd_fill_rect(device, _x, _y,  device->disp_win.x2+1- _x, tmph, epd_bg_color);
            }
		} else if (ch == 0x0a) { // ==== '\n', new line ====
            _y += tmph + device->font_line_space;
            if ( _y > (device->disp_win.y2-tmph)) {
                break;
            }
            _x = device->disp_win.x1;
		} else { // ==== other characters ====
			if (device->font.x_size == 0) {
				// for proportional font get character data to 'fontChar'
				if (epd_get_char_ptr(device, ch)) {
                    tmpw = device->font_char.x_delta;
                } else {
                    continue;
                }
			}

			// check if character can be displayed in the current line
			if (( _y+tmpw) > (device->disp_win.x2)) {
				if (device->text_wrap == 0) {
                    break;
                }
				 _y += tmph + device->font_line_space;
				if ( _y > (device->disp_win.y2-tmph)) {
                    break;
                }
				 _x = device->disp_win.x1;
			}

			// Let's print the character
			if (device->font.x_size == 0) {
				// == proportional font
				_x += epd_print_proportional_char(device, _x, _y) + 1;
			} else {
                // == fixed font
                if ((ch < device->font.offset) || ((ch-device->font.offset) > device->font.numchars)) {
                    ch = device->font.offset;
                }

                epd_print_char(device, ch, _x, _y);
                _x += tmpw;
			}
		}
	}
}

static void epd_get_max_width_height(epd_device_t* device) {
	uint16_t tempPtr = 4; // point at first char data
	uint8_t cc, cw, ch, cd, cy;

	device->font.numchars = 0;
	device->font.max_x_size = 0;

    cc = device->font.font[tempPtr++];
    while (cc != 0xFF)  {
    	device->font.numchars++;
        cy = device->font.font[tempPtr++];
        cw = device->font.font[tempPtr++];
        ch = device->font.font[tempPtr++];
        tempPtr++;
        cd = device->font.font[tempPtr++];
        cy += ch;
		if (cw > device->font.max_x_size) {
            device->font.max_x_size = cw;
        }
		if (cd > device->font.max_x_size) {
            device->font.max_x_size = cd;
        }
		if (ch > device->font.y_size) {
            device->font.y_size = ch;
        }
		if (cy > device->font.y_size) {
            device->font.y_size = cy;
        }
		if (cw != 0) {
			// packed bits
			tempPtr += (((cw * ch)-1) / 8) + 1;
		}
	    cc = device->font.font[tempPtr++];
	}
    device->font.size = tempPtr;
}

int epd_get_string_width(epd_device_t* device, char* str) {
    epd_font_t* font = &device->font;
    int strWidth = 0;

	// if (font->bitmap == 2) {
    //     strWidth = ((epd_7seg_width(font)+2) * strlen(str)) - 2;	// 7-segment font
    if (font->x_size != 0) {
        strWidth = strlen(str) * font->x_size;			// fixed width font
    } else {
		// calculate the width of the string of proportional characters
		char* tempStrptr = str;
		while (*tempStrptr != 0) {
			if (epd_get_char_ptr(device, *tempStrptr++)) {
				strWidth += (((device->font_char.width > device->font_char.x_delta) ? device->font_char.width : device->font_char.x_delta) + 1);
			}
		}
		strWidth--;
	}
	return strWidth;
}

// static int epd_7seg_width(epd_font_t* font) {
// 	return (2 * (2 * font->y_size + 1)) + font->x_size;
// }

// static int epd_7seg_height(epd_font_t* font) {
// 	return (3 * (2 * font->y_size + 1)) + (2 * font->x_size);
// }

static uint8_t epd_get_char_ptr(epd_device_t* device, uint8_t c) {
    epd_font_t* font = &device->font;
    uint16_t temp_ptr = 4; // point at first char data

    do {
        device->font_char.char_code = device->font.font[temp_ptr++];
        if (device->font_char.char_code == 0xFF) {
            return 0;
        }

        device->font_char.adj_y_offset = font->font[temp_ptr++];
        device->font_char.width = font->font[temp_ptr++];
        device->font_char.height = font->font[temp_ptr++];
        device->font_char.x_offset = font->font[temp_ptr++];
        device->font_char.x_offset = device->font_char.x_offset < 0x80 ? device->font_char.x_offset : -(0xff - device->font_char.x_offset);
        device->font_char.x_delta = font->font[temp_ptr++];

        if (c != device->font_char.char_code && device->font_char.char_code != 0xff) {
            if (device->font_char.width != 0) {
                // packed bits
                temp_ptr += (((device->font_char.width * device->font_char.height)-1) / 8) + 1;
            }
        }
    } while ((c != device->font_char.char_code) && (device->font_char.char_code != 0xff));

  device->font_char.data_ptr = temp_ptr;
  if (c == device->font_char.char_code) {
    // if (font_forceFixed > 0) {
    //   // fix width & offset for forced fixed width
    //   device->font_char.xDelta = font->max_x_size;
    //   device->font_char.xOffset = (device->font_char.xDelta - device->font_char.width) / 2;
    // }
  }
  else return 0;

  return 1;
}

static int epd_print_proportional_char(epd_device_t* device, int x, int y) {
	uint8_t ch = 0;
	int i, j, char_width;

	char_width = ((device->font_char.width > device->font_char.x_delta) ? device->font_char.width : device->font_char.x_delta);
	int cx, cy;

	if (!device->font_transparent) {
        epd_fill_rect(device, x, y, char_width+1, device->font.y_size, epd_bg_color);
    }

	// draw Glyph
	uint8_t mask = 0x80;
	for (j=0; j < device->font_char.height; j++) {
		for (i=0; i < device->font_char.width; i++) {
			if (((i + (j*device->font_char.width)) % 8) == 0) {
				mask = 0x80;
				ch = device->font.font[device->font_char.data_ptr++];
			}

			if ((ch & mask) !=0) {
				cx = (uint16_t)(x+device->font_char.x_offset+i);
				cy = (uint16_t)(y+j+device->font_char.adj_y_offset);
				epd_draw_pixel(device, cx, cy, epd_fg_color);
			}
			mask >>= 1;
		}
	}

	return char_width;
}

static void epd_fill_rect(epd_device_t* device, int16_t x, int16_t y, int16_t w, int16_t h, color_t color) {
	// clipping
	if ((x >= device->disp_win.x2) || (y > device->disp_win.y2)) {
        return;
    }

	if (x < device->disp_win.x1) {
		w -= (device->disp_win.x1 - x);
		x = device->disp_win.x1;
	}
	if (y < device->disp_win.y1) {
		h -= (device->disp_win.y1 - y);
		y = device->disp_win.y1;
	}
	if (w < 0) {
        w = 0;
    }
	if (h < 0) {
        h = 0;
    }

	if ((x + w) > (device->disp_win.x2+1)) {
        w = device->disp_win.x2 - x + 1;
    }
	if ((y + h) > (device->disp_win.y2+1)) {
        h = device->disp_win.y2 - y + 1;
    }
	if (w == 0) {
        w = 1;
    }
	if (h == 0) {
        h = 1;
    }
	epd_push_color_rep(device, x, y, x+w-1, y+h-1, color);
}

static void epd_push_color_rep(epd_device_t* device, int x1, int y1, int x2, int y2, color_t color) {
	// if (_gs == 0) color &= 0x01;
	// else color &= 0x0F;
    color &= 0x01;
	for (int y=y1; y<=y2; y++) {
		for (int x = x1; x<=x2; x++){
			epd_draw_pixel(device, x, y, color);
		}
	}
}

static void epd_print_char(epd_device_t* device, uint8_t c, int x, int y) {
	uint8_t i, j, ch, fz, mask;
	uint16_t k, temp, cx, cy;

	// fz = bytes per char row
	fz = device->font.x_size/8;
	if (device->font.x_size % 8) {
        fz++;
    }

	// get character position in buffer
	temp = ((c-device->font.offset)*((fz)*device->font.y_size))+4;

	if (!device->font_transparent) {
        epd_fill_rect(device, x, y, device->font.x_size, device->font.y_size, epd_bg_color);
    }

	for (j=0; j<device->font.y_size; j++) {
		for (k=0; k < fz; k++) {
			ch = device->font.font[temp+k];
			mask=0x80;
			for (i=0; i<8; i++) {
				if ((ch & mask) !=0) {
					cx = (uint16_t)(x+i+(k*8));
					cy = (uint16_t)(y+j);
					epd_draw_pixel(device, cx, cy, epd_fg_color);
				}
				mask >>= 1;
			}
		}
		temp += (fz);
	}
}

// static void epd_draw7seg(epd_device_t* device, int16_t x, int16_t y, int8_t num, int16_t w, int16_t l, color_t color) {
//     /* TODO: clipping */
//     if (num < 0x2d || num > 0x3a) {
//         return;
//     }

//     int16_t c = font_bcd[num-0x2d];
//     int16_t d = 2*w+l+1;

//     epd_bar_vert(device, x+d, y+d, w, l, epd_bg_color, epd_bg_color);
//     epd_bar_vert(device, x,   y+d, w, l, epd_bg_color, epd_bg_color);
//     epd_bar_vert(device, x+d, y, w, l, epd_bg_color, epd_bg_color);
//     epd_bar_vert(device, x,   y, w, l, epd_bg_color, epd_bg_color);
//     epd_bar_hor(device, x, y+2*d, w, l, epd_bg_color, epd_bg_color);
//     epd_bar_hor(device, x, y+d, w, l, epd_bg_color, epd_bg_color);
//     epd_bar_hor(device, x, y, w, l, epd_bg_color, epd_bg_color);

//     epd_fill_rect(device, x+(d/2), y+2*d, 2*w+1, 2*w+1, epd_bg_color);
//     epd_draw_rect(device, x+(d/2), y+2*d, 2*w+1, 2*w+1, epd_bg_color);
//     epd_fill_rect(device, x+(d/2), y+d+2*w+1, 2*w+1, l/2, epd_bg_color);
//     epd_draw_rect(device, x+(d/2), y+d+2*w+1, 2*w+1, l/2, epd_bg_color);
//     epd_fill_rect(device, x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, epd_bg_color);
//     epd_draw_rect(device, x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, epd_bg_color);
//     epd_fill_rect(device, x+2*w+1, y+d, l, 2*w+1, epd_bg_color);
//     epd_draw_rect(device, x+2*w+1, y+d, l, 2*w+1, epd_bg_color);

//     // === Draw used segments ===
//     if (c & 0x001) {
//         epd_bar_vert(device, x+d, y+d, w, l, color, device->font.color);	// down right
//     }
//     if (c & 0x002) {
//         epd_bar_vert(device, x,   y+d, w, l, color, device->font.color);	// down left
//     }
//     if (c & 0x004) {
//         epd_bar_vert(device, x+d, y, w, l, color, device->font.color);		// up right
//     }
//     if (c & 0x008) {
//         epd_bar_vert(device, x,   y, w, l, color, device->font.color);		// up left
//     }
//     if (c & 0x010) {
//         epd_bar_hor(device, x, y+2*d, w, l, color, device->font.color);	// down
//     }
//     if (c & 0x020) {
//         epd_bar_hor(device, x, y+d, w, l, color, device->font.color);		// middle
//     }
//     if (c & 0x040) {
//         epd_bar_hor(device, x, y, w, l, color, device->font.color);		// up
//     }

//     if (c & 0x080) {
//         // low point
//         epd_fill_rect(device, x+(d/2), y+2*d, 2*w+1, 2*w+1, color);
//         if (device->font.offset) {
//             epd_draw_rect(device, x+(d/2), y+2*d, 2*w+1, 2*w+1, device->font.color);
//         }
//     }
//     if (c & 0x100) {
//         // down middle point
//         epd_fill_rect(device, x+(d/2), y+d+2*w+1, 2*w+1, l/2, color);
//         if (device->font.offset) {
//             epd_draw_rect(device, x+(d/2), y+d+2*w+1, 2*w+1, l/2, device->font.color);
//         }
//     }
//     if (c & 0x800) {
//         // up middle point
//         epd_fill_rect(device, x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, color);
//         if (device->font.offset) {
//             epd_draw_rect(device, x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, device->font.color);
//         }
//     }
//     if (c & 0x200) {
//         // middle, minus
//         epd_fill_rect(device, x+2*w+1, y+d, l, 2*w+1, color);
//         if (device->font.offset) {
//             epd_draw_rect(device, x+2*w+1, y+d, l, 2*w+1, device->font.color);
//         }
//     }
// }


static void epd_draw_rect(epd_device_t* device, uint16_t x1,uint16_t y1,uint16_t w,uint16_t h, color_t color) {
    epd_draw_fast_hline(device, x1,y1,w, color);
    epd_draw_fast_vline(device, x1+w-1,y1,h, color);
    epd_draw_fast_hline(device, x1,y1+h-1,w, color);
    epd_draw_fast_vline(device, x1,y1,h, color);
}

static void epd_draw_fast_hline(epd_device_t* device, int16_t x, int16_t y, int16_t w, color_t color) {
	// clipping
	if ((y < device->disp_win.y1) || (x > device->disp_win.x2) || (y > device->disp_win.y2)) {
        return;
    }
	if (x < device->disp_win.x1) {
		w -= (device->disp_win.x1 - x);
		x = device->disp_win.x1;
	}
	if (w < 0) {
        w = 0;
    }
	if ((x + w) > (device->disp_win.x2+1)) {
        w = device->disp_win.x2 - x + 1;
    }
	if (w == 0) {
        w = 1;
    }

	epd_push_color_rep(device, x, y, x+w-1, y, color);
}

static void epd_draw_fast_vline(epd_device_t* device, int16_t x, int16_t y, int16_t h, color_t color) {
	// clipping
	if ((x < device->disp_win.x1) || (x > device->disp_win.x2) || (y > device->disp_win.y2)) {
        return;
    }
	if (y < device->disp_win.y1) {
		h -= (device->disp_win.y1 - y);
		y = device->disp_win.y1;
	}
	if (h < 0) {
        h = 0;
    }
	if ((y + h) > (device->disp_win.y2+1)) {
        h = device->disp_win.y2 - y + 1;
    }
	if (h == 0) {
        h = 1;
    }
	epd_push_color_rep(device, x, y, x, y+h-1, color);
}

static void epd_bar_vert(epd_device_t* device, int16_t x, int16_t y, int16_t w, int16_t l, color_t color, color_t outline) {
    epd_fill_triangle(device, x+1, y+2*w, x+w, y+w+1, x+2*w-1, y+2*w, color);
    epd_fill_triangle(device, x+1, y+2*w+l+1, x+w, y+3*w+l, x+2*w-1, y+2*w+l+1, color);
    epd_fill_rect(device, x, y+2*w+1, 2*w+1, l, color);
    if (device->font.offset) {
        epd_draw_triangle(device, x+1, y+2*w, x+w, y+w+1, x+2*w-1, y+2*w, outline);
        epd_draw_triangle(device, x+1, y+2*w+l+1, x+w, y+3*w+l, x+2*w-1, y+2*w+l+1, outline);
        epd_draw_rect(device, x, y+2*w+1, 2*w+1, l, outline);
    }
}

//----------------------------------------------------------------------------------------------
static void epd_bar_hor(epd_device_t* device, int16_t x, int16_t y, int16_t w, int16_t l, color_t color, color_t outline) {
    epd_fill_triangle(device, x+2*w, y+2*w-1, x+w+1, y+w, x+2*w, y+1, color);
    epd_fill_triangle(device, x+2*w+l+1, y+2*w-1, x+3*w+l, y+w, x+2*w+l+1, y+1, color);
    epd_fill_rect(device, x+2*w+1, y, l, 2*w+1, color);
    if (device->font.offset) {
        epd_draw_triangle(device, x+2*w, y+2*w-1, x+w+1, y+w, x+2*w, y+1, outline);
        epd_draw_triangle(device, x+2*w+l+1, y+2*w-1, x+3*w+l, y+w, x+2*w+l+1, y+1, outline);
        epd_draw_rect(device, x+2*w+1, y, l, 2*w+1, outline);
    }
}

static void epd_fill_triangle(epd_device_t* device, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color) {
    int16_t a, b, y, last;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1) {
        EPD_SWAP(y0, y1); EPD_SWAP(x0, x1);
    }
    if (y1 > y2) {
        EPD_SWAP(y2, y1); EPD_SWAP(x2, x1);
    }
    if (y0 > y1) {
        EPD_SWAP(y0, y1); EPD_SWAP(x0, x1);
    }

    if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if(x1 < a) { 
            a = x1;
        } else if(x1 > b) {
            b = x1;
        }
        if(x2 < a) {
            a = x2;
        } else if(x2 > b) {
            b = x2;
        }
        epd_draw_fast_hline(device, a, y0, b-a+1, color);
        return;
    }

    int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
    int32_t
    sa   = 0,
    sb   = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    // is included here (and second loop will be skipped, avoiding a /0
    // error there), otherwise scanline y1 is skipped here and handled
    // in the second loop...which also avoids a /0 error here if y0=y1
    // (flat-topped triangle).
    if(y1 == y2) {
        last = y1;   // Include y1 scanline
    } else {
        last = y1-1; // Skip it
    }

    for(y=y0; y<=last; y++) {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if(a > b) {
            EPD_SWAP(a,b);
        }
        epd_draw_fast_hline(device, a, y, b-a+1, color);
    }

    // For lower part of triangle, find scanline crossings for segments
    // 0-2 and 1-2.  This loop is skipped if y1=y2.
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for(; y<=y2; y++) {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if(a > b) {
            EPD_SWAP(a,b);
        }
        epd_draw_fast_hline(device, a, y, b-a+1, color);
    }
}

static void epd_draw_triangle(epd_device_t* device, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color) {
	epd_draw_line(device, x0, y0, x1, y1, color);
	epd_draw_line(device, x1, y1, x2, y2, color);
	epd_draw_line(device, x2, y2, x0, y0, color);
}

static void epd_draw_line(epd_device_t* device, int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color) {
    if (x0 == x1) {
        if (y0 <= y1) {
            epd_draw_fast_vline(device, x0, y0, y1-y0, color);
        } else {
            epd_draw_fast_vline(device, x0, y1, y0-y1, color);
        }
        return;
    }
    if (y0 == y1) {
        if (x0 <= x1) {
            epd_draw_fast_hline(device, x0, y0, x1-x0, color);
        } else {
            epd_draw_fast_hline(device, x1, y0, x0-x1, color);
        }
        return;
    }

    int steep = 0;
    if (abs(y1 - y0) > abs(x1 - x0)) {
        steep = 1;
    }
    if (steep) {
        EPD_SWAP(x0, y0);
        EPD_SWAP(x1, y1);
    }
    if (x0 > x1) {
        EPD_SWAP(x0, x1);
        EPD_SWAP(y0, y1);
    }

    int16_t dx = x1 - x0, dy = abs(y1 - y0);;
    int16_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

    if (y0 < y1) {
        ystep = 1;
    }

    // Split into steep and not steep for FastH/V separation
    if (steep) {
        for (; x0 <= x1; x0++) {
            dlen++;
            err -= dy;
            if (err < 0) {
                err += dx;
                if (dlen == 1) {
                    epd_draw_pixel(device, y0, xs, color);
                } else {
                    epd_draw_fast_vline(device, y0, xs, dlen, color);
                }
                dlen = 0; y0 += ystep; xs = x0 + 1;
            }
        }
        if (dlen) {
            epd_draw_fast_vline(device, y0, xs, dlen, color);
        }
    } else {
        for (; x0 <= x1; x0++) {
            dlen++;
            err -= dy;
            if (err < 0) {
                err += dx;
                if (dlen == 1) {
                    epd_draw_pixel(device, xs, y0, color);
                } else {
                    epd_draw_fast_hline(device, xs, y0, dlen, color);
                }
                dlen = 0; y0 += ystep; xs = x0 + 1;
            }
        }
        if (dlen) {
            epd_draw_fast_hline(device, xs, y0, dlen, color);
        }
    }
}