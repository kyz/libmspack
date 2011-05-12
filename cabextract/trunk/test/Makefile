CABX=../cabextract
GENERATED_CABS=small-none.cab huge-none.cab search.cab
REAL_CABS=huge-mszip.cab huge-lzx15.cab huge-lzx21.cab
ALL_CABS=$(REAL_CABS) $(GENERATED_CABS)

test: $(ALL_CABS)
	$(CABX) -l $(ALL_CABS) >output.txt
	$(CABX) -t $(ALL_CABS) >>output.txt
	diff expected.txt output.txt 

clean:
	-rm -f $(GENERATED_CABS) output.txt

small-none.cab:
	perl largecab.pl $@ 1 1

huge-none.cab:
	perl largecab.pl $@ 65535 65505

search.cab: $(REAL_CABS) small-none.cab huge-none.cab
	echo "spacer!" >spacer.txt
	cat spacer.txt small-none.cab spacer.txt huge-none.cab huge-mszip.cab \
	spacer.txt huge-lzx15.cab huge-none.cab huge-lzx21.cab >$@
	-rm spacer.txt
