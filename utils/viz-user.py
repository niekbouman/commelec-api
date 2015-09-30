"""
Usage example of the visualization server ("viz")

We require that matplotlib is installed.

"""

import matplotlib.pyplot as plt
import numpy as np
import json
import socket
import struct

def makeHeader(jsonsize,capnpsize):
    """Make the header for TCP framing (so that the server knows
       how many bytes to read from the stream)

       The format is as follows:

       [4 byte unsigned integer] - size of json render request 
       [4 byte unsigned integer] - size of capnproto advertisement

       The integers should be stored in network (big-end) byte order"""
    return struct.pack('!2I',jsonsize,capnpsize)

def uncompress(data):
    """
    Uncompression function for runlength-compressed matrix data
    Runlength compression is applied to zeros (0.0) and not-a-number
    values (which are used to indicate that a point lies outside the
    PQ profile). The NaN values are encoded as -1, since JSON does
    not support NaN values.
    
    The compression format is as follows:
      - in case a value is either 0.0 or 1.0, it is followed by an
        integer that gives the multiplicity k where k>=1.
      - otherwise, the value is stored in the ordinary way  
    """
    output = []
    i=0
    while i<len(data):
        if data[i] not in [-1.0,0.0]:
            output.append(data[i])
        else:
            val = float('nan') if data[i]==-1.0 else data[i]
            output.extend([val for x in range(data[i+1])])
            i+=1
        i+=1
    return output

def makeJsonReq(dimP,dimQ):
    """
    Make render request

    Currently, we specify the desired image resolution.
    Alternatively, we can specify the resolution in terms of 
    Watts per pixel (for P) and VAR per pixel (for Q)
    """
    req = dict()
    #req['resP'] = 1
    #req['resQ'] = 1
    req['dimP'] = dimP
    req['dimQ'] = dimQ
    data=json.dumps(req,separators=(',',':'))
    return data

def hexdump(data):
    print ":".join("{:02x}".format(ord(c)) for c in data) 

def readbytes(sock, length):
    """Read a (large) number of bytes from a socket"""
    chunksize = 4096
    chunks = length / chunksize
    rest   = length % chunksize
    readdata = ''
    for i in range(chunks):
        data = sock.recv(chunksize)
        if len(data) != chunksize:
            print 'len error'
        readdata += data
    readdata += sock.recv(rest)
    return readdata

def main():

    res = 100
    Ppixels = 64
    Qpixels = 64
    # resolution of the plot

    listenPort = 12345
    # listen for incoming advertisements on this port

    vizHost = '127.0.0.1'
    vizPort = 50007
    # endpoint where the (headless) vizualization server is running

    udpsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udpsock.bind(('', listenPort))
    # listen on the UDP port for incoming advertisements

    jsnReq = makeJsonReq(Ppixels,Qpixels)
    # the render-request (i.e., plotting paramters) is static, 
    # hence it can be made before entering the loop below

    plt.ion()
    plt.show()
    # enable interactive plotting mode

    while True:

        data, addr = udpsock.recvfrom(1<<16)
        # receive Cap'n Proto advertisement on the UDP socket

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((vizHost, vizPort))
        # connect to viz-server

        s.sendall(makeHeader(len(jsnReq), len(data)) + jsnReq + data)
        # forward render request + capnp-advertisement to viz-server
        # (over TCP)

        length = struct.unpack('!I',s.recv(4))[0]
        # read response: first the length 
        # (encoded as one 4-byte unsigned int in network byte order

        response = json.loads(readbytes(s,length))
        # read the actual (json) data
       
        s.close()

        # check for errors
        if 'errors' in response.keys():
            print 'Errors occurred!'
            for err in response['errors']:
                print '[code: %d]' % err['code'], err['msg'] 

        else:
            uncData = uncompress(response['cf']['data'])
            # uncompress matrix-data

            M = np.array(uncData).reshape((Qpixels,Ppixels))
            # reshape into a numpy array

            plt.imshow(np.flipud(M.transpose()))
            plt.draw()
            # plot M
   
# this only runs if the module was not imported
if __name__ == "__main__":
    main()
