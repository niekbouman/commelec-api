import json
import SocketServer

class JSONReqHandler(SocketServer.BaseRequestHandler):
    """
    Simulate a PV resource agent

    The program works as an echo server; upon reception of a request,
    the resource agent implements the requested setpoint (if it is valid),     
    and computes new parameters for the (PV) advertisement and sends these
    to the daemon.
    """

    def printRequest(self,requestAsDict):
        # Print the received request to the user
        print 'Received a request:'
        print 'Sender ID number:', requestAsDict['senderId']
        if requestAsDict['setpointValid']:
            print 'Request contains a valid setpoint for implementation,'
            print 'P = %f [Watts], Q = %f [VAR].' % (requestAsDict['P'],requestAsDict['Q'])

    def implementSetpoint(self,P,Q):
        # A dummy function that would implement the setpoint
        pass

    def computePVparameters(self,P,Q):
        p=dict()

        # PQ profile parameters
        p['Pmax'] = 10e3
        p['Srated'] = 12e3
        p['cosPhi'] = .8 # power factor

        # max power-drop in [W] for the belief function
        p['Pdelta'] = 8e3

        # cost function parameters
        p['a_pv'] = 1.0
        p['b_pv'] = 1.0

        # implemented setpoint; simulate error free implementation
        p['Pimp'] = P
        p['Qimp'] = Q

        return p

    def handle(self):
        socket = self.request[1] 
        data = self.request[0].strip() # extract the UDP payload
        req = json.loads(data) #parse the JSON data

        self.printRequest(req)
        (P,Q) = (req['P'],req['Q'])
        
        if req['setpointValid']:
            self.implementSetpoint(P,Q)
        else:
            (P,Q) = (1.0e3,0)
            # we did not get a valid setpoint from the grid agent
            # hence we simulate that the PV converter is currently
            # implementing the above setpoint 
            # (in a real resource agent, you would obviously not do this)

        paramsAsDict = self.computePVparameters(P,Q)

        # send the parameters to the daemon 
        socket.sendto(json.dumps(paramsAsDict), self.client_address)

if __name__ == "__main__":
    HOST, PORT = "localhost", 12350
    server = SocketServer.UDPServer((HOST, PORT), JSONReqHandler)
    server.serve_forever()

