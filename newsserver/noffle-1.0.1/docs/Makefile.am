# Process this file with automake to produce Makefile.in

man_MANS = $(PACKAGE).1 $(PACKAGE).conf.5

EXTRA_DIST = INTERNALS FAQ NOTES ${man_MANS} $(PACKAGE).lsm

install-data-local:
	$(INSTALL) -o 0 -g 0 -d $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/INTERNALS $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/FAQ $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/NOTES $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/$(PACKAGE).lsm $(DOCDIR)

uninstall-local:
	rm -f $(DOCDIR)/INTERNALS
	rm -f $(DOCDIR)/FAQ
	rm -f $(DOCDIR)/NOTES
	rm -f $(DOCDIR)/$(PACKAGE).lsm

