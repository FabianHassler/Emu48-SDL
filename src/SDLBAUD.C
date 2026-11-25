/*
 *   SdlBaud.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// exact serial baudrates, also the non standard ones of the HP48
// (1920, 3840, 7680, 15360), in a file of its own because the Linux
// kernel header <asm/termbits.h> conflicts with <termios.h>

#include "pch.h"
#include <sys/ioctl.h>
#if defined __APPLE__
#include <IOKit/serial/ioss.h>
#elif defined __linux__
#include <asm/ioctls.h>
#include <asm/termbits.h>
#endif

// set the exact baudrate after tcsetattr(), FALSE if not supported
BOOL SetCommSpeed(INT nFd,DWORD dwBaudRate)
{
#if defined __APPLE__
	speed_t nSpeed = dwBaudRate;

	return ioctl(nFd,IOSSIOSPEED,&nSpeed) == 0;
#elif defined __linux__
	struct termios2 sTerm;

	if (ioctl(nFd,TCGETS2,&sTerm) != 0) return FALSE;
	sTerm.c_cflag = (sTerm.c_cflag & ~CBAUD) | BOTHER;
	sTerm.c_cflag &= ~(CBAUD << IBSHIFT);	// input speed same as output speed
	sTerm.c_ispeed = sTerm.c_ospeed = dwBaudRate;
	return ioctl(nFd,TCSETS2,&sTerm) == 0;
#else
	return FALSE;
	UNREFERENCED_PARAMETER(nFd);
	UNREFERENCED_PARAMETER(dwBaudRate);
#endif
}
