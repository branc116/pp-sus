#pragma once
#include "assert.h"
#include "stdlib.h"
#include "stdbool.h"

#define BR_MALLOC malloc
#define BR_FREE free
#define BR_REALLOC realloc
#define BR_ASSERT assert

#ifdef __cplusplus
#define DECLTYPE(VALUE) std::remove_reference<decltype((VALUE))>::type
extern "C" {
#else
#define DECLTYPE(VALUE) void
#endif

#define br_da_push_t(SIZE_T, ARR, VALUE) do {                                                                       \
  if ((ARR).cap == 0) {                                                                                             \
    BR_ASSERT((ARR).arr == NULL);                                                                                   \
    (ARR).arr = (DECLTYPE((VALUE))*)BR_MALLOC(sizeof(*(ARR).arr));                                                  \
    if ((ARR).arr != NULL) {                                                                                        \
      (ARR).cap = 1;                                                                                                \
      (ARR).arr[(ARR).len++] = (VALUE);                                                                             \
    }                                                                                                               \
  }                                                                                                                 \
  else if ((ARR).len < (ARR).cap) (ARR).arr[(ARR).len++] = (VALUE);                                                 \
  else {                                                                                                            \
    BR_ASSERT((ARR).arr != NULL);                                                                                   \
    SIZE_T cap_diff = (ARR).cap;                                                                                    \
    bool is_ok = false;                                                                                             \
    while (!is_ok && cap_diff > 0) {                                                                                \
      SIZE_T new_cap =  (ARR).cap + cap_diff;                                                                       \
      DECLTYPE((VALUE))* new_arr = (DECLTYPE((VALUE))*)BR_REALLOC((ARR).arr, (size_t)new_cap * sizeof(*(ARR).arr)); \
      if (new_arr) {                                                                                                \
        (ARR).arr = new_arr;                                                                                        \
        (ARR).cap = new_cap;                                                                                        \
        (ARR).arr[(ARR).len++] = ((VALUE));                                                                         \
        is_ok = true;                                                                                               \
      } else {                                                                                                      \
        cap_diff >>= 1;                                                                                             \
      }                                                                                                             \
    }                                                                                                               \
  }                                                                                                                 \
} while(0)                                                                                                          \

#define br_da_push(ARR, VALUE) br_da_push_t(size_t, ARR, VALUE)

#define br_da_free(ARR) do { \
  BR_FREE((ARR).arr);        \
  (ARR).arr = NULL;          \
  (ARR).len = (ARR).cap = 0; \
} while(0)

#define br_da_remove_n_at_t(T, ARR, N, I) do {                                    \
  assert("Index is out of bounds" && ((I) + (N) - 1) < (T)(ARR).len);             \
  T _i = (T)(I);                                                                  \
  for (; (_i + N) < (T)((ARR).len); ++_i) {                                       \
    (ARR).arr[_i] = (ARR).arr[_i + 1];                                            \
  }                                                                               \
  --(ARR).len;                                                                    \
} while(0)

#define br_da_remove_n_at(ARR, N, I) br_da_remove_n_at_t(size_t, ARR, N, I)
#define br_da_remove_at(ARR, I) br_da_remove_n_at(ARR, 1, I)
#define br_da_remove_at_t(T, ARR, I) br_da_remove_n_at_t(T, ARR, 1, I)

#define br_da_remove(ARR, V) do {                                     \
  size_t _i = 0;                                                      \
  for (; _i < (size_t)(ARR).len; ) {                                  \
    if ((ARR).arr[_i] == (V)) (ARR).arr[_i] = (ARR).arr[--(ARR).len]; \
    else ++_i;                                                        \
  }                                                                   \
} while(0)

#define br_da_contains_t(T, ARR, V, CONTAINS) do { \
  for (T _i = 0; _i < (ARR).len; ++_i) {           \
    if ((ARR).arr[_i] == V) {                      \
      CONTAINS = true;                             \
      break;                                       \
    }                                              \
  }                                                \
} while(0)

#define br_da_contains(ARR, V, CONTAINS) br_da_contains_t(size_t, ARR, V, CONTAINS)

#ifdef __cplusplus
}
#endif
  
