// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Glaze specific macros

#define GLZ_X(a) \
   static_assert(false, "GLZ_X macro has been removed with the addition of member object and pure reflection")
#define GLZ_QUOTED_X(a) \
   static_assert(false, "GLZ_QUOTED_X macro has been removed with the addition of member object and pure reflection")
#define GLZ_QUOTED_NUM_X(a) \
   static_assert(false,     \
                 "GLZ_QUOTED_NUM_X macro has been removed with the addition of member object and pure reflection")

#define GLZ_META(C, ...) \
   static_assert(false, "GLZ_META macro has been removed with the addition of member object and pure reflection")

#define GLZ_LOCAL_META(C, ...)                                                                        \
   static_assert(false,                                                                               \
                 "GLZ_LOCAL_META macro has been removed with the addition of member object and pure " \
                 "reflection")
