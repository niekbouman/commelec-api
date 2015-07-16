In this tutorial we will demonstrate how we can make a resource agent for a PV that interfaces with `commelecd`, the daemon.

## Step 1: The Resource Agent code

We will create a simple resource agent, that waits for an incoming request, and then sends the parameters for a PV advertisment to the daemon.

The source code can be downloaded here: [demo-resource-agent.py](../examples/demo-resource-agent.py). 

```Python
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

    def printRequest(requestAsDict):
        # Print the received request to the user
        print 'Received a request:'
        print 'Sender ID number:', request['senderId']
        if request['setpointValid']:
            print 'Request contains a valid setpoint for implementation,'
            print 'P = %f [Watts], Q = %f [VAR].' % (request['P'],request['Q'])

    def implementSetpoint(P,Q):
        # A dummy function that would implement the setpoint
        pass

    def computePVparameters(P,Q):
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
        request = json.loads(data) #parse the JSON data

        printRequest(request)
        [P,Q] = (request['P'],request['Q'])

        if request['setpointValid']:
            implementSetpoint(P,Q)
        else:
            [P,Q] = (1.0e3,0)
            # we did not get a valid setpoint from the grid agent
            # hence we simulate that the PV converter is currently
            # implementing the above setpoint 
            # (in a real resource agent, you would obviously not do this)

        paramsAsDict = computePVparameters(P,Q)

        # send the parameters to the daemon 
        socket.sendto(json.dumps(paramsAsDict), self.client_address)

if __name__ == "__main__":
    HOST, PORT = "localhost", 12350
    server = SocketServer.UDPServer((HOST, PORT), JSONReqHandler)
    server.serve_forever()

```

## Step 2: Configuring the daemon

Here, we assume that you have built the project, so that you can run the executable `commelecd`.

First, we will generate a default configuration file, by running
  
    commelecd --generate

This creates the file `daemon-cfg.json`. Open this file with a text editor and set "resource-type" to "pv". Also, set the appropriate ip addresses and port numbers.

## Step 3: Running the daemon and the resource agent

Open a terminal and run

    commelecd daemon-cfg.json

Then, in a new terminal, run the Python code

    python demo-resource-agent.py

work in progres...
