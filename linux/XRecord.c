/* JNativeHook: Global keyboard and mouse hooking for Java.
 * Copyright (C) 2006-2011 Alexander Barker.  All Rights Received.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/extensions/record.h>

static Display * disp_data;
static Display * disp_ctrl;
static XRecordContext context;
#ifdef ASYNC
bool running = true;
#endif

//struct taken from libxnee
typedef union {
	unsigned char		type;
	xEvent				event;
	xResourceReq		req;
	xGenericReply		reply;
	xError				error;
	xConnSetupPrefix	setup;
} XRecordDatum;

void callback(XPointer pointer, XRecordInterceptData * hook) {
	//Make sure that our data come from a legitimate source.
	if (hook->category != XRecordFromServer && hook->category != XRecordFromClient) {
		XRecordFreeData(hook);
		return;
	}

	//Convert the hook data to an XRecordDatum.
	XRecordDatum * data = (XRecordDatum *) hook->data;

	switch(data->type) {
		case KeyPress:
			//Stop looping if the escape key is pressed.
			if (XKeycodeToKeysym(disp_ctrl, data->event.u.u.detail, 0) == XK_Escape) {
				#ifdef ASYNC
					running = false;
				#else
					XRecordDisableContext(disp_ctrl, context);
					XSync(disp_ctrl, True);
				#endif
			}

			printf("Key Press - %i\n", (int) XKeycodeToKeysym(disp_ctrl, data->event.u.u.detail, 0));
		break;

		case KeyRelease:
			printf("Key Release - %i\n", (int) XKeycodeToKeysym(disp_ctrl, data->event.u.u.detail, 0));
		break;

		case ButtonPress:
			printf("Button Press - %i\n", data->event.u.u.detail);
		break;

		case ButtonRelease:
			printf("Button Release - %i\n", data->event.u.u.detail);
		break;

		case MotionNotify:
			printf("Motion Notify - %i, %i\n", data->event.u.keyButtonPointer.rootX, data->event.u.keyButtonPointer.rootY);
		break;

		default:
			printf("Unknown Event - %x\n", data->type);
		break;
	}

	XRecordFreeData(hook);
}


int main(int argc, const char * argv[]) {
	//Try to attach to the default X11 display.
	disp_data = XOpenDisplay(NULL);
	disp_ctrl = XOpenDisplay(NULL);
	if(disp_data == NULL || disp_ctrl == NULL) {
		printf("Error: Could not open display!\n");
		return EXIT_FAILURE;
	}

	//Setup XRecord range
	XRecordClientSpec clients = XRecordAllClients;
	XRecordRange * range = XRecordAllocRange();
	if (range == NULL) {
		printf("Error: Could not allocate XRecordRange!");
		return EXIT_FAILURE;
	}

	//Create XRecord Context
	range->device_events.first = KeyPress;
	range->device_events.last = MotionNotify;
	context = XRecordCreateContext(disp_data, 0, &clients, 1, &range, 1);
	XFree(range);
	if (context == 0) {
		printf("Error: Could not create XRecordContext!");
		return EXIT_FAILURE;
	}

	//Start XRecord process
	#ifdef ASYNC
		XRecordEnableContextAsync(disp_data, context, callback, NULL);
		while(running) {
			XRecordProcessReplies(disp_data);
		}
		XRecordDisableContext(disp_ctrl, context);
		XSync(disp_ctrl, True);
	#else
		XRecordEnableContext(disp_data, context, callback, NULL);
	#endif

	XRecordFreeContext(disp_ctrl, context);

	//Disable the XRecord context on the data display and close the connection.
	XCloseDisplay(disp_ctrl);

	//Free the XRecord context on the hook display and close the connection.
	XCloseDisplay(disp_data);

	return EXIT_SUCCESS;
}
