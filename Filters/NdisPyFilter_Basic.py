# --------------------------------------------------------------------------------
# NdisPyFilter
#
# Copyright ©2007 Liam Kirton <liam@int3.ws>
# --------------------------------------------------------------------------------
# NdisPyFilter_Basic.py
#
# Created: 01/09/2007
# --------------------------------------------------------------------------------

import ndispyfilter

# --------------------------------------------------------------------------------

def recv_packet_filter(buffer, length):
	print "recv_packet_filter(%d)" % length
	
	packet_pass = True
	packet_checksum_fixup = True
	
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
