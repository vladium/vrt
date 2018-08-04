#pragma once
/*
 * this is based on https://github.com/vladium/static_hash
 */
#include "vr/util/crc32.h"

#include <cstring>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{
/**
 * CRC32 uses '-1' but we'd like to avoid a large literal in the emitted asm
 */
constexpr uint32_t src_hash_seed ()     { return 1; }

} // end of anonymous
//............................................................................
//............................................................................
/**
 * a user-defined string literal for static string hashing, to be used together
 * with @ref str_hash_32() along the lines of:
 * @code
 *   char const * s = ...;
 *
 *   switch (str_hash_32 (s))
 *   {
 *      case "ABCD"_hash:  ...;
 *
 *      case "XYZ"_hash:   ...;
 *      ...
 *   }
 * @endcode
 */
constexpr uint32_t
operator "" _hash (char_const_ptr_t const str, std::size_t const len)
{
    return util::crc32_constexpr (str, len, src_hash_seed ());
}
//............................................................................
/**
 * dynamically compute the same hash value as @ref _hash()
 *
 * @param str [does not need to be 0-terminated]
 * @param len number of chars in 'str' to hash
 */
inline uint32_t
str_hash_32 (char_const_ptr_t const str, int32_t const len)
{
    return util::crc32 (reinterpret_cast<uint8_t const *> (str), len, src_hash_seed ());
}

/**
 * @param str [must be 0-terminated]
 */
inline uint32_t
str_hash_32 (string_literal_t const str)
{
    return str_hash_32 (str, std::strlen (str));
}

inline uint32_t
str_hash_32 (std::string const & str)
{
    return str_hash_32 (str.data (), str.size ());
}

} // end of namespace
//----------------------------------------------------------------------------
