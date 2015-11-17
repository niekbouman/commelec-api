# Commelec’s Daemon

Commelec’s *daemon* is a program (to be run as a background process) that runs on the resource agent’s machine as a middleman between the resource agent and the (leader) grid agent.

The reason for using this daemon is:  

* to hide the details of transport protocol between the daemon and the grid agent from the user, 
* to make the interface between the resource agent and daemon very simple (based on JSON).

For requests, the daemon acts as a translator: it translates the requests as encoded in COMMELEC’s Cap’n Proto-based wire format into JSON objects, and forwards those to the resource agent. For advertisements, the daemon works as a *compiler*: the *resource-dependent* advertisement parameters, along with some static parameters (as set in the configuration file) are compiled into a COMMELEC advertisement, which is *device-independent*, and are then forwarded to the grid agent.

## Configuration 
The daemon is configured by means of a small configuration file, through which the user can set static parameters like the resource type (battery, PV, etc.) and the (unique) ID number of his resource, as well as the relevant IP addresses and port numbers. Dynamic parameters (i.e., those that are repeatedly updated in real-time) are exchanged between the daemon and the resource agent over UDP, encoded as a JSON object. (Most programming environments have out-of-the-box support for reading and writing JSON objects.)

To generate a default configuration file, run the daemon as follows (we assume that the binary is located in your current directory) 

    $ ./commelecd --generate

    Generating configuration file: daemon-cfg.json ... Done.
    You can now edit and customize this file.

If you open this configuration file, youll find that it has the following content

    {"resource-type":"battery","agent-id":1000,"GA-ip":"127.0.0.1",
     "GA-port":12345,"RA-ip":"127.0.0.1","RA-port":12342,"listenport-RA-side":12340,
     "listenport-GA-side":12341,"debug-mode":false}

The fields have the following meaning:

field | type | meaning
------|------|--------
`resource-type` | string | the advertisement type (for a list of available advertisement types, run `commelecd --list-resources` )  
`agent-id` | integer | the ID number of the resource agent (should be assigned manually, and coincide with the configuration of the grid agent)
`GA-ip`,`GA-port` |  string, integer | the IP address and port where the grid agent listens for incoming advertisements
`RA-ip`,`RA-port` | string, integer | the IP address and port of the socket of your resource agent (where it listens for incoming requests). In the typical case, the daemon runs on the same machine which then implies that `"RA-ip":"127.0.0.1"`
`listenport-GA-side` |  integer | the port where the daemon listens for requests sent by the grid agent
`listenport-RA-side` |  integer | the port where the daemon listens for advertisements as sent by your resource agent
`debug-mode` |  bool (true/false) | mode to verify correctness of the advertisements

## Running the Daemon

Once the configuration file has been properly set up, run

    $ ./commelecd daemon-cfg.json

It is also possible to let the daemon take its configuration from the standard input by means of using a dash (`-`) as filename, i.e.

    $ cat daemon-cfg.json | ./commelecd -

## Sending advertisements to other destinations
For debugging purposes, it is possible to let the daemon send the Commelec advertisements to a list of auxiliary destinations (in addition to sending it to the grid agent).

To use this feature, add the following JSON field to the configuration:

    "clone-adv":[{"ip":"<ip 1>","port":<port 1>},{"ip":"<ip 2>","port":<port 2>}]

etc.

## The `custom` Resource
It is also possible to send Commelec advertisements and receive Commelec requests in the packed Cap’n Proto representation. To use this feature, set the `resource-type` field to `custom`. (The daemon will then disable the translation from/to JSON.)


