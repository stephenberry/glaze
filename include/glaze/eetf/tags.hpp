#pragma once

#include <cstdint>

namespace glz
{
   enum eetf_tags : uint8_t {
      SMALL_INTEGER = 'a',
      INTEGER = 'b',
      FLOAT = 'c',
      FLOAT_NEW = 'F',
      ATOM = 'd',
      SMALL_ATOM = 's',
      ATOM_UTF8 = 'v',
      SMALL_ATOM_UTF8 = 'w',
      REFERENCE = 'e',
      NEW_REFERENCE = 'r',
      NEWER_REFERENCE = 'Z',
      PORT = 'f',
      NEW_PORT = 'Y',
      PID = 'g',
      NEW_PID = 'X',
      SMALL_TUPLE = 'h',
      LARGE_TUPLE = 'i',
      NIL = 'j',
      STRING = 'k',
      LIST = 'l',
      BINARY = 'm',
      BIT_BINARY = 'M',
      SMALL_BIG = 'n',
      LARGE_BIG = 'o',
      NEW_FUN = 'p',
      MAP = 't',
      FUN = 'u',
      EXPORT = 'q',
      V4_PORT = 'x',
   };
} // namespace glz
