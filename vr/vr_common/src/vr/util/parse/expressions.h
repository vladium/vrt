#pragma once

#include "vr/operators.h"
#if VR_DEBUG
#   include "vr/utility.h" // instance_counter
#endif
#include "vr/util/memory.h"
#include "vr/util/str_range.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace parse
{
//............................................................................

using expr_token        = util::str_range;

VR_ENUM (node_type,
    (
        identifier,
        logical_expr
    ),
    printable

); // end of enum
//............................................................................

struct expr_node VR_IF_DEBUG (: public instance_counter<expr_node>)
{
    expr_node (node_type::enum_t const type) :
        m_type { type }
    {
    }

    virtual ~expr_node ()   = default;

    node_type::enum_t const m_type;

}; // end of class

struct logical_expr: public expr_node
{
    logical_expr (lop::enum_t const op, expr_node * const expr) :
        expr_node (node_type::logical_expr),
        m_rhs { expr }, // grab ownership
        m_op { op }
    {
    }

    std::unique_ptr<expr_node> const m_rhs;
    lop::enum_t const m_op;

}; // end of class
//............................................................................

struct identifier final: public expr_node
{
    identifier (expr_token const & token) :
        expr_node (node_type::identifier),
        m_image { token.to_string () } // 'token' still references the input string, so here we finally make a copy
    {
    }

    std::string const m_image;

}; // end of class

struct unary_logical_expr final: public logical_expr
{
    unary_logical_expr (lop::enum_t const op, expr_node * const rhs) :
        logical_expr (op, rhs)
    {
    }

}; // end of class

struct binary_logical_expr final: public logical_expr
{
    binary_logical_expr (expr_node * const lhs, lop::enum_t const op, expr_node * const rhs) :
        logical_expr (op, rhs),
        m_lhs { lhs } // grab ownership
    {
    }

    std::unique_ptr<expr_node> const m_lhs;

}; // end of class

} // end of 'parse'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
