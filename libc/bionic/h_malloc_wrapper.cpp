#include <errno.h>
#include <malloc.h>
#include <sys/param.h>
#include <unistd.h>

#include <private/MallocXmlElem.h>

#include "h_malloc.h"

__BEGIN_DECLS
int h_malloc_info(int options, FILE* fp);
__END_DECLS

int h_malloc_info(int options, FILE* fp) {
  if (options != 0) {
    errno = EINVAL;
    return -1;
  }

  MallocXmlElem root(fp, "malloc", "version=\"jemalloc-1\"");

  // Dump all of the large allocations in the arenas.
  for (size_t i = 0; i < h_mallinfo_narenas(); i++) {
    struct mallinfo mi = h_mallinfo_arena_info(i);
    if (mi.hblkhd != 0) {
      MallocXmlElem arena_elem(fp, "heap", "nr=\"%d\"", i);
      {
        MallocXmlElem(fp, "allocated-large").Contents("%zu", mi.ordblks);
        MallocXmlElem(fp, "allocated-huge").Contents("%zu", mi.uordblks);
        MallocXmlElem(fp, "allocated-bins").Contents("%zu", mi.fsmblks);

        size_t total = 0;
        for (size_t j = 0; j < h_mallinfo_nbins(); j++) {
          struct mallinfo mi = h_mallinfo_bin_info(i, j);
          if (mi.ordblks != 0) {
            MallocXmlElem bin_elem(fp, "bin", "nr=\"%d\"", j);
            MallocXmlElem(fp, "allocated").Contents("%zu", mi.ordblks);
            MallocXmlElem(fp, "nmalloc").Contents("%zu", mi.uordblks);
            MallocXmlElem(fp, "ndalloc").Contents("%zu", mi.fordblks);
            total += mi.ordblks;
          }
        }
        MallocXmlElem(fp, "bins-total").Contents("%zu", total);
      }
    }
  }

  return 0;
}
