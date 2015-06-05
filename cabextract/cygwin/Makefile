PKG=cabextract-1.6-1

PORT=$(PKG).cygport
EXTRA=README setup.hint
DIR32=$(PKG).i686
DIR64=$(PKG).x86_64

pkg32: $(DIR32)/dist/cabextract/$(PKG).tar.xz
pkg64: $(DIR64)/dist/cabextract/$(PKG).tar.xz
clean:
	rm -rf $(DIR32) $(DIR64)

$(DIR32)/dist/cabextract/$(PKG).tar.xz: $(PORT) $(EXTRA)
	cygport --32 $(PORT) download prep
	cp $(EXTRA) $(DIR32)/CYGWIN-PATCHES/
	cygport --32 $(PORT) compile install package upload

$(DIR64)/dist/cabextract/$(PKG).tar.xz: $(PORT) $(EXTRA)
	cygport --64 $(PORT) download prep
	cp $(EXTRA) $(DIR64)/CYGWIN-PATCHES/
	cygport --64 $(PORT) compile install package upload
