#include <stdio.h>  /* for stdout */
#include <stdlib.h> /* for malloc() */
#include <assert.h> /* for run-time control */
#include "Certificate.h"

int main(int argc, char *argv[]) {
  static char buf[1024 * 1024]; /* Hope, sufficiently large buffer */
  Certificate_t *ca = NULL;
  asn_dec_rval_t rval;
  size_t size;
  FILE *f;

  const char *filename = argv[1];
  /*
   * Read in the input file.
   */
  f = fopen(filename, "rb");
  assert(f);
  size = fread(buf, 1, sizeof buf, f);
  if (size == 0 || size == sizeof buf) {
    fprintf(stderr, "%s: Too large input\n", filename);
    exit(1);
  }

  /*
   * Decode the DER buffer.
   */
  rval = ber_decode(0, &asn_DEF_Certificate, &ca, buf, size);
  assert(rval.code == RC_OK);

  /*
   * Print the resulting structure as XER (XML)
   */
  xer_fprint(stdout, &asn_DEF_Certificate, ca);

  return 0;
}
