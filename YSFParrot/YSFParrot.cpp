/*
*   Copyright (C) 2016,2018 by Jonathan Naylor G4KLX
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

#include "StopWatch.h"
#include "YSFParrot.h"
#include "Parrot.h"
#include "Network.h"
#include "Version.h"
#include "Thread.h"
#include "Timer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
	if (argc == 1) {
		::fprintf(stderr, "Usage: YSFParrot [-d|--debug] <port>\n");
		return 1;
	}

	int n = 1U;

	bool debug = false;

	for (; n < argc-1; n++) {
		if (::strcmp(argv[n], "-d") == 0 || ::strcmp(argv[n], "--debug") == 0) {
			debug = true;
		}
	}

	unsigned int port = ::atoi(argv[n]);
	if (port == 0U) {
		::fprintf(stderr, "YSFParrot: invalid port number - %s\n", argv[n]);
		return 1;
	}

	CYSFParrot parrot(port, debug);
	parrot.run();

	return 0;
}

CYSFParrot::CYSFParrot(unsigned int port, bool debug) :
m_port(port),
m_debug(debug)
{
}

CYSFParrot::~CYSFParrot()
{
}

void CYSFParrot::run()
{
	CParrot parrot(180U);
	CNetwork network(m_port);

	bool ret = network.open();
	if (!ret)
		return;

	CStopWatch stopWatch;
	stopWatch.start();

	CTimer watchdogTimer(1000U, 0U, 1500U);
	CTimer turnaroundTimer(1000U, 2U);

	CStopWatch playoutTimer;
	unsigned int count = 0U;
	bool playing = false;

	::fprintf(stdout, "Starting YSFParrot-%s\n", VERSION);

	for (;;) {
		unsigned char buffer[200U];

		unsigned int len = network.read(buffer);
		if (len > 0U) {
			parrot.write(buffer);
			watchdogTimer.start();

			if ((buffer[34U] & 0x01U) == 0x01U) {
				turnaroundTimer.start();
				watchdogTimer.stop();
				parrot.end();
			}
		}

		if (turnaroundTimer.isRunning() && turnaroundTimer.hasExpired()) {
			if (!playing) {
				playoutTimer.start();
				playing = true;
				count = 0U;
			}

			// A frame every 100ms
			unsigned int wanted = playoutTimer.elapsed() / 100U;
			while (count < wanted) {
				len = parrot.read(buffer);
				if (len > 0U) {
					network.write(buffer);
					count++;
				} else {
					parrot.clear();
					network.end();
					turnaroundTimer.stop();
					playing = false;
					count = wanted;
				}
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		network.clock(ms);
		watchdogTimer.clock(ms);
		turnaroundTimer.clock(ms);

		if (watchdogTimer.isRunning() && watchdogTimer.hasExpired()) {
			turnaroundTimer.start();
			watchdogTimer.stop();
			parrot.end();
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	network.close();
}
