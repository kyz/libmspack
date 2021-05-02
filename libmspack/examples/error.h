#include <stdio.h>
#include "mspack.h"

/**
 * Returns a string with an error message appropriate for the last error
 * of an mspack compressor or decompressor.
 * 
 * For use with mspack compressor and decompressor struct pointers.
 *
 * Example usage:
 *
 * @code
 *   if (chmd->extract(chmd, f[i], outname)) {
 *       printf("%s: extract error on \"%s\": %s\n",
 *            *argv, f[i]->filename, MSPACK_ERROR(chmd));
 *   }
 * @endcode
 *
 * @param  base   An mspack compressor or decompressor struct pointer.
 * @return a constant string with an appropriate error message.
 */
#define MSPACK_ERROR(base) mspack_error_msg(base->last_error(base))

/**
 * A function to convert the MSPACK error codes into strings.
 *
 * Example usage:
 *
 * @code
 *   if (err != MSPACK_ERR_OK) {
 *       fprintf(stderr, "%s -> %s: %s\n", argv[1], argv[2], mspack_error_msg(err));
 *   }
 * @endcode
 *
 * @param  int   An MSPACK_ERR code.
 * @return a constant string with an appropriate error message.
 */
static inline const char *mspack_error_msg(int error) {
    static char buf[32];
    switch (error) {
    case MSPACK_ERR_OK:         return "no error";
    case MSPACK_ERR_ARGS:       return "bad arguments to library function";
    case MSPACK_ERR_OPEN:       return "error opening file";
    case MSPACK_ERR_READ:       return "read error";
    case MSPACK_ERR_WRITE:      return "write error";
    case MSPACK_ERR_SEEK:       return "seek error";
    case MSPACK_ERR_NOMEMORY:   return "out of memory";
    case MSPACK_ERR_SIGNATURE:  return "bad signature";
    case MSPACK_ERR_DATAFORMAT: return "error in data format";
    case MSPACK_ERR_CHECKSUM:   return "checksum error";
    case MSPACK_ERR_CRUNCH:     return "compression error";
    case MSPACK_ERR_DECRUNCH:   return "decompression error";
    }

    snprintf(buf, sizeof(buf), "unknown error %d", error);
    return buf;
}
