#pragma once

#include "vr/containers/util/stable_vector.h"
#include "vr/market/sources/asx/defs.h"
#include "vr/strings.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace itch {
class order_book_dir; // forward
class combo_order_book_dir; // forward
}

/**
 * this is the universe
 */
class itch_ref_data final: noncopyable
{
    public: // ...............................................................

        struct entry final
        {
            entry (std::string && symbol, std::string && name, iid_t const iid, ITCH_product::enum_t const product, int32_t const pix) :
                m_symbol { std::move (symbol) },
                m_name { std::move (name) },
                m_iid { iid },
                m_product { product },
                m_pix { pix },
                m_qty_traded { },
                m_book_state { }
            {
            }

            std::string const m_symbol;
            std::string const m_name;
            iid_t const m_iid;
            ITCH_product::enum_t const m_product;
            int32_t const m_pix; // [negative if unknown] partition index

            friend std::ostream & operator<< (std::ostream & os, entry const & e) VR_NOEXCEPT;


            // HACK these don't belong here:

            std::string & book_state () const
            {
                return m_book_state;
            }

            int64_t & qty_traded () const
            {
                return m_qty_traded;
            }

            private:

                mutable int64_t m_qty_traded;
                mutable std::string m_book_state;

        }; // end of nested class


        // ACCESSORs:

        int32_t size () const
        {
            return m_iid_map.size ();
        }

        VR_FORCEINLINE bool contains (iid_t const & iid) const
        {
            return m_iid_map.count (iid);
        }

        VR_ASSUME_HOT entry const & operator[] (std::string const & symbol) const;
        VR_ASSUME_HOT entry const & operator[] (iid_t const & iid) const;

        int32_t list_symbols (name_filter const & nf, std::ostream & out) const;
        int32_t list_names (name_filter const & nf, std::ostream & out) const;

        // MUTATORs:

        void add (itch::order_book_dir const & def, int32_t const pix);
        void add (itch::combo_order_book_dir const & def, int32_t const pix);

    private: // ..............................................................

        using entry_vector      = util::stable_vector<entry>;
        using symbol_map        = boost::unordered_map<std::string, entry *>;
        using iid_map           = boost::unordered_map<iid_t, entry *>;


        entry_vector m_entries { };
        iid_map m_iid_map { }; // references entries in 'm_entries'
        symbol_map m_symbol_map { }; // references entries in 'm_entries'

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
