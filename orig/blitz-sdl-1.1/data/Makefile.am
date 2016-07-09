mediadir = $(pkgdatadir)
dist_media_DATA = spritesheet.bmp explosion.wav

install-data-local:
	@-$(mkinstalldirs) $(DESTDIR)$(mediadir)
	$(INSTALL_DATA) $(srcdir)/spritesheet.bmp $(DESTDIR)$(mediadir)
	$(INSTALL_DATA) $(srcdir)/explosion.wav $(DESTDIR)$(mediadir)

