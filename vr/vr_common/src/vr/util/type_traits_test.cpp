
#include "vr/util/type_traits.h"

#include "vr/asserts.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

char ca [3];
std::array<char, 3> a;


int32_t i { };
int32_t const i_const { };
int32_t volatile i_volatile { };

uint32_t u { };
uint32_t const u_const { };
uint32_t volatile u_volatile { };


struct streamable
{
    friend std::ostream & operator<< (std::ostream & os, const streamable &)
    {
        return os;
    }

}; // end of class

struct printable
{
    friend std::string __print__ (const printable &)
    {
        return "my print";
    }

}; // end of class


struct not_streamable_not_printable
{
}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

// redundant casts:

vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (i))>::type>::value);
vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (i_const))>::type>::value);
vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (i_volatile))>::type>::value);

vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (u))>::type>::value);
vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (u_const))>::type>::value);
vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (u_volatile))>::type>::value);

// unsigned_cast:

vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (i))>::type>::value);
vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (i_const))>::type>::value);
vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (i_volatile))>::type>::value);

//      rvalues:

vr_static_assert (std::is_unsigned<std::remove_reference<decltype (unsigned_cast (static_cast<int32_t> (1)))>::type>::value);

// signed_cast:

vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (u))>::type>::value);
vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (u_const))>::type>::value);
vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (u_volatile))>::type>::value);

//      rvalues:

vr_static_assert (std::is_signed<std::remove_reference<decltype (signed_cast (static_cast<uint32_t> (1)))>::type>::value);

//............................................................................
//............................................................................
namespace util
{

// is_... helpers:

vr_static_assert (! is_std_char_array<decltype (ca)>::value);
vr_static_assert (is_std_char_array<decltype (a)>::value);

// is_integral_or_pointer:

vr_static_assert (! is_integral_or_pointer<void>::value);
vr_static_assert (is_integral_or_pointer<void *>::value);
vr_static_assert (is_integral_or_pointer<char>::value);
vr_static_assert (is_integral_or_pointer<int32_t>::value);


// is_fundamental_or_pointer:

vr_static_assert (is_fundamental_or_pointer<void>::value);
vr_static_assert (is_fundamental_or_pointer<char>::value);
vr_static_assert (is_fundamental_or_pointer<int32_t>::value);

vr_static_assert (is_fundamental_or_pointer<void const>::value);
vr_static_assert (is_fundamental_or_pointer<char const>::value);
vr_static_assert (is_fundamental_or_pointer<int32_t const>::value);

vr_static_assert (is_fundamental_or_pointer<void const *>::value);
vr_static_assert (is_fundamental_or_pointer<char const *>::value);
vr_static_assert (is_fundamental_or_pointer<int32_t const *>::value);

vr_static_assert (is_fundamental_or_pointer<void const * const>::value);
vr_static_assert (is_fundamental_or_pointer<char const * const>::value);
vr_static_assert (is_fundamental_or_pointer<int32_t const * const>::value);

// is_streamable:

vr_static_assert (is_streamable<char, std::ostream>::value);
vr_static_assert (is_streamable<int32_t, std::ostream>::value);
vr_static_assert (is_streamable<std::string, std::ostream>::value);
vr_static_assert (is_streamable<string_literal_t, std::ostream>::value);

vr_static_assert (is_streamable<streamable, std::ostream>::value);
vr_static_assert (! (is_streamable<not_streamable_not_printable, std::ostream>::value));

// has_print:

vr_static_assert (has_print<printable>::value);
vr_static_assert (! (has_print<streamable>::value));
vr_static_assert (! (has_print<not_streamable_not_printable>::value));

//............................................................................

TEST (string_cast, arithmetic)
{
    {
        std::string const s = string_cast (123);
        EXPECT_EQ ("123", s);
    }
    {
        std::string const s = string_cast ("123");
        EXPECT_EQ ("123", s);
    }
    {
        std::string const s = string_cast ('C');
        EXPECT_EQ ("C", s);
    }
    {
        std::string const str { "asdfa" };
        std::string const & s = string_cast (str);
        EXPECT_EQ (& str, & s); // passthrough
    }
}
//............................................................................

vr_static_assert (has_empty<string_vector>::value);
vr_static_assert (! (has_empty<int32_t>::value));

vr_static_assert (is_iterable<string_vector>::value);
vr_static_assert (! (is_iterable<int32_t>::value));

//............................................................................

vr_static_assert (contains<int32_t, int8_t, int64_t, int32_t, char>::value);
vr_static_assert (contains<int32_t, int32_t>::value);

vr_static_assert (! (contains<int32_t, int8_t, int64_t, char>::value)); // 'Ts' doesn't contain 'T'
vr_static_assert (! (contains<int32_t>::value)); // empty 'Ts' list

//............................................................................

vr_static_assert (index_of<int32_t, int8_t, int64_t, int32_t, char>::value == 2);
vr_static_assert (index_of<int32_t, int32_t>::value == 0);

vr_static_assert (index_of<int32_t, int8_t, int64_t, char>::value == -1); // 'Ts' doesn't contain 'T'
vr_static_assert (index_of<int32_t>::value == -1); // empty 'Ts' list

//............................................................................

struct B { };
struct D final: public B { };
struct C { };

vr_static_assert (std::is_same<find_derived_t<B, void, D, C, B>, D>::value); // 'Ts' contain two 'B' subclasses, but 'D' is first
vr_static_assert (std::is_same<find_derived_t<B, void, C, B>, B>::value); // 'Ts' contains one 'B'
vr_static_assert (std::is_same<find_derived_t<B, void, C, C>, void>::value); // 'Ts' doesn't contain a 'B' subclass; but has duplicates

//............................................................................

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
