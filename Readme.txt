================================================================================
NdisPyFilter 0.1.1
Copyright ©2007 Liam Kirton <liam@int3.ws>

07th September 2007
http://int3.ws/
================================================================================

Overview:
---------

NdisPyFilter is a Windows NDIS firewall that processes all incoming and
outgoing network packets through a dynamically loaded Python script. This script
has the ability to modify or drop the packet prior to it reaching the TCP/IP
stack (incoming) or network device (outgoing).

This is very much a proof-of-concept tool, and is not currently recommended for
execution outside of a virtual machine (no such testing has been performed).
If it dies with a BSOD, simply revert to the lastest snapshot!

Please email any comments, questions, bugs or suggestions to liam@int3.ws.

Notes:
------

Requires Windows XP SP2 w/ Python 2.5 (http://www.python.org/).

VMware should be configured with the network adapter in bridged mode.

When NdisPyFilterCtrl.exe is not running, packets should pass straight through
as normal.

Installation:
-------------

> Control Panel / Network Connections / <Device> Properties / Install... /
  Service / Add... / Have Disk ... / Browse ...

> Select NdisPyFilter.inf

> Click OK, etc. until it installs (accept that the driver isn't signed), make
  sure to close the "<Device> Properties" screen, as this is required to
  complete installation.

Usage:
------

> NdisPyFilterCtrl.exe <Filter.py>

> After modification to the loaded Python script, press Ctrl+Break to reload.

> Press Ctrl+C at any point to quit (and hence to stop filtering).

================================================================================
