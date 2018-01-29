/*
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
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

#if !defined(WIRESX_H)
#define	WIRESX_H

#include "Reflectors.h"
#include "Network.h"
#include "Timer.h"

#include <string>

enum WX_STATUS {
	WXS_NONE,
	WXS_CONNECT,
	WXS_DISCONNECT
};

enum WXSI_STATUS {
	WXSI_NONE,
	WXSI_DX,
	WXSI_CONNECT,
	WXSI_DISCONNECT,
	WXSI_ALL,
	WXSI_SEARCH
};

class CWiresX {
public:
	CWiresX(const std::string& callsign, const std::string& suffix, CNetwork* network, const std::string& hostsFile, unsigned int reloadTime);
	~CWiresX();

	void setInfo(const std::string& name, unsigned int txFrequency, unsigned int rxFrequency);
	void setParrot(const std::string& address, unsigned int port);
	void setYSF2DMR(const std::string& address, unsigned int port);

	bool start();

	WX_STATUS process(const unsigned char* data, const unsigned char* source, unsigned char fi, unsigned char dt, unsigned char fn, unsigned char ft);

	CYSFReflector* getReflector() const;
	CYSFReflector* getReflector(const std::string& id);

	void processConnect(CYSFReflector* reflector);
	void processDisconnect(const unsigned char* source = NULL);

	void clock(unsigned int ms);

private:
	std::string    m_callsign;
	std::string    m_node;
	CNetwork*      m_network;
	CReflectors    m_reflectors;
	CYSFReflector* m_reflector;
	std::string    m_id;
	std::string    m_name;
	unsigned char* m_command;
	unsigned int   m_txFrequency;
	unsigned int   m_rxFrequency;
	CTimer         m_timer;
	unsigned char  m_seqNo;
	unsigned char* m_header;
	unsigned char* m_csd1;
	unsigned char* m_csd2;
	unsigned char* m_csd3;
	WXSI_STATUS    m_status;
	unsigned int   m_start;
	std::string    m_search;

	WX_STATUS processConnect(const unsigned char* source, const unsigned char* data);
	void processDX(const unsigned char* source);
	void processAll(const unsigned char* source, const unsigned char* data);

	void sendDXReply();
	void sendConnectReply();
	void sendDisconnectReply();
	void sendAllReply();
	void sendSearchReply();
	void sendSearchNotFoundReply();

	void createReply(const unsigned char* data, unsigned int length);
	unsigned char calculateFT(unsigned int length, unsigned int offset) const;
};

#endif
