import sys
import time
import subprocess

class Iperf:


    def run(self, host, port, seconds, sleep=5):
        subprocess.call(["iperf", "-c", host, "-p", port, "-t", seconds])
        time.sleep(sleep)
		

