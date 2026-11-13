/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* HLP decompression implementation */

#include <stdio.h>
#include <system.h>
#include <hlp.h>

#define HLPD_FLAG_NONE 0
#define HLPD_FLAG_FAST_HEADERS 1

off_t hlpd_strnlen(const char *string, off_t max) {
	off_t i;

	for (i = 0; i < max; i++) {
		if (string[i] == '\0') {
			break;
		}
	}
	return i;
}

static char *hlpd_strdup(struct mspack_system *sys, const char *str) {
    size_t len;
    char *copy;
    
    len = strlen(str) + 1;
    copy = (char *)sys->alloc(sys, len);
    if (copy) {
        sys->copy((void *)str, copy, len);
    }
    return copy;
}

static int hlpd_find_from_btree(struct mspack_system *sys, struct mspack_file *fh, struct mshlpd_header_p *hlp, const char *filename, off_t *out_offset) {
	int i;
	short current_page, n_levels;
	
	current_page = hlp->directory_bh.root_page;
	n_levels = hlp->directory_bh.n_levels;
	if (!hlp->page_buffer) {
		if ((hlp->page_buffer = sys->alloc(sys, hlp->directory_bh.page_size))) {
			hlp->page_buffer_sz = hlp->directory_bh.page_size;
		} else {
			return MSPACK_ERR_NOMEMORY;
		}
	} else {
		if (hlp->page_buffer_sz < hlp->directory_bh.page_size) {
			sys->free(hlp->page_buffer);
			if ((hlp->page_buffer = sys->alloc(sys, hlp->directory_bh.page_size))) {
				hlp->page_buffer_sz = hlp->directory_bh.page_size;
			} else {
				return MSPACK_ERR_NOMEMORY;
			}
		}
	}
	
	while (n_levels-- > 0 && current_page >= 0) {
		short n_entries;
		
		if (sys->seek(fh, hlp->first_page_offset + (current_page * hlp->directory_bh.page_size), MSPACK_SYS_SEEK_START)) {
			return MSPACK_ERR_SEEK;
		}
		
		if (sys->read(fh, hlp->page_buffer, hlp->directory_bh.page_size) != hlp->directory_bh.page_size) {
			return MSPACK_ERR_READ;
		}
		
		n_entries = (short)EndGetI16(((unsigned char *)hlp->page_buffer)+2);

		if (n_levels > 0) { 
			unsigned char *p, *end;
			
			p = (unsigned char *)hlp->page_buffer + 4;
			end = (unsigned char *)hlp->page_buffer + hlp->directory_bh.page_size;

			for (i = 0; i < n_entries; i++) {
				char *entry_name; 
				off_t max_len, name_len;

				entry_name = (char *)(p + 2);
				max_len = end - (p + 2);
				name_len = hlpd_strnlen(entry_name, max_len);
				
				if (name_len == max_len || (p + 2 + name_len + 1 + 2) > end) {
					return MSPACK_ERR_DATAFORMAT;
				}

				if (strcmp(entry_name, filename) > 0) {
					break;
				}
				
				p += 2 + name_len + 1; 
			}

			current_page = (short)EndGetI16(p);
		} else {
			unsigned char *p, *end;

			p = (unsigned char *)hlp->page_buffer + 8;
			end = (unsigned char *)hlp->page_buffer + hlp->directory_bh.page_size;

			for (i = 0; i < n_entries; i++) {
				char *entry_name; 
				off_t max_len, name_len;
				int cmp;
				
				entry_name = (char *)p;
				max_len = end - p;
				name_len = hlpd_strnlen(entry_name, max_len);

				if (name_len == max_len || (p + name_len + 1 + 4) > end) {
					return MSPACK_ERR_DATAFORMAT;
				}

				cmp = strcmp(entry_name, filename);
				if (!cmp) {
					*out_offset = (off_t)EndGetI32(p + name_len + 1);
					return MSPACK_ERR_OK;
				} else if (cmp > 0) {
					break; 
				}

				p += name_len + 1 + 4;
			}
		}
	}
	
	return MSPACK_ERR_DATAFORMAT;
}

