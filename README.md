# StreamProcessors

Dependencies:

texinfo (sudo apt-get install texinfo)

Build and install StreamProcessors

<todo...>

Build and install some dependencies for execution:

1) Install scons:

Go to the source folder
 
$ cd <...>/StreamProcessors/3rdplibs/scons

Uncompress the source file

$ tar -xf scons-3.0.0.tar.gz --strip 1

and do:

$ python setup.py install --prefix=<...>/StreamProcessors/_install_dir_x86/

2) Build mongodb from source code on Ubuntu:

Go to the mongodb source folder

$ cd <...>/StreamProcessors/3rdplibs/mongodb/

Uncompress the source file

$ tar -xf mongodb-src-r3.6.0.tar.gz --strip 1

Python-pip should be installed (check it by 'apt')

$ sudo apt install python-pip

Install all the nedded python dependencies...

$ pip2 install -r buildscripts/requirements.txt

#######build using scons (takes lot of time and space... more than 30GB for building)

#######$ <...>/StreamProcessors/3rdplibs/mongodb$ ../../_install_dir_x86/bin/scons all

Install:

$ <...>/StreamProcessors/3rdplibs/mongodb$ ../../_install_dir_x86/bin/scons --prefix=<...>/StreamProcessors/_install_dir_x86/ install

Start Mongodb Daemon

$ <...>/StreamProcessors/_install_dir_x86/bin/mongod -f <...>/StreamProcessors/_install_dir_x86/etc/mongod/mongod.conf

