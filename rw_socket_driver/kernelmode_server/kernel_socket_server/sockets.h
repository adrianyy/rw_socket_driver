#pragma once

extern "C"
{
	#include "ksocket/ksocket.h"
	#include "ksocket/berkeley.h"
}

typedef int SOCKET;

#define INVALID_SOCKET  (SOCKET)(-1)
#define SOCKET_ERROR            (-1)