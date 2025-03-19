/*
 *   Copyright (C) 2020,2025 by Jonathan Naylor G4KLX
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

#ifndef	DGIdNetwork_H
#define	DGIdNetwork_H

#include <string>

enum class DGID_STATUS {
	NOTOPEN,
	NOTLINKED,
	LINKING,
	LINKED
};

class CDGIdNetwork {
public:
	CDGIdNetwork();
	virtual ~CDGIdNetwork() = 0;

	virtual std::string getDesc(unsigned int dgId) = 0;

	virtual unsigned int getDGId() = 0;

	virtual bool open() = 0;

	virtual void link() = 0;

	virtual DGID_STATUS getStatus() = 0;

	virtual void write(unsigned int dgId, const unsigned char* data) = 0;

	virtual unsigned int read(unsigned int dgid, unsigned char* data) = 0;

	virtual void clock(unsigned int ms) = 0;

	virtual void unlink() = 0;

	virtual void close() = 0;

	// Allowed modes
	unsigned char m_modes;

	bool m_static;

	unsigned int m_rfHangTime;
	unsigned int m_netHangTime;

private:
};

#endif
