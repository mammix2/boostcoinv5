#!/bin/bash

set -e

date
ps axjf

#################################################################
# Update Ubuntu and install prerequisites for running boostcoin   #
#################################################################
sudo apt-get update
#################################################################
# Build boostcoin from source                                     #
#################################################################
NPROC=$(nproc)
echo "nproc: $NPROC"
#################################################################
# Install all necessary packages for building boostcoin           #
#################################################################
sudo apt-get install -y qt4-qmake libqt4-dev libminiupnpc-dev libdb++-dev libdb-dev libcrypto++-dev libqrencode-dev libboost-all-dev build-essential libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev libdb++-dev libssl-dev ufw git
sudo add-apt-repository -y ppa:bitcoin/bitcoin
sudo apt-get update
sudo apt-get install -y libdb4.8-dev libdb4.8++-dev

cd /usr/local
file=/usr/local/boostcoin
if [ ! -e "$file" ]
then
        sudo git clone https://github.com/BOST/boostcoin/.git
fi

cd /usr/local/boostcoin/src
file=/usr/local/boostcoin/src/boostcoind
if [ ! -e "$file" ]
then
        sudo make -j$NPROC -f makefile.unix
fi

sudo cp /usr/local/boostcoin/src/boostcoind /usr/bin/boostcoind

################################################################
# Configure to auto start at boot                                      #
################################################################
file=$HOME/.boostcoin
if [ ! -e "$file" ]
then
        sudo mkdir $HOME/.boostcoin
fi
printf '%s\n%s\n%s\n%s\n' 'daemon=1' 'server=1' 'rpcuser=u' 'rpcpassword=p' | sudo tee $HOME/.boostcoin/boostcoin.conf
file=/etc/init.d/boostcoin
if [ ! -e "$file" ]
then
        printf '%s\n%s\n' '#!/bin/sh' 'sudo boostcoind' | sudo tee /etc/init.d/boostcoin
        sudo chmod +x /etc/init.d/boostcoin
        sudo update-rc.d boostcoin defaults
fi

/usr/bin/boostcoind
echo "boostcoin has been setup successfully and is running..."
exit 0

