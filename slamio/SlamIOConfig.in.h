//
// Project configuration header. This is a generated header; do not modify
// it directly. Instead, modify the config.h.in version and run CMake again.
//
#ifndef SLAMIO_SLAMIOCONFIG_H_
#define SLAMIO_SLAMIOCONFIG_H_

#include "SlamIOExport.h"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif  // _USE_MATH_DEFINES
#ifndef NOMINMAX
#define NOMINMAX
#endif  // NOMINMAX

#ifdef _MSC_VER
// Avoid dubious security warnings for plenty of legitimate code
#ifndef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif  // _SCL_SECURE_NO_WARNINGS
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif  // _CRT_SECURE_NO_WARNINGS
//#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif  // _MSC_VER

#include <cmath>

#include <memory>

/// Enable experimental parts of GLM, like `glm::length2()` (length squared)
#define GLM_ENABLE_EXPERIMENTAL

// clang-format on

#cmakedefine01 SLAMIO_HAVE_PDAL
#cmakedefine01 SLAMIO_HAVE_PDAL_STREAMS

#endif  // SLAMIO_SLAMIOCONFIG_H_
