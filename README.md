![Eskenas cli build - Ubuntu 18.04](https://github.com/EskenasCoin/eskenas/workflows/Eskenas%20cli%20build%20-%20Ubuntu%2018.04/badge.svg)\
![Eskenas cli build - Ubuntu 20.04](https://github.com/EskenasCoin/eskenas/workflows/Eskenas%20cli%20build%20-%20Ubuntu%2020.04/badge.svg)\
![Eskenas cli build - Windows cross compile 18.04](https://github.com/EskenasCoin/eskenas/workflows/Eskenas%20cli%20build%20-%20Windows%20cross%20compile%2018.04/badge.svg)\
![Eskenas cli build - Windows cross compile 20.04](https://github.com/EskenasCoin/eskenas/workflows/Eskenas%20cli%20build%20-%20Windows%20cross%20compile%2020.04/badge.svg)\
![Eskenas cli build - MacOS 10.15 Catalina](https://github.com/EskenasCoin/eskenas/workflows/Eskenas%20cli%20build%20-%20MacOS%2010.15%20Catalina/badge.svg)\
![Eskenas Logo]( "Eskenas Coin Logo")


## Eskenas Chain

This is the official Eskenas Chain sourcecode repository.

## Development Resources

- Eskenas Chain Website: [https://eskenascoin.com](https://eskenascoin.com/)
- Komodo Platform: [https://komodoplatform.com](https://komodoplatform.com/)
- Eskenas Blockexplorer: [https://explorer.eskenascoin.com](https://eskenascoin.com/)
- Eskenas Discord: [https://eskenascoin.com/discord](https://eskenascoin.com/discord)
- BTT ANN: [https://bitcointalk.org/index.php?topic=4979549.0](https://bitcointalk.org/index.php?topic=4979549.0/)
- Mail: [marketing@eskenascoin.com](mailto:marketing@eskenascoin.com)
- Support: [https://eskenascoin.com/discord](https://eskenascoin.com/discord)
- API references & Dev Documentation: [https://docs.eskenascoin.com](https://docs.eskenascoin.com/)
- Blog: [https://eskenascoin.com/blog](https://eskenascoin.com/blog/)
- Whitepaper: [Eskenas Chain Whitepaper](https://eskenascoin.com/whitepaper)

## Komodo Platform Technologies Integrated In Eskenas Chain

- Delayed Proof of Work (dPoW) - Additional security layer and Komodos own consensus algorithm (to be implemented)
- zk-SNARKs - Komodo Platform's privacy technology for shielded transactions  


## Tech Specification
- Max Supply: 200 million ESKN
- Block Time: 60s
- Block Reward:

ERA0: ends before block 64. blockreward: 0.00001 ESKN (premine)

ERA1: ends before block 65. blockreward: 1921160 ESKN (premine)

ERA2: ends before block 143. blockreward: 2000000 ESKN (premine)

ERA3: ends before block 525000. blockreward: 0.01 ESKN

ERA4: ends before block 1050000. blockreward: 0.02 ESKN

ERA5: ends before block 1575000. blockreward: 0.03 ESKN

ERA6: ends before block 2100000. blockreward: 0.04 ESKN

ERA7: ends before block 2625000. blockreward: 0.05 ESKN

ERA8: forever. from block 2625000, blockreward is 10 but is halved every 525000 blocks (approximately 4 years)

- Mining Algorithm: Equihash 200,9 with Adaptive PoW developed by Komodo
- Coinbase transactions can be shielded after 10 confirmations

## About this Project
Eskenas Coin (ESKN) is a 100% private send cryptocurrency. It uses a privacy protocol that cannot be compromised by other users activity on the network. Most privacy coins are riddled with holes created by optional privacy. Eskenas Chain uses zk-SNARKs to shield 100% of the peer to peer transactions on the blockchain making for highly anonymous and private transactions.

## Getting started
Build the code as described below. To see instructions on how to construct and send an offline transaction look
at README_offline_transaction_signing.md

A list of outstanding improvements is included in README_todo.md

### Dependencies

```shell
#The following packages are needed:
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool libncurses-dev unzip git python zlib1g-dev wget bsdmainutils automake libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler libqrencode-dev libdb++-dev ntp ntpdate nano software-properties-common curl libevent-dev libcurl4-gnutls-dev cmake clang libsodium-dev -y
```

### Build Eskenas

This software is based on zcash and considered experimental and is continuously undergoing heavy development.

The dev branch is considered the bleeding edge codebase while the master-branch is considered tested (unit tests, runtime tests, functionality). At no point of time do the Eskenas developers take any responsibility for any damage out of the usage of this software.
Eskenas builds for all operating systems out of the same codebase. Follow the OS specific instructions from below.

#### Linux
```shell
git clone https://github.com/EskenasCoin/eskenas --branch eskenas
cd eskenas
# This step is not required for when using the Qt GUI
./zcutil/fetch-params.sh

# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use; -j$(nproc) for all threads available

#For CLI binaries
./zcutil/build.sh -j8

#For qt GUI binaries
./zcutil/build-qt-linux.sh -j8.
```

#### OSX
Ensure you have [brew](https://brew.sh) and the command line tools installed (comes automatically with XCode) and run:
```shell
brew update
brew upgrade
brew tap discoteq/discoteq; brew install flock
brew install autoconf autogen automake gcc@8 binutilsprotobuf coreutils wget python3
git clone https://github.com/EskenasCoin/eskenas --branch eskenas
cd eskenas
# This step is not required for when using the Qt GUI
./zcutil/fetch-params.sh

# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use; -j$(nproc) for all threads available

#For CLI binaries
./zcutil/build-mac.sh -j8

#For qt GUI binaries
./zcutil/build-qt-mac.sh -j8
```

#### Windows
Use a debian cross-compilation setup with mingw for windows and run:
```shell
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python python-zmq zlib1g-dev wget libcurl4-gnutls-dev bsdmainutils automake curl cmake mingw-w64
curl https://sh.rustup.rs -sSf | sh
source $HOME/.cargo/env
rustup target add x86_64-pc-windows-gnu

sudo update-alternatives --config x86_64-w64-mingw32-gcc
# (configure to use POSIX variant)
sudo update-alternatives --config x86_64-w64-mingw32-g++
# (configure to use POSIX variant)

git clone https://github.com/EskenasCoin/eskenas --branch eskenas
cd eskenas
# This step is not required for when using the Qt GUI
./zcutil/fetch-params.sh

# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use; -j$(nproc) for all threads available

#For CLI binaries
./zcutil/build-win.sh -j8

#For qt GUI binaries
./zcutil/build-qt-win.sh -j8
```
**Eskenas is experimental and a work-in-progress.** Use at your own risk.

To run the Eskenas GUI wallet:

**Linux**
`eskenas-qt-linux`

**OSX**
`eskenas-qt-mac`

**Windows**
`eskenas-qt-win.exe`


To run the daemon for Eskenas Chain:  
`pirated`
both pirated and eskenas-cli are located in the src directory after successfully building  

To reset the Eskenas Chain blockchain change into the *~/.komodo/PIRATE* data directory and delete the corresponding files by running `rm -rf blocks chainstate debug.log komodostate db.log` and restart daemon

To initiate a bootstrap download on the GUI wallet add bootstrap=1 to the PIRATE.conf file.


**Eskenas is based on Komodo which is unfinished and highly experimental.** Use at your own risk.

License
-------
For license information see the file [COPYING](COPYING).


Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
