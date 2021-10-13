#include <string.h>

void* explicit_bzero(void* s, size_t n) {
  void *ptr = memset(s, 0, n);
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
  return ptr;
}