static int hlpd_add_files(struct mspack_system *sys, struct mspack_file *fh, struct mshlpd_header_p *hlp) {
	struct mshlpd_file *cur, *prev;
	unsigned char *buf;
    int i;
	short current_page, n_levels;
	
	cur = prev = NULL;
	current_page = hlp->directory_bh.root_page;
	n_levels = hlp->directory_bh.n_levels;
    if (!hlp->directory_bh.total_entries || current_page < 0) {
        return MSPACK_ERR_OK; 
    }

	if (!hlp->page_buffer) {
		if ((hlp->page_buffer = sys->alloc(sys, hlp->directory_bh.page_size))) {
			hlp->page_buffer_sz = hlp->directory_bh.page_size;
		} else {
			return MSPACK_ERR_NOMEMORY;
		}
	} else {
		if (hlp->page_buffer_sz < hlp->directory_bh.page_size) {
			sys->free(hlp->page_buffer);
			if ((hlp->page_buffer = sys->alloc(sys, hlp->directory_bh.page_size))) {
				hlp->page_buffer_sz = hlp->directory_bh.page_size;
			} else {
				return MSPACK_ERR_NOMEMORY;
			}
		}
	}
    buf = (unsigned char *)hlp->page_buffer;

    while (--n_levels > 0) {
        if (current_page < 0)  {
			return MSPACK_ERR_DATAFORMAT;
		}
		
        if (sys->seek(fh, hlp->first_page_offset + (current_page * hlp->directory_bh.page_size), MSPACK_SYS_SEEK_START)) {
            return MSPACK_ERR_SEEK;
        }
        
        if (sys->read(fh, buf, hlp->directory_bh.page_size) != hlp->directory_bh.page_size) {
            return MSPACK_ERR_READ;
        }

        current_page = (short)EndGetI16(buf + 4);
    }

    while (current_page >= 0) {
		unsigned char *p, *end;
		short n_entries, next_page;

        if (sys->seek(fh, hlp->first_page_offset + (current_page * hlp->directory_bh.page_size), MSPACK_SYS_SEEK_START)) {
            return MSPACK_ERR_SEEK;
        }
        if (sys->read(fh, buf, hlp->directory_bh.page_size) != hlp->directory_bh.page_size) {
            return MSPACK_ERR_READ;
        }

        n_entries = (short)EndGetI16(buf + 2);
        next_page = (short)EndGetI16(buf + 6);

        p = buf + 8;
        end = buf + hlp->directory_bh.page_size;
        for (i = 0; i < n_entries; i++) {
            char *entry_name;
            off_t max_len;
            off_t name_len;
            off_t file_offset;

			entry_name = (char*)p;
			max_len = end - p;
			name_len = hlpd_strnlen(entry_name, max_len);
            if (name_len == max_len || (p + name_len + 1 + 4) > end) {
                return MSPACK_ERR_DATAFORMAT;
            }
            file_offset = (off_t)EndGetI32(p + name_len + 1);

			if (!hlp->base.files) {
				if (!(hlp->base.files = (struct mshlpd_file *)sys->alloc(sys, sizeof(struct mshlpd_file)))) {
					return MSPACK_ERR_NOMEMORY;
				}
				
				hlp->base.files->header = (struct mshlpd_header*)hlp;
				hlp->base.files->filename = hlpd_strdup(sys, entry_name);
				hlp->base.files->offset = file_offset;
				hlp->base.files->length = 0;
				hlp->base.files->next = NULL;
				prev = hlp->base.files;
			} else {
				if (!(cur = (struct mshlpd_file *)sys->alloc(sys, sizeof(struct mshlpd_file)))) {
					return MSPACK_ERR_NOMEMORY;
				}
				
				cur->header = (struct mshlpd_header*)hlp;
				cur->filename = hlpd_strdup(sys, entry_name);
				cur->offset = file_offset;
				cur->length = 0;
				cur->next = NULL;
				if (prev) {
					prev->next = cur;
					prev = cur;
				}
			}

            p += name_len + 1 + 4;
        }

        current_page = next_page;
    }

	for (cur = hlp->base.files; cur; cur = cur->next) {
		unsigned char cbuf[9];

		if (sys->seek(fh, cur->offset, MSPACK_SYS_SEEK_START)) {
			return MSPACK_ERR_SEEK;
		}
		
		if (sys->read(fh, cbuf, 9) != 9) {
			return MSPACK_ERR_READ;
		}

		cur->offset += 9;
		cur->length = (off_t)EndGetI32(cbuf + 4);
	}

    return MSPACK_ERR_OK;
}


