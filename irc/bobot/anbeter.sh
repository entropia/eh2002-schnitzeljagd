#!/bin/sh

read BASE < config/base

cd ${BASE}/irc/bobot/anbeter

(while read i; do 
   sed -e "s/@IRCNAME@/$i/g" -e "s/@NICKNAME@/$i/g" bot.conf.orig > bot.$i.conf
   bobot++ -f bot.$i.conf
   sleep 3
done) < ../anbeternamen.txt
