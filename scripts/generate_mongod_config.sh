#!/bin/bash
# $1 'mongod.conf' template file full path
# $2 <install_dir> substitute
CONFFILE="$2"/etc/mongod/mongod.conf
IFS=''
while read a ; do echo -e "${a//<install_dir>/$2}" ; done < "$1" > $CONFFILE;