static int hlpd_read_headers(struct mspack_system *sys, struct mspack_file *fh, struct mshlpd_header_p *hlp, unsigned char flags) {
	unsigned char buf[38];
	int err;
	
	/* Read HELPHEADER */
	if (sys->read(fh, buf, 16) != 16) {
		return MSPACK_ERR_READ;
	}
	
	if (((int)EndGetI32(buf)) != 0x00035F3F) {
		return MSPACK_ERR_SIGNATURE;
	}
	
	hlp->directory_start = ((int)EndGetI32(buf+4));
	hlp->free_list_start = ((int)EndGetI32(buf+8));
	hlp->base.length = ((int)EndGetI32(buf+12));
	
	/* Read internal directory */
	if (sys->seek(fh, hlp->directory_start, MSPACK_SYS_SEEK_START)) {
		return MSPACK_ERR_SEEK;
	}
	
	if (sys->read(fh, buf, 9) != 9) {
		return MSPACK_ERR_READ;
	}
	hlp->directory_fh.reserved = ((int)EndGetI32(buf));
	hlp->directory_fh.used = ((int)EndGetI32(buf+4));
	
	if (sys->read(fh, buf, 38) != 38) {
		return MSPACK_ERR_READ;
	}

	if (EndGetI16(buf) != 0x293B) {
		return MSPACK_ERR_SIGNATURE;
	}
	
	hlp->directory_bh.flags = EndGetI16(buf+2);
	hlp->directory_bh.page_size = EndGetI16(buf+4);
	sys->copy(buf+6, hlp->directory_bh.structure, 16);
	hlp->directory_bh.page_splits = (short)EndGetI16(buf+24);
	hlp->directory_bh.root_page = (short)EndGetI16(buf+26);
	hlp->directory_bh.total_pages = (short)EndGetI16(buf+30);
	hlp->directory_bh.n_levels = (short)EndGetI16(buf+32);
	hlp->directory_bh.total_entries = (int)EndGetI16(buf+34);
	hlp->first_page_offset = hlp->directory_start + 47;
	hlp->page_buffer = NULL;
	
	if (flags & HLPD_FLAG_FAST_HEADERS) {
		return MSPACK_ERR_OK;
	}

	/* Build file list */
	hlp->base.files = NULL;
	err = hlpd_add_files(sys, fh, hlp);
	if (err != MSPACK_ERR_OK) {
		return err;
	}
	
	return MSPACK_ERR_OK;
}

static struct mshlpd_header *hlpd_open_real(struct mshlp_decompressor *base, const char *filename, unsigned char flags) {
	struct mshlp_decompressor_p *self;
	struct mshlpd_header_p *hlp;
	struct mspack_system *sys;
	struct mspack_file *fh;

	if (!base) {
		return NULL;
	}
	
	self = (struct mshlp_decompressor_p *)base;
	sys = self->system;
	hlp = NULL;
	
	if ((fh = sys->open(sys, filename, MSPACK_SYS_OPEN_READ))) {
		if ((hlp = (struct mshlpd_header_p *)sys->alloc(sys, sizeof(struct mshlpd_header_p)))) {
			hlpd_read_headers(sys, fh, hlp, flags);
			hlp->base.filename = filename;
		} else {
			self->error = MSPACK_ERR_NOMEMORY;
		}
		sys->close(fh);
	} else {
		self->error = MSPACK_ERR_OPEN;
	}
	
	return (struct mshlpd_header*)hlp;
}

static struct mshlpd_header *hlpd_open(struct mshlp_decompressor *base, const char *filename) {
	return hlpd_open_real(base, filename, HLPD_FLAG_NONE);
}

static struct mshlpd_header *hlpd_fast_open(struct mshlp_decompressor *base, const char *filename) {
	return hlpd_open_real(base, filename, HLPD_FLAG_FAST_HEADERS);
}

static int hlpd_last_error(struct mshlp_decompressor *base) {
	struct mshlp_decompressor_p *self;

	self = (struct mshlp_decompressor_p *)base; 
	if (self) {
		return self->error;
	} else {
		return MSPACK_ERR_ARGS;
	}
}

static void hlpd_close(struct mshlp_decompressor *base, struct mshlpd_header *hbase) {
	struct mshlp_decompressor_p *self;
	struct mspack_system *sys;
	struct mshlpd_header_p *hlp;

	if (!base) {
		return;
	}
	
	self = (struct mshlp_decompressor_p *)base; 
	hlp = (struct mshlpd_header_p *)hbase; 
	self->error = MSPACK_ERR_OK;
	sys = self->system;

	if (hlp->page_buffer) {
		sys->free(hlp->page_buffer);
	}
	
	if (hlp->base.files) {
		struct mshlpd_file *cur, *nxt;

		for (cur = hlp->base.files; cur; cur = nxt) {
			nxt = cur->next;
			if (cur->filename) {
				sys->free((void*)cur->filename);
			}
			sys->free(cur);
		}
	}
	
	sys->free(hlp);
}

