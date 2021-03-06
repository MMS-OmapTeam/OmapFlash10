/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fastboot.h"
#include "omap_protocol.h"

static char ERROR[128];

extern unsigned fastbootMode;
extern int omap_send_command(omap_usb_handle *usb, const char *cmd);
extern int omap_send_command_response(omap_usb_handle *usb, const char *cmd, char *response);
extern int omap_send_download_data(omap_usb_handle *usb, const void *data, unsigned size);

extern char *fb_get_error(void);

static int check_response(fb_usb_handle *usb, unsigned size, 
                          unsigned data_okay, char *response)
{
    unsigned char status[65];
    int r;

    for(;;) {
        r = fb_usb_read(usb, status, 64);
        if(r < 0) {
            sprintf(ERROR, "status read failed (%s)", strerror(errno));
            fb_usb_close(usb);
            return -1;
        }
        status[r] = 0;

        if(r < 4) {
            sprintf(ERROR, "status malformed (%d bytes)", r);
            fb_usb_close(usb);
            return -1;
        }

        if(!memcmp(status, "INFO", 4)) {
            fprintf(stderr,"(bootloader) %s\n", status + 4);
            continue;
        }

        if(!memcmp(status, "OKAY", 4)) {
            if(response) {
                strcpy(response, (char*) status + 4);
            }
            return 0;
        }

        if(!memcmp(status, "FAIL", 4)) {
            if(r > 4) {
                sprintf(ERROR, "remote: %s", status + 4);
            } else {
                strcpy(ERROR, "remote failure");
            }
            return -1;
        }

        if(!memcmp(status, "DATA", 4) && data_okay){
            unsigned dsize = strtoul((char*) status + 4, 0, 16);
            if(dsize > size) {
                strcpy(ERROR, "data size too large");
                fb_usb_close(usb);
                return -1;
            }
            return dsize;
        }

        strcpy(ERROR,"unknown status code");
        fb_usb_close(usb);
        break;
    }

    return -1;
}

static int _command_send(fb_usb_handle *usb, const char *cmd,
                         const void *data, unsigned size,
                         char *response)
{
    int cmdsize = strlen(cmd);
    int r;
    
    if(response) {
        response[0] = 0;
    }

    if(cmdsize > 128) {
        sprintf(ERROR,"command too large");
        return -1;
    }
	
    if(fb_usb_write(usb, cmd, cmdsize) != cmdsize) {
        sprintf(ERROR,"command write failed (%s)", strerror(errno));
        fb_usb_close(usb);
        return -1;
    }

    if(data == 0) {
        return check_response(usb, size, 0, response);
    }

    r = check_response(usb, size, 1, 0);
    if(r < 0) {
        return -1;
    }
    size = r;

    if(size) {
        r = fb_usb_write(usb, data, size);
        if(r < 0) {
            sprintf(ERROR, "data transfer failure (%s)", strerror(errno));
            fb_usb_close(usb);
            return -1;
        }
        if(r != ((int) size)) {
            sprintf(ERROR, "data transfer failure (short transfer)");
            fb_usb_close(usb);
            return -1;
        }
    }
    
    r = check_response(usb, 0, 0, 0);
    if(r < 0) {
        return -1;
    } else {
        return size;
    }
}

int fb_command(usb_handle *usb, const char *cmd)
{
	return fastbootMode? _command_send(usb, cmd, 0, 0, 0) :
					omap_send_command(usb, cmd);
}

int fb_command_response(usb_handle *usb, const char *cmd, char *response)
{
	return fastbootMode? _command_send(usb, cmd, 0, 0, response) :
					omap_send_command_response(usb, cmd, response);
}

int fb_download_data(usb_handle *usb, const void *data, unsigned size)
{
	if(fastbootMode) {
		char cmd[128];
		int r;
	    
		sprintf(cmd, "download:%08x", size);

		r = _command_send(usb, cmd, data, size, 0);
	    
		if(r < 0) {
			return -1;
		} else {
			return 0;
		}
	} else {
		//return omap_send_download_data(usb, data, size);
        sprintf(ERROR, "fastboot download commands require switch to fastboot mode");
        fb_usb_close(usb);
        return -1;
	}
}

