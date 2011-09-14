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

void get_pixel_color(Display *d, int x, int y, XColor *c) {
	XImage *img;
	img = XGetImage(d, RootWindow (d, DefaultScreen (d)), x, y, 1, 1, AllPlanes, XYPixmap);
	c->pixel = XGetPixel(img, 0, 0);
	XFree(img);
	XQueryColor(d, DefaultColormap(d, DefaultScreen(d)), c);
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
	
	int x = -1;
	int y = -1;
	int old_x = -1;
	int old_y = -1;
	XColor color;
	while(1) {
		get_cursor_position(d, &x, &y);
		if (x != old_x || y != old_y) {
			get_pixel_color(d, x, y, &color);
			printf("%d/%d\t(%d/%d/%d)\n", x, y, color.red/256, color.green/256, color.blue/256);
			old_x = x;
			old_y = y;
		}
		usleep(100);
	}
	XCloseDisplay(d);
}
