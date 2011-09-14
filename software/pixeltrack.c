/*
 * pixeltrack.c
 *
 * Track the mouse cursor and print its position alongside the color
 * of the pixel it resides over
 *
 * By Stefan Tomanek <stefan@pico.ruhr.de>
 */

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdint.h>
#include "hid.h"

struct rgb_color {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

void get_pixel_color(Display *d, int x, int y, struct rgb_color *c, int radius) {
	XImage *img;
	img = XGetImage(d, RootWindow (d, DefaultScreen (d)), x-radius, y-radius, 2*radius, 2*radius, AllPlanes, XYPixmap);
	XColor xc;
	/* calculate average color */
	uint8_t ix, iy;
	double r = 0;
	double g = 0;
	double b = 0;
	int pixels = (img->width*img->height);
	for (ix=0; ix<img->width; ix++) {
		for (iy=0; iy<img->height; iy++) {
			xc.pixel = XGetPixel(img, ix, iy);
			XQueryColor(d, DefaultColormap(d, DefaultScreen(d)), &xc);
			r += 1.0*xc.red/pixels;
			g += 1.0*xc.green/pixels;
			b += 1.0*xc.blue/pixels;
		}
	}
	c->red = (xc.red/256);
	c->green = (xc.green/256);
	c->blue = (xc.blue/256);
	XFree(img);
}

void get_cursor_position(Display *d, int *x, int *y) {
	int screen = DefaultScreen(d);
	Window rootwindow = RootWindow(d, screen);

	Window root_return;
	Window child_return;
	int win_x_return;
	int win_y_return;
	unsigned int mask_return;

	XQueryPointer(d, rootwindow, &root_return, &child_return,
			x, y,
			&win_x_return, &win_y_return,
			&mask_return);
}

int main(int argc, char *argv[]) {
	Display *d = XOpenDisplay(NULL);
	if (!d) {
		printf("Unable to open display\n");
		return;
	}
	int usb_present = rawhid_open(1, 0x16C0, 0x0480, -1, -1);
	if (!usb_present) {
		printf("Unable to open usb device, proceeding anyway...\n");
	}
	
	int x = -1;
	int y = -1;
	int old_x = -1;
	int old_y = -1;
	struct rgb_color color;
	uint8_t buf[64];
	while(1) {
		get_cursor_position(d, &x, &y);
		if (x != old_x || y != old_y) {
			get_pixel_color(d, x, y, &color, 1);
			printf("%d/%d\t(%d/%d/%d)\n", x, y, color.red, color.green, color.blue);
			if (usb_present) {
				uint8_t i;
				for (i=0; i<64; i++) {
					buf[i] = 0;
				}
				buf[0] = color.red;
				buf[1] = color.green;
				buf[2] = color.blue;
				rawhid_send(0, buf, 64, 100);
			}
			old_x = x;
			old_y = y;
		}
		usleep(100);
	}
	XCloseDisplay(d);
}
