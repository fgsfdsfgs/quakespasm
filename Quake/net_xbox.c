/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>
#include <string.h>

#include <lwip/debug.h>
#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>
#include <lwip/timeouts.h>
#include <lwip/errno.h>
#include <lwip/netdb.h>
#include <netif/etharp.h>
#include <nxdk/net.h>

#include <hal/debug.h>
#include <windows.h>

#include "net_xbox.h"

static BOOL xnet_inited = 0;

extern struct netif *g_pnetif;

// string versions
static char net_xbox_ip[20] = "0.0.0.0";
static char net_xbox_netmask[20] = "0.0.0.0";
static char net_xbox_gateway[20] = "0.0.0.0";

void NET_Xbox_Init(void)
{
	nxNetInit(NULL);
	xnet_inited = TRUE;
}

void NET_Xbox_Shutdown(void)
{
	nxNetShutdown();
	xnet_inited = FALSE;
}

int NET_Xbox_GetHostname(char *name, int namelen)
{
	if (!xnet_inited || !name)
	{
		errno = EFAULT;
		return -1;
	}

	const ip4_addr_t *localaddr = netif_ip4_addr(g_pnetif);
	if (localaddr)
	{
		char *ipstr = ip4addr_ntoa(localaddr);
		if (ipstr && *ipstr)
		{
			strncpy(name, ipstr, namelen);
			name[namelen - 1] = 0;
			return 0;
		}
	}

	errno = EINVAL;
	return -1;
}
