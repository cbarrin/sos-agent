import os
import sys
import subprocess
import math


class Tc:

    def __init__(self):
        if os.getuid() != 0:
            print "You must be root!"
            sys.exit(1)

        self.hosts=[["127.0.0.1", "10.0.0.11/32"],["pg47", '10.0.0.12/32']]
        self.gbps = 134217728
#        self.INTERFACE="eth1"
#        if sys.argv[1] == "del":
#            self.clear()
#            sys.exit(1)

#        self.DELAY = sys.argv[1]
#        self.DELTA = sys.argv[2]
#        self.STREAMS = int(sys.argv[3])
#        self.LIMIT = str(int(math.ceil(self.STREAMS*(float(self.DELAY.split("ms")[0]) + float(self.DELTA.split("ms")[0]))*.001 *self.gbps)))
                

    def clear(self, interface="eth1"):
        for i in self.hosts:
            subprocess.call(["ssh", "-l" "root", i[0], "tc", "qdisc", "del", "dev", interface, "root"])
        

    def setup_qdisc(self, delay, delta, streams, interface="eth1"):
        """"
        First we delete the all the tc stuff incase it's there... 
        """ 
    
        self.clear()
        limit = str(int(math.ceil(streams* (float(delay) + float(delta))*.001*self.gbps)))
        print "delay: " + str(delay) + " delta: " + str(delta) + " streams " + str(streams) + " limit " + str(limit)
        if delay+delta == 0:
            return 

        for i in self.hosts:
            destIP = i[1] 
            subprocess.call(["ssh", "-l", "root", i[0], "tc", "qdisc", "add", "dev", interface, "root", "handle", "1:", "prio"])
            subprocess.call(["ssh", "-l", "root", i[0], "tc", "qdisc", "add", "dev", interface, "parent", "1:3", "handle", "30:", "netem", "delay", str(delay)+"ms", str(delta)+"ms" , "limit", limit])
            subprocess.call(["ssh", "-l", "root", i[0], "tc", "filter", "add", "dev", interface, "protocol", "ip", "parent", "1:0", "prio", "3", "u32", "match", "ip", "dst", destIP, "flowid", "1:3"])


#def main(): 
#    tc = Tc()
#    tc.setup_qdisc()


if __name__ == "__main__":
    main() 
