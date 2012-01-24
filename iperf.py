import sys
import time
import subprocess

class Iperf:
	def __init__(self):
		self.runs = 100
		self.host = "awesome"
		self.time = "10"
		self.port = "5001"
	def start(self):
		for i in range(0, self.runs):	
			subprocess.call(["iperf", "-c", self.host, "-p", self.port, "-t", self.time])
			time.sleep(10)
		
def main():
	iperf = Iperf()
	iperf.start()


if __name__=="__main__":
	main()
