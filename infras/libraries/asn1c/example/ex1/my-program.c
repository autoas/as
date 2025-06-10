#include <stdio.h>   /* for stdout */
#include <stdlib.h>  /* for malloc() */
#include <assert.h>  /* for run-time control */
#include "MyTypes.h" /* Include MyTypes definition */

int encode() {
  /* Define an OBJECT IDENTIFIER value */
  int oid[] = {1, 3, 6, 1, 4, 1, 9363, 1, 5, 0}; /* or whatever */

  /* Declare a pointer to a new instance of MyTypes type */
  MyTypes_t *myType;
  /* Declare a pointer to a MyInt type */
  MyInt_t *myInt;

  /* Temporary return value */
  int ret;

  /* Allocate an instance of MyTypes */
  myType = calloc(1, sizeof *myType);
  assert(myType); /* Assume infinite memory */

  /*
   * Fill in myObjectId
   */
  ret = OBJECT_IDENTIFIER_set_arcs(&myType->myObjectId, oid, sizeof(oid[0]),
                                   sizeof(oid) / sizeof(oid[0]));
  assert(ret == 0);

  /*
   * Fill in mySeqOf with a couple of integers.
   */

  /* Prepare a certain INTEGER */
  myInt = calloc(1, sizeof *myInt);
  assert(myInt);
  *myInt = 123; /* Set integer value */

  /* Fill in mySeqOf with the prepared INTEGER */
  ret = ASN_SEQUENCE_ADD(&myType->mySeqOf, myInt);
  assert(ret == 0);

  /* Prepare another integer */
  myInt = calloc(1, sizeof *myInt);
  assert(myInt);
  *myInt = 111222333; /* Set integer value */

  /* Append another INTEGER into mySeqOf */
  ret = ASN_SEQUENCE_ADD(&myType->mySeqOf, myInt);
  assert(ret == 0);

  /*
   * Fill in myBitString
   */

  /* Allocate some space for bitmask */
  myType->myBitString.buf = calloc(1, 1);
  assert(myType->myBitString.buf);
  myType->myBitString.size = 1; /* 1 byte */

  /* Set the value of muxToken */
  myType->myBitString.buf[0] |= 1 << (7 - myBitString_muxToken);

  /* Also set the value of modemToken */
  myType->myBitString.buf[0] |= 1 << (7 - myBitString_modemToken);

  /* Trim unused bits (optional) */
  myType->myBitString.bits_unused = 6;

  /*
   * Print the resulting structure as XER (XML)
   */
  xer_fprint(stdout, &asn_DEF_MyTypes, myType);

  size_t derLen = 4096;
  uint8_t derBuffer[1024];

  asn_enc_rval_t ec = der_encode_to_buffer(&asn_DEF_MyTypes, myType, derBuffer, &derLen);

  printf("encoded der len = %d, encoded=%d\n", (int)derLen, (int)ec.encoded);
  FILE *fp = fopen("MyTypes.der", "wb");
  if (fp) {
    fwrite(derBuffer, ec.encoded, 1, fp);
    fclose(fp);
  }

  return 0;
}

int decode(const char *filename) {
  char buf[1024]; /* Hope, sufficiently large buffer */
  MyTypes_t *myType = 0;
  asn_dec_rval_t rval;
  size_t size;
  FILE *f;

  /*
   * Target variables.
   */
  int *oid_array; /* holds myObjectId */
  int oid_size;
  int *int_array; /* holds mySeqOf */
  int int_size;
  int muxToken_set;   /* holds single bit */
  int modemToken_set; /* holds single bit */

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
  rval = ber_decode(0, &asn_DEF_MyTypes, &myType, buf, size);
  assert(rval.code == RC_OK);

  /*
   * Convert the OBJECT IDENTIFIER into oid_array/oid_size pair.
   */
  /* Figure out the number of arcs inside OBJECT IDENTIFIER */
  oid_size = OBJECT_IDENTIFIER_get_arcs(&myType->myObjectId, 0, sizeof(oid_array[0]), 0);
  assert(oid_size >= 0);
  /* Create the array of arcs and fill it in */
  oid_array = malloc(oid_size * sizeof(oid_array[0]));
  assert(oid_array);
  (void)OBJECT_IDENTIFIER_get_arcs(&myType->myObjectId, oid_array, sizeof(oid_array[0]), oid_size);

  /*
   * Convert the sequence of integers into array of integers.
   */
  int_size = myType->mySeqOf.list.count;
  int_array = malloc(int_size * sizeof(int_array[0]));
  assert(int_array);
  for (int_size = 0; int_size < myType->mySeqOf.list.count; int_size++)
    int_array[int_size] = *myType->mySeqOf.list.array[int_size];

  if (myType->myBitString.buf) {
    muxToken_set = myType->myBitString.buf[0] & (1 << (7 - myBitString_muxToken));
    modemToken_set = myType->myBitString.buf[0] & (1 << (7 - myBitString_modemToken));
  } else {
    muxToken_set = modemToken_set = 0; /* Nothing is set */
  }

  /*
   * Print the resulting structure as XER (XML)
   */
  xer_fprint(stdout, &asn_DEF_MyTypes, myType);

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    return decode(argv[1]);
  }
  return encode();
}
