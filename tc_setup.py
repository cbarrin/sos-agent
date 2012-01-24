import os
import sys
import subprocess
import math


class Tc:


    def __init__(self):
        if os.getuid() != 0:
            print "You must be root!"
            sys.exit(1)

        self.hosts=[["127.0.0.1", "10.0.0.13"],["hawk", '10.0.0.12']]
        self.INTERFACE="eth1"
        if sys.argv[1] == "del":
            self.clear()
            sys.exit(1)

        self.DELAY = sys.argv[1]
        self.DELTA = sys.argv[2]
        self.gbps = 134217728
        self.LIMIT = str(int(math.ceil((float(self.DELAY.split("ms")[0]) + float(self.DELTA.split("ms")[0]))*.001 *self.gbps)))
                

    def clear(self):
        for i in self.hosts:
            subprocess.call(["ssh", "-l" "root", i[0], "tc", "qdisc", "del", "dev", self.INTERFACE, "root"])
        

    def setup_qdisc(self):
        """"
        First we delete the all the tc stuff incase it's there... 
        """ 
        self.clear()
        for i in self.hosts:
            print i
            destIP = i[1] 
            subprocess.call(["ssh", "-l", "root", i[0], "tc", "qdisc", "add", "dev", self.INTERFACE, "root", "handle", "1:", "prio"])
            subprocess.call(["ssh", "-l", "root", i[0], "tc", "qdisc", "add", "dev", self.INTERFACE, "parent", "1:3", "handle", "30:", "netem", "delay", self.DELAY, self.DELTA , "limit", self.LIMIT])
            subprocess.call(["ssh", "-l", "root", i[0], "tc", "filter", "add", "dev", self.INTERFACE, "protocol", "ip", "parent", "1:0", "prio", "3", "u32", "match", "ip", "dst", destIP, "flowid", "1:3"])


def main(): 
    tc = Tc()
    tc.setup_qdisc()


if __name__ == "__main__":
    main() 
