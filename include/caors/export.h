// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EXPORT_H_
#define CAORS_EXPORT_H_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(CAORS_STATIC_DEFINE)
#define CAORS_API
#elif defined(CAORS_BUILDING_LIBRARY)
#define CAORS_API __declspec(dllexport)
#else
#define CAORS_API __declspec(dllimport)
#endif
#define CAORS_LOCAL
#else
#if defined(CAORS_STATIC_DEFINE)
#define CAORS_API
#elif defined(CAORS_BUILDING_LIBRARY) && (defined(__GNUC__) || defined(__clang__))
#define CAORS_API __attribute__((visibility("default")))
#else
#define CAORS_API
#endif
#if defined(__GNUC__) || defined(__clang__)
#define CAORS_LOCAL __attribute__((visibility("hidden")))
#else
#define CAORS_LOCAL
#endif
#endif

#endif
