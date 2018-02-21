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
#include "FCSNetwork.h"
#include "WiresX.h"
#include "Conf.h"
#include "DTMF.h"
#include "GPS.h"

#include <string>

class CYSFGateway
{
public:
	CYSFGateway(const std::string& configFile);
	~CYSFGateway();

	int run();

private:
	std::string  m_callsign;
	std::string  m_suffix;
	CConf        m_conf;
	CGPS*        m_gps;
	CWiresX*     m_wiresX;
	CDTMF*       m_dtmf;
	CYSFNetwork* m_ysfNetwork;
	CFCSNetwork* m_fcsNetwork;
	bool         m_linked;
	bool         m_exclude;

	void createGPS();
};

#endif
