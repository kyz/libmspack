/* This file is part of libmspack.
 * (C) 2021 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <mspack.h>


extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct mscabd_cabinet *cab;
	struct mscab_decompressor *cabd;
	
	char cab_file[256];
	sprintf(cab_file, "/tmp/libfuzzer.cab");

	FILE *fp = fopen(cab_file, "wb");
	if (!fp)
		return 0;
	fwrite(data, size, 1, fp);
	fclose(fp);
	
	cabd = mspack_create_cab_decompressor(NULL);
	if(cabd==NULL){
		return 0;
	}
	if(cab = cabd->open(cabd, cab_file)){
		int err = cabd->extract(cabd, cab->files, "/tmp");
	}
	
	cabd->close(cabd, cab);
	
	mspack_destroy_cab_decompressor(cabd);
	std::remove(cab_file);
	return 0;
}
