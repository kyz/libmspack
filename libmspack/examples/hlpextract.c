#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mspack.h>

int main(int argc, char *argv[]) {
	struct mshlp_decompressor *hlpd;
	struct mshlpd_header *hlp;

	if ((hlpd = mspack_create_hlp_decompressor(NULL))) {
		if ((hlp = hlpd->open(hlpd, argv[1]))) {
			struct mshlpd_file *cur;

			for (cur = hlp->files; cur; cur = cur->next) {
				printf("%s (%ld@%ld)\n", cur->filename, cur->length, cur->offset);
			}

			hlpd->close(hlpd, hlp);
		}
		mspack_destroy_hlp_decompressor(hlpd);
	}
	return 0;
}
