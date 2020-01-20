/*
 * Compile-test to check if mkdir() only takes one argument.
 */

#include <sys/stat.h>
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif

int main(void) {
    mkdir(".");
    return 0;
}
