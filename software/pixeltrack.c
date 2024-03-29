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
#include <X11/extensions/XInput2.h>
#include <sys/ipc.h>

#ifdef USB_PIXEL
#include <usb.h>
#include "../firmware/requests.h"
#include "../firmware/usbconfig.h"
#endif

/* capture that many pixels _around_ the cursor position;
 * the pixel below the cursor will be captured anyway, so
 * the captured area will be (2*RADIUS+1)^2; a setting of 0
 * will just capture the single pixel and no surroundings
 */
#define RADIUS 5

/* ordered that way due to strange byte order in XImage */
struct rgb_color {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t alpha;
};

XShmSegmentInfo shminfo;
XImage *img = NULL;
struct {
	int x;
	int y;
} img_offset;

void init_shm(Display *d, int radius) {
	if (img != NULL) {
		XShmDetach(d, &shminfo);
		XDestroyImage(img);
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
	}

	long width = (2*radius+1);
	long height = (2*radius+1);

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

void init_xinput(Display *d) {
	XIEventMask eventmask;
	unsigned char mask[1] = {0};
	eventmask.deviceid = XIAllDevices;
	eventmask.mask_len = sizeof(mask);
	eventmask.mask = mask;
	XISetMask(mask, XI_Motion);
	XISelectEvents(d, RootWindow(d, DefaultScreen (d)), &eventmask, 1);
}

#define VAL_MAX(x, y) ((x)>(y) ? (x) : (y))
#define VAL_MIN(x, y) ((x)<(y) ? (x) : (y))
#define VAL_BETWEEN(l, u, v) VAL_MIN( (VAL_MAX((l), (v))), (u))
int refresh_image(Display *d, int x, int y, int radius) {
	/* if we are near the border, we capture more than just the area around the cursor */
	int w = DisplayWidth(d, DefaultScreen(d));
	int h = DisplayHeight(d, DefaultScreen(d));
	img_offset.x = VAL_BETWEEN(radius, w-1-2*radius, x);
	img_offset.y = VAL_BETWEEN(radius, h-1-2*radius, y);

	int ret =  XShmGetImage(d, RootWindow(d, DefaultScreen (d)), img, img_offset.x, img_offset.y, AllPlanes);
	return ret;
}

void get_pixel_color(Display *d, int x, int y, struct rgb_color *c, int radius) {
#if USE_XQUERYCOLOR
	/* Cache color lookups */
#define CACHE_SIZE 16384
	static unsigned long pixels[CACHE_SIZE] = {0};
	static XColor colors[CACHE_SIZE] = {0};
	static uint8_t cached[CACHE_SIZE] = {0};

	XColor xc;
#else
	/* we blindly assume that we have an array of rgb structs */
	struct rgb_color *pic = (struct rgb_color *)img->data;
#endif
	/* calculate average color */
	long ix, iy;
	unsigned long r = 0;
	unsigned long g = 0;
	unsigned long b = 0;
	int n_pixels = 0;
	for (ix=0; ix < img->width; ix++) {
		if (ix+img_offset.x < x-radius || ix+img_offset.x > x+radius) continue;
		for (iy=0; iy < img->height; iy++) {
			if (iy+img_offset.y < y-radius || iy+img_offset.y > y+radius) continue;
#if USE_XQUERYCOLOR
			unsigned long p = XGetPixel(img, ix, iy);
			if (cached[p%CACHE_SIZE] && pixels[p%CACHE_SIZE] == p) {
				xc = colors[p%CACHE_SIZE];
			} else {
				xc.pixel = p;
				XQueryColor(d, DefaultColormap(d, DefaultScreen(d)), &xc);
				pixels[p%CACHE_SIZE] = p;
				colors[p%CACHE_SIZE] = xc;
				cached[p%CACHE_SIZE] = 1;
			}
			r += xc.red>>8;
			g += xc.green>>8;
			b += xc.blue>>8;
#else
			struct rgb_color *pixel = &pic[ix+iy*img->width];
			r += pixel->red;
			g += pixel->green;
			b += pixel->blue;
#endif
			n_pixels++;
		}
	}
	c->red = (r/n_pixels);
	c->green = (g/n_pixels);
	c->blue = (b/n_pixels);
}

void wait_for_movement(Display *d, int *x, int *y) {
	XEvent ev;
	XGenericEventCookie *cookie = &ev.xcookie;
	XNextEvent(d, &ev);
	if (XGetEventData(d, cookie)) {
		XIDeviceEvent *xd;
		switch(cookie->evtype) {
			case XI_Motion:
				xd = cookie->data;
				*x = xd->root_x;
				*y = xd->root_y;
				break;
		}
		XFreeEventData(d, cookie);
	}
}

uint8_t open_usb(usb_dev_handle **handle) {
	uint16_t vid = 0x16c0;
	uint16_t pid = 0x05df;
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle *target = NULL;

	usb_init();
	usb_find_busses();
	usb_find_devices();
	for (bus=usb_get_busses(); bus; bus=bus->next) {
		for (dev=bus->devices; dev; dev=dev->next) {
			if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid) {
				target = usb_open(dev);
			}
		}
	}
	//return (usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) != 0);
	//return (usbOpenDevice(&handle, 0x16c0, NULL, 0x05df, NULL) != 0);
	if (target != NULL) {
		*handle = target;
		return 1;
	} else {
		return 0;
	}
}

int main(int argc, char *argv[]) {
	Display *d = XOpenDisplay(NULL);
	if (!d) {
		printf("Unable to open display\n");
		return;
	}
#ifdef USB_PIXEL
	usb_dev_handle *handle = NULL;

	int usb_present = open_usb(&handle);
	if (!handle) {
		printf("Unable to open usb device, proceeding anyway...\n");
	}
#endif
	int radius = RADIUS;

	init_shm(d, radius);
	init_xinput(d);

	int x = -1;
	int y = -1;
	int old_x = -1;
	int old_y = -1;
	struct rgb_color color;
	color.alpha = 255;
	uint8_t buf[3];
	while(1) {
		wait_for_movement(d, &x, &y);
		if (x != old_x || y != old_y) {
			refresh_image(d, x, y, radius);
			get_pixel_color(d, x, y, &color, radius);
			printf("%d/%d\t(%d/%d/%d)\n", x, y, color.red, color.green, color.blue);
#ifdef USB_PIXEL
			if (usb_present) {
				buf[0] = color.red;
				buf[1] = color.green;
				buf[2] = color.blue;
				int8_t sent = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, CUSTOM_RQ_SET_RGB, 0, 0, buf, 3, 100);
				if (sent < 0) {
					printf("Lost contact to USB device\n");
					usb_present = 0;
					handle = NULL;
				}
			}
#endif
			old_x = x;
			old_y = y;
		}
	}
	XCloseDisplay(d);
}
