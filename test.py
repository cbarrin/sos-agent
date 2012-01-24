import os 
from tc import *
from iperf import *


def main():
    tc = Tc() 
    iperf = Iperf()
    runs = 10
    time = "10"
    delay = 0 
    delta = 0 
    increment = 10 

    for i in range(0, runs): 
       tc.setup_qdisc(delay, delta, 1)
       delay = delay + increment
       iperf.run("10.0.0.11", "5004", time)






if __name__=="__main__":
    main()
