/* Single translation unit for the C model-loading libraries. */
#ifdef _MSC_VER
#pragma warning(push, 0)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "ufbx.c"

#ifdef _MSC_VER
#pragma warning(pop)
#endif
