import sys 
import re


def main():
    f = open(sys.argv[1])
    for line in f:
        if re.search("delay:", line):
            tmp = line.split(" ")
            sys.stdout.write("\n"+tmp[1] + " " + tmp[3] + " " + tmp[5] + " " )
        elif re.search("^\[  3\].*sec", line):
            tmp =  line.split(" ")
            if tmp[4] == "sec":
                if tmp[3].split("-")[0] != "0.0":
                    sys.stdout.write(str(float(tmp[9])*0.000007629) + " " )
            elif tmp[5] == "sec": 
                if tmp[4].split("-")[0] != "0.0":
                    #sys.stdout.write(tmp[4].split("-")[1]  + " " + tmp[10] + " " )
                    sys.stdout.write(str(float(tmp[10])*0.000007629) + " " )
            else: 
                #sys.stdout.write(tmp[5] + " " +  tmp[11] + " " )
                sys.stdout.write(str(float(tmp[11])*0.000007629) + " " )

    
            
if __name__ =="__main__": 
    main()
