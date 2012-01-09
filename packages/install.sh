wget http://130.127.62.2/protobuf-2.4.1.tar.bz2
wget http://130.127.62.2/protobuf-c-0.15.tar.gz
echo export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib >> ~/.bashrc
source ~/.bashrc
tar -xvf protobuf-2.4.1.tar.bz2 
cd protobuf-2.4.1
./autogen.sh
./configure
make
sudo make install 
cd ..
tar -xvf protobuf-c-0.15.tar.gz
cd protobuf-c-0.15
./configure
make
sudo make install 
cd .. 

