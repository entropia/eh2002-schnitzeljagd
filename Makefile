all:
	@echo "-----------------------------------------"
	@echo "apacheconf   - Apache Konfig neu erzeugen"
	@echo "p2p          - Prison 2 Prison Sender"
	@echo
	@echo "clean        - Saustall aufräumen"
	@echo
	@echo "blorg        - Alles von oben"
	@echo "-----------------------------------------"

blorg: apacheconf p2p
	
apacheconf:
	scripts/generate-apache > httpd.conf

p2p: p2p.c
	gcc -ggdb -Wall p2p.c -o p2p

clean:
	- rm -f httpd.conf p2p
	

