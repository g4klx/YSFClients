/*
*   Copyright (C) 2016,2017,2018,2020 by Jonathan Naylor G4KLX
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

#if !defined(DGIdGateway_H)
#define	DGIdGateway_H

#include "APRSWriter.h"
#include "Conf.h"
#include "GPS.h"

#include <string>

class CDGIdGateway
{
public:
	CDGIdGateway(const std::string& configFile);
	~CDGIdGateway();

	int run();

private:
	std::string  m_callsign;
	std::string  m_suffix;
	CConf        m_conf;
	CAPRSWriter* m_writer;
	CGPS*        m_gps;

	std::string calculateLocator();
	void createGPS();
	void sendPips(unsigned int n);
};

#endif
