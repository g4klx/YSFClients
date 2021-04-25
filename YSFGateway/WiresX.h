/*
*   Copyright (C) 2016,2017,2018,2019,2020 by Jonathan Naylor G4KLX
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

#include "YSFReflectors.h"
#include "YSFNetwork.h"
#include "YSFFICH.h"
#include "Timer.h"
#include "StopWatch.h"
#include "RingBuffer.h"

#include <string>

enum WX_STATUS {
	WXS_NONE,
	WXS_CONNECT_YSF,
	WXS_CONNECT_FCS,
	WXS_DISCONNECT
};

enum WXSI_STATUS {
	WXSI_NONE,
	WXSI_DX,
	WXSI_CONNECT,
	WXSI_DISCONNECT,
	WXSI_ALL,
	WXSI_SEARCH,
	WXSI_CATEGORY
};

class CWiresX {
public:
	CWiresX(const std::string& callsign, const std::string& suffix, CYSFNetwork* network, CYSFReflectors& reflectors);
	~CWiresX();

	void setInfo(const std::string& name, unsigned int txFrequency, unsigned int rxFrequency);
	void setParrot(const std::string& address, unsigned short port);
	void setYSF2DMR(const std::string& address, unsigned short port);
	void setYSF2NXDN(const std::string& address, unsigned short port);
	void setYSF2P25(const std::string& address, unsigned short port);
	void addFCSRoom(const std::string& id, const std::string& name);

	bool start();
	bool isBusy() const;

	WX_STATUS process(const unsigned char* data, const unsigned char* source, const CYSFFICH& fich, bool wiresXCommandPassthrough);

	CYSFReflector* getReflector() const;
	void setReflector(CYSFReflector* reflector);

	void processConnect(CYSFReflector* reflector);
	void processDisconnect(const unsigned char* source = NULL);

	void sendConnect(CYSFNetwork* network);

	void clock(unsigned int ms);

private:
	std::string     m_callsign;
	std::string     m_node;
	CYSFNetwork*    m_network;
	CYSFReflectors& m_reflectors;
	CYSFReflector*  m_reflector;
	std::string     m_id;
	std::string     m_name;
	unsigned char*  m_command;
	unsigned int    m_txFrequency;
	unsigned int    m_rxFrequency;
	CTimer          m_timer;
	unsigned char   m_seqNo;
	unsigned char*  m_header;
	unsigned char*  m_csd1;
	unsigned char*  m_csd2;
	unsigned char*  m_csd3;
	WXSI_STATUS     m_status;
	unsigned int    m_start;
	std::string     m_search;
	std::vector<CYSFReflector*> m_category;
	bool            m_busy;
	CTimer          m_busyTimer;
	CStopWatch      m_txWatch;
	CRingBuffer<unsigned char> m_bufferTX;

	WX_STATUS processConnect(const unsigned char* source, const unsigned char* data);
	void processDX(const unsigned char* source);
	void processAll(const unsigned char* source, const unsigned char* data);
	void processCategory(const unsigned char* source, const unsigned char* data);

	void sendDXReply();
	void sendConnectReply();
	void sendDisconnectReply();
	void sendAllReply();
	void sendSearchReply();
	void sendSearchNotFoundReply();
	void sendCategoryReply();

	void createReply(const unsigned char* data, unsigned int length, CYSFNetwork* network = NULL);
	void writeData(const unsigned char* data, CYSFNetwork* network, bool isYSF2XX);
	unsigned char calculateFT(unsigned int length, unsigned int offset) const;
};

#endif
