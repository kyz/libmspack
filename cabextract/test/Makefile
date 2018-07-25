CABX=../cabextract
LIST_CABS=case.cab dirwalk-vulns.cab encoding-koi8.cab encoding-latin1.cab encoding-sjis.cab search.cab simple.cab utf8-stresstest.cab
TEST_CABS=mixed.cab search.cab simple.cab split-1.cab split-2.cab split-3.cab split-4.cab split-5.cab

test: $(LIST_CABS:.cab=.list) $(TEST_CABS:.cab=.test)
	sh scripts/check-results.sh
clean:
	rm -f *.list *.test large-files.cab large-search.cab

%.list: %.cab $(CABX)
	$(CABX) -l $< >$@
%.test: %.cab $(CABX)
	$(CABX) -t $< >$@

case.list: case.cab $(CABX)
	$(CABX) -l $< >$@
	$(CABX) -L -l $< >>$@
	$(CABX) -d DIR/PATH -l $< >>$@
	$(CABX) -d DIR/PATH -L -l $< >>$@
encoding-koi8.list: encoding-koi8.cab $(CABX)
	$(CABX) -e koi8-ru -l $< >$@
encoding-latin1.list: encoding-latin1.cab $(CABX)
	$(CABX) -e iso-8859-1 -l $< >$@
encoding-sjis.list: encoding-sjis.cab $(CABX)
	$(CABX) -e sjis -l $< >$@
mixed.test: mixed.cab $(CABX)
	$(CABX) -p $< >$@
	$(CABX) -p $< -F 'mszip.*' >>$@
	$(CABX) -p $< -F '*zx*'    >>$@
	$(CABX) -p $< -F '*m.txt'  >>$@

# these filename-only cabinets can be regnerated from source text files
case.cab: scripts/case.txt
	perl scripts/filenames.pl $< >$@
dirwalk-vulns.cab: scripts/dirwalk-vulns.txt
	perl scripts/filenames.pl $< >$@
encoding-koi8.cab: scripts/encoding-koi8.txt
	FORCE_CODEPAGE=1 perl scripts/filenames.pl $< >$@
encoding-latin1.cab: scripts/encoding-latin1.txt
	FORCE_CODEPAGE=1 perl scripts/filenames.pl $< >$@
encoding-sjis.cab: scripts/encoding-sjis.txt
	FORCE_CODEPAGE=1 perl scripts/filenames.pl $< >$@
utf8-stresstest.cab: scripts/utf8-stresstest.txt
	FORCE_UTF8=1 perl scripts/filenames.pl $< > $@

# this tests cabextract's largefile support.
# It doesn't run by default, because it takes a long time
# and needs 12GB of disk space
test-large: large-search.list large-files.test
	sh scripts/check-results.sh
large-search.cab: simple.cab scripts/large-cab.pl
	( echo 'first spacer';  perl scripts/large-cab.pl 1 1; \
	  echo 'second spacer'; perl scripts/large-cab.pl 65535 65505; cat simple.cab; \
	  echo 'third spacer';  perl scripts/large-cab.pl 65535 65505; cat simple.cab; \
	  echo 'fourth spacer'; cat simple.cab ) >$@
large-files.cab: scripts/large-files-cab.cab $(CABX)
	$(CABX) -q $<

