These programs are clients for the System Fusion networking now built into the MMDVM Host.

The Parrot can be used as a functional replacement for the built-in parrot that was removed from the MMDVM Host when the networking was added. The Parrot is very simple minded and can only handle one client at a time and is therefore not suitable for use as a shared resource via the Internet.

The YSF Gateway allows for use of Yaesu Wires-X commands from the radio to control the listing and access to the various reflectors (rooms in Wires-X parlance). It optionally sends System Fusion GPS information to aprs.fi.

The DG-ID Gateway allows the use of the DG-ID setting on the radio to control access to the different System Fusion network systems. It optionally sends System Fusion GPS information to aprs.fi.

The Gateways have ini files that contain the parameters for running the software. The filename of the ini file is passed as a parameter on the command line. The Parrot takes the UDP port number to listen on as an argument.

The MMDVM .ini file should have the IP address and port number of the client in the [System Fusion Network] settings.

The file that contains the information about the reachable YSF reflectors is held in the YSFHosts.txt files that should be donwloaded from the DVRef.com web site. A script to do this under Linux is included. This is handled automatically in WPSD and Pi-Star.

They build on 32-bit and 64-bit Linux as well as on Windows using Visual Studio 2019 on x86 and x64.

This software is licenced under the GPL v2 and is primarily intended for amateur and educational use.
