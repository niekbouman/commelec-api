In this tutorial we will demonstrate how we can make a resource agent for a PV that interfaces with `commelecd`, the daemon.

## Step 1: The Resource Agent code

We will create a simple resource agent, that waits for an incoming request, and then sends the parameters for a PV advertisment to the daemon.

```Python
import re
print "hello"
```

## Step 2: Configuring the daemon

Here, we assume that you have built the project, so that you can run the executable `commelecd`.

First, we will generate a default configuration file, by running
  
  commelecd --generate

This creates the file `daemon-cfg.json`. Open this file with a text editor and set "resource-type" to "pv". Also, set the appropriate ip addresses and port numbers.

## Step 3: Running

work in progres...
