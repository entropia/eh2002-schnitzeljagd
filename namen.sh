#!/usr/bin/ksh

for i in a bc d e f g h i j k l m n o p q r s t u v w x y z;
   do lynx -source \
      "http://www.babynames.com/V5/BabyNames_Main.php3?content=names/$i.html" |
      grep "^<TR><TD>.*<B>[A-Z]*<\/b>" | 
      sed -e "s/^<TR><TD>[^>]*><B>\([A-Z]*\)<\/b>.*/\1/g" > names.$i.txt
done
