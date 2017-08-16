/*
 *   Copyright (C) 2012,2013,2017 by Jonathan Naylor G4KLX
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

#ifndef DTMF_H
#define	DTMF_H

#include "WiresX.h"

#include <string>

class CDTMF {
public:
	CDTMF();
	~CDTMF();

	WX_STATUS decodeVW(const unsigned char* payload);
	WX_STATUS decodeDN1(const unsigned char* payload);
	WX_STATUS decodeDN2(const unsigned char* payload);

	std::string getReflector();

	void reset();

private:
	std::string  m_data;
	std::string  m_command;
	bool         m_pressed;
	unsigned int m_releaseCount;
	unsigned int m_pressCount;
	char         m_lastChar;

	WX_STATUS decodeVWSlice(const unsigned char* ambe);
	WX_STATUS decodeDN1Slice(const unsigned char* ambe);
	WX_STATUS decodeDN2Slice(const unsigned char* ambe);
	WX_STATUS validate() const;
};

#endif
