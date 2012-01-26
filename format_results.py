import sys 
import re


def main():
    f = open(sys.argv[1])
    data=[]
    for line in f:
       data.append(line.split(" ")) 
    
    for i in range(0, 49):
        for j in range(2, 14):
           print data[j][i]  
if __name__ =="__main__": 
    main()
