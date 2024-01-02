#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <catch2/catch_all.hpp>
#include "../src/server.h"

#define CATCH_CATEGORY "[hex]"

using std::cerr;
using std::endl;
using std::vector;

// ================ pmserver tests ================

struct MockServer : Server {
  MockServer() {};
};

void hex_word_test(const char * const str, byte expected[], int num_expected) {
  MockServer server;
  vector<byte> bytes;
  char errmsg[BUFSIZ];

  server.hex_word_to_bytes(str, bytes);
  REQUIRE(bytes.size() == num_expected);

  for (int i = 0; i < num_expected; ++i) {
    byte seen = bytes.at(i);
    REQUIRE(seen == expected[i]);
  }
}

TEST_CASE("hex word to bytes", CATCH_CATEGORY) {
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

  cerr << "expect to see an error message here about odd # digits" << endl;
  hex_word_test("1234a", bytes, 2);
}
