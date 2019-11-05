#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "pmserver_test.h"
#include "../src/server.h"

using std::vector;

// ================ running tests ================

test_results results;

void test_results_init() {
  results.num_tests = results.num_errors = 0;
}

void test_passed() {
  ++results.num_tests;
  printf(".");
}

void test_failed() {
  ++results.num_tests;
  ++results.num_errors;
  printf("*");
}

int test_num_errors() {
  return results.num_errors;
}

void print_time() {
  rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  printf("\n\nFinished in %ld.%06ld seconds\n", usage.ru_utime.tv_sec,
         (long)usage.ru_utime.tv_usec);
}

void print_results() {
  printf("\nTests run: %d, tests passed: %d, tests failed: %d\n",
         results.num_tests, results.num_tests - results.num_errors,
         results.num_errors);
  printf("done\n");
}

// ================ pmserver tests ================

void hex_word_test(const char * const str, byte expected[], int num_expected) {
  vector<byte> bytes;
  char errmsg[BUFSIZ];

  hex_word_to_bytes(str, bytes);

  sprintf(errmsg, "%s converted length %d is wrong, expected %d\n",
          str, (int)bytes.size(), num_expected);
  tassert(bytes.size() == num_expected, errmsg);

  for (int i = 0; i < num_expected; ++i) {
    byte seen = bytes.at(i);
    sprintf(errmsg, "%s index %d bad byte 0x%02x seen, expected 0x%02x\n",
            str, i, seen, expected[i]);
    tassert(seen == expected[i], errmsg);
  }
}

void test_hex_word_to_bytes() {
  byte bytes[BUFSIZ];

  memset(bytes, 0, BUFSIZ);

  hex_word_test("", bytes, 0);
  hex_word_test("0", bytes, 1);

  bytes[0] = 0x0a;
  hex_word_test("a", bytes, 1);
  hex_word_test("A", bytes, 1);
  hex_word_test("0a", bytes, 1);
  hex_word_test("0A", bytes, 1);

  bytes[0] = 0x1a;
  hex_word_test("1a", bytes, 1);
  hex_word_test("1A", bytes, 1);

  bytes[0] = 0x12;
  bytes[1] = 0x34;
  bytes[2] = 0x0a;
  hex_word_test("1234", bytes, 2);

  fprintf(stderr, "expect to see an error message here about odd # digits\n");
  hex_word_test("1234a", bytes, 2);
}

void run_tests() {
  test_hex_word_to_bytes();
}

void run_tests_and_print_results() {
  run_tests();
  print_time();
  print_results();
}

// ================ main ================

int main(int argc, const char **argv) {
  run_tests_and_print_results();
  exit(results.num_errors == 0 ? 0 : 1);
  return 0;
}
