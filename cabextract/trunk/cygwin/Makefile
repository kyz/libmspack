P=cabextract
V=1.4
R=1
PVR=$(P)-$(V)-$(R)
SRC=$(P)-$(V).tar.gz
CYGPORT=cygport $(PVR).cygport

all:
	$(CYGPORT) download prep
	cp README setup.hint $(PVR)/CYGWIN-PATCHES
	$(CYGPORT) compile install package
	-rm -rf $(P)-$(V).tar.gz $(PVR)
