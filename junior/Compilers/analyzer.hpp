#pragma once

#include "ast.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Analyzer {
public:
  struct Symbol {
    SourceLoc declLoc;
    bool used = false;
  };

  void analyze(const std::vector<std::unique_ptr<Stmt>> &program) {
    beginScope(); // global scope
    for (const auto &st : program) {
      analyzeStmt(st.get());
    }
    endScope();
  }

  int warningCount() const { return warnings_; }

private:
  std::vector<std::unordered_map<std::string, Symbol>> scopes_;
  int warnings_ = 0;

  void beginScope() { scopes_.push_back({}); }

  void endScope() {
    if (scopes_.empty())
      return;

    const auto &scope = scopes_.back();
    for (const auto &[name, sym] : scope) {
      if (!sym.used) {
        std::cerr << "Warning: unused variable '" << name << "' declared at "
                  << sym.declLoc.line << ":" << sym.declLoc.col << "\n";
        warnings_++;
      }
    }

    scopes_.pop_back();
  }

  void declareVar(const std::string &name, SourceLoc loc) {
    auto &scope = scopes_.back();

    // optional: redeclaration check in same scope
    auto it = scope.find(name);
    if (it != scope.end()) {
      std::cerr << "Error: redeclaration of variable '" << name << "' at "
                << loc.line << ":" << loc.col << " (previous declaration at "
                << it->second.declLoc.line << ":" << it->second.declLoc.col
                << ")\n";
      return;
    }

    scope.emplace(name, Symbol{loc, false});
  }

  Symbol *resolve(const std::string &name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
      auto it = scopes_[i].find(name);
      if (it != scopes_[i].end()) {
        return &it->second;
      }
    }
    return nullptr;
  }

  void markUsed(const std::string &name, SourceLoc useLoc) {
    Symbol *sym = resolve(name);
    if (!sym) {
      std::cerr << "Error: use of undeclared variable '" << name << "' at "
                << useLoc.line << ":" << useLoc.col << "\n";
      return;
    }
    sym->used = true;
  }

  void assignTo(const std::string &name, SourceLoc loc) {
    Symbol *sym = resolve(name);
    if (!sym) {
      std::cerr << "Error: assignment to undeclared variable '" << name
                << "' at " << loc.line << ":" << loc.col << "\n";
      return;
    }
    // do not mark as used: assignment is a write, not a read
  }

  void analyzeStmt(const Stmt *st) {
    if (auto *s = dynamic_cast<const VarStmt *>(st)) {
      declareVar(s->name, s->loc);
      if (s->init)
        analyzeExpr(s->init.get());
      return;
    }

    if (auto *s = dynamic_cast<const PrintStmt *>(st)) {
      analyzeExpr(s->expr.get());
      return;
    }

    if (auto *s = dynamic_cast<const ExprStmt *>(st)) {
      analyzeExpr(s->expr.get());
      return;
    }

    if (auto *s = dynamic_cast<const BlockStmt *>(st)) {
      beginScope();
      for (const auto &child : s->stmts) {
        analyzeStmt(child.get());
      }
      endScope();
      return;
    }

    if (auto *s = dynamic_cast<const IfStmt *>(st)) {
      analyzeExpr(s->cond.get());
      analyzeStmt(s->thenBranch.get());
      if (s->elseBranch)
        analyzeStmt(s->elseBranch.get());
      return;
    }

    if (auto *s = dynamic_cast<const WhileStmt *>(st)) {
      analyzeExpr(s->cond.get());
      analyzeStmt(s->body.get());
      return;
    }
  }

  void analyzeExpr(const Expr *e) {
    if (dynamic_cast<const NumberExpr *>(e)) {
      return;
    }

    if (auto *x = dynamic_cast<const IdentExpr *>(e)) {
      markUsed(x->name, x->loc);
      return;
    }

    if (auto *x = dynamic_cast<const UnaryExpr *>(e)) {
      analyzeExpr(x->rhs.get());
      return;
    }

    if (auto *x = dynamic_cast<const BinaryExpr *>(e)) {
      analyzeExpr(x->lhs.get());
      analyzeExpr(x->rhs.get());
      return;
    }

    if (auto *x = dynamic_cast<const AssignExpr *>(e)) {
      assignTo(x->name, x->loc);
      analyzeExpr(x->value.get());
      return;
    }
  }
};
