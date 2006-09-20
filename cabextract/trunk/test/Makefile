test: small.cab huge.cab huger.cab
	../src/cabinfo   small.cab >cabinfo_small.txt
	../src/cabinfo   huge.cab  >cabinfo_huge.txt
	../src/cabinfo   huger.cab >cabinfo_huger.txt
	../cabextract -l small.cab >cabextract_l_small.txt
	../cabextract -l huge.cab  >cabextract_l_huge.txt
	../cabextract -l huger.cab >cabextract_l_huger.txt
	../cabextract -t small.cab >cabextract_t_small.txt
	../cabextract -t huge.cab  >cabextract_t_huge.txt
	../cabextract -t huger.cab >cabextract_t_huger.txt

clean:
	-rm -f small.cab huge.cab huger.cab cabinfo_*.txt cabextract_*.txt \
	test1.bin test2.bin

small.cab:
	./largecab.pl small.cab 1 1

huge.cab:
	./largecab.pl huge.cab 65535 65505

huger.cab: huge.cab
	echo "spacer!" >spacer.txt
	cat spacer.txt huge.cab spacer.txt huge.cab >huger.cab
	-rm spacer.txt