static int hlpd_fast_find(struct mshlp_decompressor *base, struct mshlpd_header *hbase, const char *filename, struct mshlpd_file *f_ptr, int f_size) {
	struct mshlp_decompressor_p *self;
	struct mshlpd_header_p *hlp;
	struct mspack_system *sys;
    struct mspack_file *fh;
	unsigned char buf[9];

    hlp = (struct mshlpd_header_p *)hbase;
	self = (struct mshlp_decompressor_p *)base; 

    if (!self || !hlp || !f_ptr || !filename || (f_size != sizeof(struct mshlpd_file))) {
        return MSPACK_ERR_ARGS;
    }
    
    sys = self->system;
    memset(f_ptr, 0, f_size);
    if (!(fh = sys->open(sys, hbase->filename, MSPACK_SYS_OPEN_READ))) {
        return MSPACK_ERR_OPEN;
    }

	self->error = MSPACK_ERR_OK;
	if (hlpd_find_from_btree(sys, fh, hlp, filename, &f_ptr->offset) != MSPACK_ERR_OK) {
		memset(f_ptr, 0, f_size);
		goto retrn;
	}

	if (sys->seek(fh, f_ptr->offset, MSPACK_SYS_SEEK_START)) {
		self->error = MSPACK_ERR_SEEK;
		goto retrn;
	}

	if (sys->read(fh, buf, 9) != 9) {
		self->error = MSPACK_ERR_READ;
		goto retrn;
	} else {
		f_ptr->offset += 9;
	}

	f_ptr->length = (off_t)EndGetI32(buf+4);
	f_ptr->header = hbase;
retrn:
    sys->close(fh);
    return self->error;	
}

static int hlpd_extract(struct mshlp_decompressor *base, struct mshlpd_file *file, const char *filename) {
    struct mshlp_decompressor_p *self;
    struct mspack_system *sys;
    struct mspack_file *in_fh, *out_fh;
    unsigned char *buf;
    off_t remaining;

	self = (struct mshlp_decompressor_p *)base;
	in_fh = NULL;
	out_fh = NULL;
	buf = NULL;
    if (!self || !file || !file->header || !filename) {
        return MSPACK_ERR_ARGS;
    }

    sys = self->system;
    self->error = MSPACK_ERR_OK;

    if (!(buf = sys->alloc(sys, 512))) {
        return self->error = MSPACK_ERR_NOMEMORY;
    }

    if (!(in_fh = sys->open(sys, file->header->filename, MSPACK_SYS_OPEN_READ))) {
        self->error = MSPACK_ERR_OPEN;
        goto cleanup;
    }
    
    if (sys->seek(in_fh, file->offset, MSPACK_SYS_SEEK_START)) {
        self->error = MSPACK_ERR_SEEK;
        goto cleanup;
    }

    if (!(out_fh = sys->open(sys, filename, MSPACK_SYS_OPEN_WRITE))) {
        self->error = MSPACK_ERR_OPEN;
        goto cleanup;
    }

    remaining = file->length;
    while (remaining > 0) {
        int chunk;
        int bytes_read;
        
        chunk = (remaining > 512) ? 512 : (int)remaining;
        bytes_read = sys->read(in_fh, buf, chunk);

        if (bytes_read != chunk) {
            self->error = MSPACK_ERR_READ;
            goto cleanup;
        }
        
        if (sys->write(out_fh, buf, chunk) != chunk) {
            self->error = MSPACK_ERR_WRITE;
            goto cleanup;
        }
        
        remaining -= chunk;
    }

cleanup:
    if (out_fh) {
		sys->close(out_fh);
	}
    if (in_fh) { 
		sys->close(in_fh); 
	}
    if (buf) {
		sys->free(buf);
	}
    return self->error;
}

struct mshlp_decompressor *mspack_create_hlp_decompressor(struct mspack_system *sys) {
	struct mshlp_decompressor_p *self;
	
	self = NULL;
	if (!sys) sys = mspack_default_system;
	if (!mspack_valid_system(sys)) return NULL;

	if ((self = (struct mshlp_decompressor_p *)sys->alloc(sys, sizeof(struct mshlp_decompressor_p)))) {
		self->base.open = &hlpd_open;
		self->base.close = &hlpd_close;
		self->base.extract = &hlpd_extract;
		self->base.last_error = &hlpd_last_error;
		self->base.fast_open = &hlpd_fast_open;
		self->base.fast_find = &hlpd_fast_find;
		self->system = sys;
		self->error = MSPACK_ERR_OK;
	}
	
	return (struct mshlp_decompressor *)self;
}

void mspack_destroy_hlp_decompressor(struct mshlp_decompressor *base) {
	struct mshlp_decompressor_p *self;
	
	self = (struct mshlp_decompressor_p *)base;
	if (self) {
		struct mspack_system *sys;
		
		sys = self->system;
		sys->free(self);
	}
}
