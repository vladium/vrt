
#include "vr/io/json/json_object_serializer.h"
#include "vr/io/parse/JSON_tokenizer.h"
#include "vr/util/logging.h"

#include <algorithm>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
using namespace meta;

//............................................................................

struct f0       { };
struct f1       { };
struct _f2_     { };
struct _f3_     { };
struct _f4_     { };
struct _f5_     { };
struct _f6_     { };
struct _f7_     { };
struct _f8_     { };

struct _fD_     { };

VR_ENUM (fenum,
    (
        A,
        QR,
        XYZ
    ),
    printable, parsable

); // end of enum
//............................................................................
/*
 * note: this uses "compact" structs in order to be able to use non-POD fields
 */
TEST (json_object_serializer, round_trip)
{
    using base_field_seq    = make_schema_t
                            <
                                fdef_<char,             f0>,
                                fdef_<std::string,      f1>
                            >;

    using derived_field_seq = make_schema_t
                            <
                                fdef_<bool,             _f2_>,
                                fdef_<int64_t,          _f3_>,
                                fdef_<fenum,            _f4_>,
                                fdef_<std::string [2],  _f5_>
                            >;

    using base              = make_compact_struct_t<base_field_seq>;
    using derived           = make_compact_struct_t<derived_field_seq, base>;

    using aggregate_field_seq   = make_schema_t
                            <
                                fdef_<derived,                  _fD_>, // aggregate 'derived'
                                fdef_<std::array<char, 30>,     _f6_ >,
                                fdef_<std::array<base, 3>,      _f7_>, // aggregate an array of 'base'
                                fdef_<std::vector<fw_string8>,  _f8_>
                            >;
    using aggregate         = make_compact_struct_t<aggregate_field_seq>;

    {
        base b;
        {
            field<f0> (b) = 'B';
            field<f1> (b) = "a string";
        }

        std::stringstream os { };
        json_object_serializer::marshall (b, os);

        std::string const js = os.str ();
        LOG_trace1 << js;
    }
    {
        derived d;
        {
            field<f0> (d) = 'D';
            field<f1> (d) = "string\"and more";
            field<_f2_> (d) = true;
            field<_f3_> (d) = -1;
            field<_f4_> (d) = fenum::QR;
            field<_f5_> (d)[0] = "string 0";
            field<_f5_> (d)[1] = "string 1";
        }

        std::stringstream os { };
        json_object_serializer::marshall (d, os);

        std::string const js = os.str ();
        LOG_trace1 << js;
    }
    {
        aggregate a;
        {
            derived & d = field<_fD_> (a);

            field<f0> (d) = 'D';
            field<f1> (d) = "escapes:..."; // TODO
            field<_f2_> (d) = true;
            field<_f3_> (d) = -1;
            field<_f4_> (d) = fenum::XYZ;
            field<_f5_> (d)[0] = "str 0";
            field<_f5_> (d)[1] = "str 1";

            char A [] = "std::array of char";
            std::copy (std::begin (A), std::end (A), std::begin (field<_f6_> (a)));

            for (int32_t k = 0; k < 3; ++ k)
            {
                base & b = field<_f7_> (a)[k];

                field<f0> (b) = 'a' + k;
                field<f1> (b) = join_as_name ("VALUE_f1", k);
            }

            auto & fws = field<_f8_> (a);
            for (int32_t k = 0; k < 5; ++ k)
            {
                fws.emplace_back (join_as_name ("FW", k));
            }
        }

        std::stringstream os { };
        json_object_serializer::marshall (a, os);

        std::string const js = os.str ();
        LOG_trace1 << js;

        aggregate a_c { };
        json_object_serializer::unmarshall (js, a_c);

        // TODO validate that 'a' == 'a_c'
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
