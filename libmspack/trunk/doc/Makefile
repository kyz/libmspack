all: index.html

clean:
	-rm -f *.html doxygen.*

index.html: ../mspack/mspack.h Doxyfile
	doxygen
