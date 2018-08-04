
#include "vr/util/parse/expr_parser.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace parse
{
//............................................................................

TEST (parse_expr, precedence)
{
    {
        std::string const input { "a OR b AND c" }; // "a OR (b AND c)"

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            std::unique_ptr<expr_node> const tree { parse_expr (input) };
            ASSERT_TRUE (tree);
            ASSERT_EQ (tree->m_type, node_type::logical_expr);

            binary_logical_expr const & exp = dynamic_cast<binary_logical_expr const &> (* tree);
            EXPECT_EQ (exp.m_op, lop::OR);
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
    {
        std::string const input { "NOT a AND b" }; // "(NOT a) AND b"

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            std::unique_ptr<expr_node> const tree { parse_expr (input) };
            ASSERT_TRUE (tree);
            ASSERT_EQ (tree->m_type, node_type::logical_expr);

            binary_logical_expr const & exp = dynamic_cast<binary_logical_expr const &> (* tree);
            EXPECT_EQ (exp.m_op, lop::AND);
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
    {
        std::string const input { "NOT (a AND b)" };

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            std::unique_ptr<expr_node> const tree { parse_expr (input) };
            ASSERT_TRUE (tree);
            ASSERT_EQ (tree->m_type, node_type::logical_expr);

            unary_logical_expr const & exp = dynamic_cast<unary_logical_expr const &> (* tree);
            EXPECT_EQ (exp.m_op, lop::NOT);
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
}

TEST (parse_expr, associativity)
{
    {
        std::string const input { "a AND b AND c" }; // "(a AND b) AND c"

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            std::unique_ptr<expr_node> const tree { parse_expr (input) };
            ASSERT_TRUE (tree);
            ASSERT_EQ (tree->m_type, node_type::logical_expr);

            binary_logical_expr const & exp = dynamic_cast<binary_logical_expr const &> (* tree);
            EXPECT_EQ (exp.m_op, lop::AND);

            ASSERT_EQ (exp.m_rhs->m_type, node_type::identifier);
            identifier const & id = dynamic_cast<identifier const &> (* exp.m_rhs);
            EXPECT_EQ (id.m_image, "c");
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
    {
        std::string const input { "a AND (b AND c)" };

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            std::unique_ptr<expr_node> const tree { parse_expr (input) };
            ASSERT_TRUE (tree);
            ASSERT_EQ (tree->m_type, node_type::logical_expr);

            binary_logical_expr const & exp = dynamic_cast<binary_logical_expr const &> (* tree);
            EXPECT_EQ (exp.m_op, lop::AND);

            ASSERT_EQ (exp.m_lhs->m_type, node_type::identifier);
            identifier const & id = dynamic_cast<identifier const &> (* exp.m_lhs);
            EXPECT_EQ (id.m_image, "a");
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
}

TEST (parse_expr, invalid_input_lexer)
{
    {
        std::string const input { "a OR b AN'D c" };

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            EXPECT_THROW (parse_expr (input), parse_failure);
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
}

TEST (parse_expr, invalid_input_parser)
{
    {
        std::string const input { "a OR b AND AND c" }; // recoverable

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            EXPECT_THROW (parse_expr (input), parse_failure);
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
    {
        std::string const input { "a OR b AND" }; // not recoverable

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            EXPECT_THROW (parse_expr (input), parse_failure);
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
}
//............................................................................

TEST (parse_expr, misc)
{
    {
        std::string const input { "NOT NOT ((NOT a.1 OR b_v_2) AND NOT (_XYZ_) OR (X OR NOT Y))" }; // go nuts

        VR_IF_DEBUG (auto const expr_node_ic_start = expr_node::instance_count ();)
        {
            EXPECT_NO_THROW (parse_expr (input));
        }
        VR_IF_DEBUG ( EXPECT_EQ (expr_node::instance_count (), expr_node_ic_start); ) // verify no leaking of 'expr_node's
    }
}

} // end of 'parse'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
