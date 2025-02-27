#ifndef __TRACYSORT_HPP__
#define __TRACYSORT_HPP__

#if CXX_17 || defined(__EMSCRIPTEN__)
#  include "tracy_pdqsort.h"
#else
#  include <ppqsort.h>
#endif

#endif
