/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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

#include "WiresX.h"

#include <cstdio>
#include <cassert>

CWiresX::CWiresX(CNetwork* network) :
m_network(network),
m_reflector()
{
	assert(network != NULL);
}

CWiresX::~CWiresX()
{
}

WX_STATUS CWiresX::process(const unsigned char* data, unsigned char fi, unsigned char dt, unsigned char fn)
{
	return WXS_NONE;
}

std::string CWiresX::getReflector() const
{
	return m_reflector;
}
