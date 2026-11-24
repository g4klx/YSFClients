The DG-ID Gateway interfaces the MMDVM Host to the YSF/FCS/IMRS networks using the DG-ID setting on your radio. It also gateways position information to aprs.fi if sent.

The file YSFHosts.txt holds information about the reflectors available.

## XLX module selection by DG-ID

XLX reflectors select their module by DG-ID: 10 is module A, 11 module B, and so on to 35 for module Z. These DG-IDs are fixed by XLXd and are not configurable.

This only applies when DG-ID 0 is the YSF Gateway, a [DGId=0] section with Type=Gateway:

- Without a [DGId=0] section, or with any other type on DG-ID 0 (YSF, FCS, IMRS, Parrot, a cross-mode bridge), DGIdGateway behaves as before: a DG-ID from 10 to 35 without a section of its own goes nowhere.
- Frames are only sent while DGIdGateway's link to the YSF Gateway is up, as for DG-ID 0 itself.
- Module selection needs the YSF Gateway linked to an XLX reflector. Linked to anything else, the frames go there as an ordinary call.

When the YSF Gateway is on DG-ID 0 (Type=Gateway), a DG-ID from 10 to 35 with no section of its own in DGIdGateway.ini is sent through the YSF Gateway with its DG-ID unchanged, whether the gateway is idle or still in its hang time. With the YSF Gateway linked to an XLX reflector, the DG-ID set on the radio selects the module. Replies come back as DG-ID 0, as XLX sends them.

A DG-ID from 10 to 35 that has its own section is a local DG-ID, and the local one wins: it always uses its own network, so the XLX module with the same DG-ID cannot be selected by DG-ID from this hotspot.
