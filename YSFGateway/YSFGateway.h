/*
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
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

#if !defined(YSFGateway_H)
#define	YSFGateway_H

#include "YSFNetwork.h"
#include "YSFReflectors.h"
#include "WiresX.h"
#include "Timer.h"
#include "Conf.h"
#include "DTMF.h"
#include "GPS.h"

#include <string>

enum LINK_TYPE {
	LINK_NONE,
	LINK_YSF
};

class CYSFGateway
{
public:
	CYSFGateway(const std::string& configFile);
	~CYSFGateway();

	int run();

private:
	std::string     m_callsign;
	std::string     m_suffix;
	CConf           m_conf;
	CGPS*           m_gps;
	CYSFReflectors* m_reflectors;
	CWiresX*        m_wiresX;
	CDTMF           m_dtmf;
	CYSFNetwork*    m_ysfNetwork;
	LINK_TYPE       m_linkType;
	std::string     m_current;
	std::string     m_startup;
	bool            m_exclude;
	CTimer          m_inactivityTimer;
	CTimer          m_lostTimer;

	void startupLinking();
	void processWiresX(const unsigned char* buffer, unsigned char fi, unsigned char dt, unsigned char fn, unsigned char ft);
	void processDTMF(unsigned char* buffer, unsigned char dt);
	void createWiresX(CYSFNetwork* rptNetwork);
	void createGPS();
};

#endif
