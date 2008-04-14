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
	
	# Note, in this simple case, we cannot resize the buffer as this would
	# invalidate the TCP stream sequence/acknowledgement numbers.
	# The work-around for this is complex, but certainly do-able with the current
	# system. This shouldn't be an issue for non-TCP packets.
	
	if buffer.find('Microsoft') >= 0:
		buffer = buffer.replace('Microsoft', 'm1cro$oft') 
	
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
