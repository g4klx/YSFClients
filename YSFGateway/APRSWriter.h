/*
 *   Copyright (C) 2010,2011,2012,2016,2017,2018,2020 by Jonathan Naylor G4KLX
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

#ifndef	APRSWriter_H
#define	APRSWriter_H

#include "UDPSocket.h"
#include "Timer.h"

#include <string>

#if !defined(_WIN32) && !defined(_WIN64)
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <gps.h>
#else
#include <winsock.h>
#endif

class CAPRSWriter {
public:
	CAPRSWriter(const std::string& callsign, const std::string& rptSuffix, const std::string& address, unsigned int port, const std::string& suffix, bool debug);
	~CAPRSWriter();

	bool open();

	void setInfo(unsigned int txFrequency, unsigned int rxFrequency, const std::string& desc);

	void setStaticLocation(float latitude, float longitude, int height);

	void setGPSDLocation(const std::string& address, const std::string& port);

	void write(const unsigned char* source, const char* type, unsigned char radio, float latitude, float longitude);

	void clock(unsigned int ms);

	void close();

private:
	CTimer            m_idTimer;
	std::string       m_callsign;
	bool              m_debug;
	unsigned int      m_txFrequency;
	unsigned int      m_rxFrequency;
	float             m_latitude;
	float             m_longitude;
	int               m_height;
	std::string       m_desc;
	std::string       m_suffix;
	in_addr           m_aprsAddress;
	unsigned int      m_aprsPort;
	CUDPSocket        m_aprsSocket;
#if !defined(_WIN32) && !defined(_WIN64)
	bool              m_gpsdEnabled;
	std::string       m_gpsdAddress;
	std::string       m_gpsdPort;
	struct gps_data_t m_gpsdData;
#endif

	void sendIdFrameFixed();
	void sendIdFrameMobile();
};

#endif
