import matplotlib.pyplot as plt
import numpy as np

import json
import socket
import struct

npoints = 100

def makeHeader(jsonsize,capnpsize):
    return struct.pack('!2I',jsonsize,capnpsize)

def uncompress(data):
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


def makeJsonReq():
  req = dict()
  #req['resP'] = 1
  #req['resQ'] = 1
  req['dimP'] = npoints
  req['dimQ'] = npoints
  data=json.dumps(req,separators=(',',':'))
  return data
  

HOST = '127.0.0.1'    # The remote host
PORT = 50007              # The same port as used by the server

BUFSIZE = 4096

udpport = 12345

if __name__ == "__main__":
    s = None
    for res in socket.getaddrinfo(HOST, PORT, socket.AF_UNSPEC, socket.SOCK_STREAM):
        af, socktype, proto, canonname, sa = res
        try:
            s = socket.socket(af, socktype, proto)
        except socket.error as msg:
            s = None
            continue
        try:
            s.connect(sa)
        except socket.error as msg:
            s.close()
            s = None
            continue
        break
    if s is None:
        print 'could not open socket'
        sys.exit(1)



    udpsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udpsock.bind(('', udpport))

    jsnReq = makeJsonReq()
    while True:

        data, addr = udpsock.recvfrom(BUFSIZE)
        mydata = makeHeader(len(jsnReq), len(data)) +jsnReq+data
        #print ":".join("{:02x}".format(ord(c)) for c in mydata) 
        s.sendall(mydata)

        length = struct.unpack('!I',s.recv(4))[0]
        response =json.loads(s.recv(length))


        
        #print 'received ' 
        #print response['cf']['data']
        #print 
        
        uncData = uncompress(response['cf']['data'])
        #print 'len of unc data', len(uncData)
        M = np.array(uncData).reshape((npoints,npoints))
        #     np.reshape(M,(10,10))

        #print uncData
        #print M
        plt.imshow(np.flipud(M.transpose()))
        plt.show()

        #

        #print ":".join("{:02x}".format(ord(c)) for c in response) 
        #print response #[0:30] 


s.close()


#receive data

# form request

#forward to viz

# receive from viz

#visualize








#class MyUDPHandler(SocketServer.BaseRequestHandler):
#    """
#    This class works similar to the TCP handler class, except that
#    self.request consists of a pair of data and client socket, and since
#    there is no connection the client address must be given explicitly
#    when sending data back via sendto().
#    """
#    
#
#    def handle(self):
#        data = self.request[0].strip()
#        socket = self.request[1]
#        #print "{} wrote:".format(self.client_address[0])
#        #msg = schema_capnp.Message.from_bytes_packed(data)
#        #self.pp.pprint(msg.to_dict())
#
#if __name__ == "__main__":
#    HOST, PORT = "localhost", 12350
#    server = SocketServer.UDPServer((HOST, PORT), MyUDPHandler)
#    server.serve_forever()
