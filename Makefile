all:
	@echo "apacheconf - Apache Konfig neu erzeugen"

apacheconf:
	scripts/generate-apache > httpd.conf

