
#include "vr/util/hashing.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (hash, supported_input_types)
{
    // integral key types:
    {
        using key_type  = int32_t;
        {
            using map_type      = boost::unordered_map<key_type, int64_t, identity_hash<key_type> >;

            map_type m VR_UNUSED { }; // instantiate to trigger full template expansion
        }
        {
            using map_type       = boost::unordered_map<key_type, int64_t, crc32_hash<key_type> >;

            map_type m VR_UNUSED { }; // instantiate to trigger full template expansion
        }
    }
    // pointer types:
    {
        using key_type  = std::string *;
        {
            using map_type      = boost::unordered_map<key_type, int64_t, identity_hash<key_type> >;

            map_type m VR_UNUSED { }; // instantiate to trigger full template expansion
        }
        {
            using map_type       = boost::unordered_map<key_type, int64_t, crc32_hash<key_type> >;

            map_type m VR_UNUSED { }; // instantiate to trigger full template expansion
        }
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
