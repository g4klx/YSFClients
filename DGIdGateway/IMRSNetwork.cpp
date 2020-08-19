/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "IMRSNetwork.h"
#include "YSFDefines.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>


CIMRSNetwork::CIMRSNetwork(const std::vector<IMRSDestination*>& destinations, bool debug)
{
}

CIMRSNetwork::~CIMRSNetwork()
{
}

bool CIMRSNetwork::open()
{
	LogMessage("Opening IMRS network connection");

	return true;
}

void CIMRSNetwork::write(unsigned int dgid, const unsigned char* data)
{
	assert(data != NULL);
}

void CIMRSNetwork::link()
{
}

void CIMRSNetwork::unlink()
{
}

void CIMRSNetwork::clock(unsigned int ms)
{
}

unsigned int CIMRSNetwork::read(unsigned int dgid, unsigned char* data)
{
	assert(data != NULL);

	return 0U;
}

void CIMRSNetwork::close()
{
	LogMessage("Closing IMRS network connection");
}

