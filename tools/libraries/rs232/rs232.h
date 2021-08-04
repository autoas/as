/*
***************************************************************************
*
* Author: Teunis van Beelen
*
* Copyright (C) 2005 - 2021 Teunis van Beelen
*
* Email: teuniz@protonmail.com
*
***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************
*/

/* For more info and how to use this library, visit: http://www.teuniz.net/RS-232/ */


#ifndef rs232_INCLUDED
#define rs232_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>



#if defined(__linux__) || defined(__FreeBSD__)

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/file.h>
#include <errno.h>

#else

#include <windows.h>

#endif

int RS232_OpenComport(int, int, const char *, int);
int RS232_PollComport(int, unsigned char *, int);
int RS232_SendByte(int, unsigned char);
int RS232_SendBuf(int, unsigned char *, int);
void RS232_CloseComport(int);
void RS232_cputs(int, const char *);
int RS232_IsDCDEnabled(int);
int RS232_IsRINGEnabled(int);
int RS232_IsCTSEnabled(int);
int RS232_IsDSREnabled(int);
void RS232_enableDTR(int);
void RS232_disableDTR(int);
void RS232_enableRTS(int);
void RS232_disableRTS(int);
void RS232_flushRX(int);
void RS232_flushTX(int);
void RS232_flushRXTX(int);
int RS232_GetPortnr(const char *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif


