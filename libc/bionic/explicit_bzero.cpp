#include <string.h>

void* _Nonnull explicit_bzero(void* _Nonnull s, size_t n) {
  void *ptr = memset(s, 0, n);
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
  return ptr;
}
