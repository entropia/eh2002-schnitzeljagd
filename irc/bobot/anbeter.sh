#!/bin/sh

(while read i; do 
   sed -e "s/@IRCNAME@/$i/g" -e "s/@NICKNAME@/$i/g" anbeter/bot.conf.orig > anbeter/bot.$i.conf
   cd anbeter && bobot++ -f bot.$i.conf
   cd ..
   sleep 20
done) < anbeternamen.txt
