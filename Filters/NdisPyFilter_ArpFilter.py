# --------------------------------------------------------------------------------
# NdisPyFilter
#
# Copyright ©2007 Liam Kirton <liam@int3.ws>
# --------------------------------------------------------------------------------
# NdisPyFilter_ArpFilter.py
#
# Created: 05/09/2007
# --------------------------------------------------------------------------------

import ndispyfilter
import struct

# --------------------------------------------------------------------------------

def recv_packet_filter(buffer, length):
	print "recv_packet_filter(%d)" % length
	
	packet_pass = True
	packet_checksum_fixup = True
	
	if length >= 14: # Size Of Ethernet Frame Header
		eth_dst = buffer[0:6] # Destination MAC
		eth_src = buffer[6:12] # Source MAC
		eth_type = struct.unpack('>h', buffer[12:14])[0] # Contained Packet Type
														 
		if eth_type == 0x0806: # ARP
			if eth_src == '\x00\x0f\xb5\x1b\xa9\x46': # From 00:0f:b5:1b:a9:46
				print "- ARP Packet Dropped."
				packet_pass = False # Drop Packet
	
	return (packet_pass, packet_checksum_fixup, buffer)

# --------------------------------------------------------------------------------

def send_packet_filter(buffer, length):
	print "send_packet_filter(%d)" % length
	
	packet_pass = True
	packet_checksum_fixup = True
	
	return (packet_pass, packet_checksum_fixup, buffer)
	
# --------------------------------------------------------------------------------

if __name__ == '__main__':
	ndispyfilter.set_recv_packet_filter(recv_packet_filter)
	ndispyfilter.set_send_packet_filter(send_packet_filter)
	print '\"Filters\\NdisPyFilter_Basic.py\" Loaded.'

# --------------------------------------------------------------------------------
