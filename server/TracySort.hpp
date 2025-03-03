#ifndef __TRACYSORT_HPP__
#define __TRACYSORT_HPP__

#if !((defined(__cplusplus) && (__cplusplus >= 202002L)) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
#define CUSTOM_SORT
#endif

#if defined(CUSTOM_SORT) || defined(__EMSCRIPTEN__)
#  include "tracy_pdqsort.h"
#else
#  include <ppqsort.h>
#endif

#endif
