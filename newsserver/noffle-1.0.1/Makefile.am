# Process this file with automake to produce Makefile.in

SUBDIRS = src docs

EXTRA_DIST = $(PACKAGE).conf.example

install-data-local:
	$(INSTALL) -m 2755 -o news -g news -d $(SPOOLDIR)
	$(INSTALL) -o news -g news -d $(SPOOLDIR)/data
	$(INSTALL) -o news -g news -d $(SPOOLDIR)/lock
	$(INSTALL) -o news -g news -d $(SPOOLDIR)/requested
	$(INSTALL) -o news -g news -d $(SPOOLDIR)/outgoing
	$(INSTALL) -o news -g news -d $(SPOOLDIR)/overview
	chown -R news.news $(SPOOLDIR)
	$(INSTALL) -m 755 -o 0 -g 0 -d $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/AUTHORS $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/COPYING $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/ChangeLog $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/INSTALL $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/NEWS $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/README $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/TODO $(DOCDIR)
	$(INSTALL_DATA) -o 0 -g 0 $(srcdir)/noffle.conf.example \
                                 	$(CONFIGFILE) 
	chown news.news $(CONFIGFILE)
	chmod 600 $(CONFIGFILE) 
	@echo
	@echo "****************************************************************"
	@echo "You should now copy $(CONFIGFILE) to noffle.conf"
	@echo "and edit that copy."
	@echo "****************************************************************"
	@echo

install-exec-local:
	chown news.news $(bindir)/$(PACKAGE)
	chmod 4755 $(bindir)/$(PACKAGE)

uninstall-local:
	rm -f $(DOCDIR)/AUTHORS
	rm -f $(DOCDIR)/COPYING
	rm -f $(DOCDIR)/ChangeLog
	rm -f $(DOCDIR)/INSTALL
	rm -f $(DOCDIR)/NEWS
	rm -f $(DOCDIR)/README
	rm -f $(DOCDIR)/TODO
	rm -f $(CONFIGFILE) 
	@echo
	@echo "****************************************************************"
	@echo "The NOFFLE spool directory $(SPOOLDIR) and the configfile"
	@echo "$(CONFIGFILE) have *not* been removed. Please do this"
	@echo "manually."
	@echo "****************************************************************"
	@echo

