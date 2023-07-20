/*
*   Copyright (C) 2016-2021 by Jonathan Naylor G4KLX
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

#if !defined(YSFReflectors_H)
#define	YSFReflectors_H

#include "UDPSocket.h"
#include "Timer.h"

#include <vector>
#include <string>

enum YSF_TYPE {
	YT_YSF,
	YT_FCS
};

class CYSFReflector {
public:
	CYSFReflector() :
	m_id(),
	m_name(),
	m_desc(),
	m_count("000"),
	m_addr(),
	m_addrLen(0U),
	m_type(YT_YSF),
	m_wiresX(false)
	{
	}

	std::string      m_id;
	std::string      m_name;
	std::string      m_desc;
	std::string      m_count;
	sockaddr_storage m_addr;
	unsigned int     m_addrLen;
	YSF_TYPE         m_type;
	bool             m_wiresX;
};

class CYSFReflectors {
public:
	CYSFReflectors(const std::string& hostsFile, unsigned int reloadTime, bool makeUpper);
	~CYSFReflectors();

	void setParrot(const std::string& address, unsigned short port);	
	void setYSF2DMR(const std::string& address, unsigned short port);
	void setYSF2NXDN(const std::string& address, unsigned short port);
	void setYSF2P25(const std::string& address, unsigned short port);
	void setYSFDirect(const std::string& address, unsigned short port);
	void addFCSRoom(const std::string& id, const std::string& name);

	bool load();

	CYSFReflector* findById(const std::string& id);
	CYSFReflector* findByName(const std::string& name);

	std::vector<CYSFReflector*>& current();

	std::vector<CYSFReflector*>& search(const std::string& name);

	bool reload();

	void clock(unsigned int ms);

private:
	std::string                 m_hostsFile;
	std::string                 m_parrotAddress;
	unsigned short              m_parrotPort;
	std::string                 m_YSF2DMRAddress;
	unsigned short              m_YSF2DMRPort;
	std::string                 m_YSF2NXDNAddress;
	unsigned short              m_YSF2NXDNPort;
	std::string                 m_YSF2P25Address;
	unsigned short              m_YSF2P25Port;
	std::string                 m_YSFDirectAddress;
	unsigned short              m_YSFDirectPort;
	std::vector<std::pair<std::string, std::string>> m_fcsRooms;
	std::vector<CYSFReflector*> m_newReflectors;
	std::vector<CYSFReflector*> m_currReflectors;
	std::vector<CYSFReflector*> m_search;
	bool                        m_makeUpper;
	CTimer                      m_timer;

	bool findById(unsigned int id) const;
};

#endif
