#ifndef PMSERVER_TEST_H
#define PMSERVER_TEST_H

#include <stdio.h>

typedef struct test_results {
  int num_tests;
  int num_errors;
} test_results;

void test_results_init();
void test_passed();
void test_failed();
int test_num_errors();

#define test_run(name)                          \
  {                                             \
    int num_errors = test_num_errors();         \
    name();                                     \
    if (test_num_errors() == num_errors)        \
      test_passed();                            \
  }

#define tassert(test, errmsg)                                           \
  {                                                                     \
    if (!(test)) {                                                      \
      fprintf(stderr, "\n%s:%d:0: error: %s\n", __FILE__, __LINE__, errmsg ? errmsg : "test failed"); \
      test_failed();                                                    \
      return;                                                           \
    }                                                                   \
  }

#endif /* PMSERVER_TEST_H */
