#ifndef VAR_RENAME_VISITOR_HPP
#define VAR_RENAME_VISITOR_HPP

#include <string>

#include "ast/ast.hpp"
#include "symtab/symbol_table.hpp"
#include "visitors/ast_visitor.hpp"

/**
 * \class VarRenameVisitor
 * \brief "Blindly" rename given variable to new name
 *
 * During inlining related passes we have to rename variables
 * to avoid name conflicts. This pass "blindly" rename any given
 * variable to new name. The error handling / legality checks are
 * supposed to be done by other higher level passes. For example,
 * local renaming pass should be done from inner-most block to top
 * level block;
 *
 * \todo : Add log/warning messages.
 */

class RenameVisitor : public AstVisitor {
  private:
    /// variable to rename
    std::string var_name;

    /// new name
    std::string new_var_name;

    // rename verbatim blocks as well
    bool rename_verbatim = true;

  public:
    RenameVisitor() = default;

    RenameVisitor(std::string old_name, std::string new_name)
        : var_name(old_name), new_var_name(new_name) {
    }

    void set(std::string old_name, std::string new_name) {
        var_name = old_name;
        new_var_name = new_name;
    }

    void enable_verbatim(bool state) {
        rename_verbatim = state;
    }

    virtual void visit_name(ast::Name* node) override;
    virtual void visit_prime_name(ast::PrimeName* node) override;
    virtual void visit_verbatim(ast::Verbatim* node) override;
};

#endif
