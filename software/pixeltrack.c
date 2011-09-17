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
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include "hid.h"

struct rgb_color {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

XShmSegmentInfo shminfo;
XImage *img;

void init_shm(Display *d, int radius) {
	if (img != NULL) {
		XShmDetach(d, &shminfo);
		XDestroyImage(img);
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
	}

	long width = radius*2;
	long height = radius*2;

	img = XShmCreateImage( d, DefaultVisual(d, DefaultScreen(d)), DefaultDepth(d, DefaultScreen(d)),
			ZPixmap, NULL, &shminfo, width, height );
	int imgsize = img->bytes_per_line * img->height;
	shminfo.shmid = shmget( IPC_PRIVATE, imgsize, IPC_CREAT|0777 );
	char* mem = (char*)shmat(shminfo.shmid, 0, 0);
	shminfo.shmaddr = mem;
	img->data = mem;
	shminfo.readOnly = False;
	XShmAttach(d, &shminfo);
}

void get_pixel_color(Display *d, int x, int y, struct rgb_color *c, int radius) {
	int success = XShmGetImage(d, RootWindow (d, DefaultScreen (d)), img, x-(radius/2), y-(radius/2), AllPlanes);
	XColor xc;
	/* calculate average color */
	long ix, iy;
	unsigned long r = 0;
	unsigned long g = 0;
	unsigned long b = 0;
	int pixels = radius*radius;
	for (ix=0; ix<(radius*2)-1; ix++) {
		for (iy=0; iy<(radius*2)-1; iy++) {
			xc.pixel = XGetPixel(img, ix, iy);
			XQueryColor(d, DefaultColormap(d, DefaultScreen(d)), &xc);
			r += xc.red/256;
			g += xc.green/256;
			b += xc.blue/256;
		}
	}
	c->red = (r/pixels);
	c->green = (g/pixels);
	c->blue = (b/pixels);
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

	int radius = 10;

	init_shm(d, radius);

	int x = -1;
	int y = -1;
	int old_x = -1;
	int old_y = -1;
	struct rgb_color color;
	uint8_t buf[64];
	while(1) {
		get_cursor_position(d, &x, &y);
		if (x != old_x || y != old_y) {
			get_pixel_color(d, x, y, &color, radius);
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
