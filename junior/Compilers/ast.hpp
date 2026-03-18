#pragma once
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

// ---------- helpers ----------
inline void pad(std::ostream &os, int n) {
  for (int i = 0; i < n; i++)
    os << ' ';
}

struct SourceLoc {
  int line = 0;
  int col = 0;
};

// ---------- Expr ----------
struct Expr {
  virtual ~Expr() = default;
  virtual void dump(std::ostream &os, int indent = 0) const = 0;
};

struct NumberExpr : Expr {
  long long value;
  explicit NumberExpr(long long v) : value(v) {}

  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Number(" << value << ")\n";
  }
};

struct IdentExpr : Expr {
  std::string name;
  SourceLoc loc;
  explicit IdentExpr(std::string n, SourceLoc l) : name(std::move(n)), loc(l) {}

  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Ident(" << name << ") @" << loc.line << ":" << loc.col << "\n";
  }
};

struct UnaryExpr : Expr {
  std::string op;
  std::unique_ptr<Expr> rhs;
  UnaryExpr(std::string o, std::unique_ptr<Expr> r)
      : op(std::move(o)), rhs(std::move(r)) {}

  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Unary(" << op << ")\n";
    rhs->dump(os, indent + 2);
  }
};

struct BinaryExpr : Expr {
  std::string op;
  std::unique_ptr<Expr> lhs, rhs;
  BinaryExpr(std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
      : op(std::move(o)), lhs(std::move(l)), rhs(std::move(r)) {}

  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Binary(" << op << ")\n";
    lhs->dump(os, indent + 2);
    rhs->dump(os, indent + 2);
  }
};

// assignment only for IDENT = expr
struct AssignExpr : Expr {
  std::string name;
  SourceLoc loc; // location of assignment target
  std::unique_ptr<Expr> value;

  AssignExpr(std::string n, SourceLoc l, std::unique_ptr<Expr> v)
      : name(std::move(n)), loc(l), value(std::move(v)) {}

  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Assign(" << name << ") @" << loc.line << ":" << loc.col << "\n";
    value->dump(os, indent + 2);
  }
};

// ---------- Stmt ----------
struct Stmt {
  virtual ~Stmt() = default;
  virtual void dump(std::ostream &os, int indent = 0) const = 0;
};

struct VarStmt : Stmt {
  std::string name;
  SourceLoc loc; // location of declaration
  std::unique_ptr<Expr> init;

  VarStmt(std::string n, SourceLoc l, std::unique_ptr<Expr> i)
      : name(std::move(n)), loc(l), init(std::move(i)) {}

  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Var(" << name << ") @" << loc.line << ":" << loc.col << "\n";
    if (init)
      init->dump(os, indent + 2);
    else {
      pad(os, indent + 2);
      os << "(no init)\n";
    }
  }
};

struct PrintStmt : Stmt {
  std::unique_ptr<Expr> expr;
  explicit PrintStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Print\n";
    expr->dump(os, indent + 2);
  }
};

struct ExprStmt : Stmt {
  std::unique_ptr<Expr> expr;
  explicit ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "ExprStmt\n";
    expr->dump(os, indent + 2);
  }
};

struct BlockStmt : Stmt {
  std::vector<std::unique_ptr<Stmt>> stmts;
  explicit BlockStmt(std::vector<std::unique_ptr<Stmt>> s)
      : stmts(std::move(s)) {}
  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "Block\n";
    for (auto &st : stmts)
      st->dump(os, indent + 2);
  }
};

struct IfStmt : Stmt {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> thenBranch;
  std::unique_ptr<Stmt> elseBranch; // may be null
  IfStmt(std::unique_ptr<Expr> c, std::unique_ptr<Stmt> t,
         std::unique_ptr<Stmt> e)
      : cond(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {
  }
  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "If\n";
    pad(os, indent + 2);
    os << "Cond:\n";
    cond->dump(os, indent + 4);
    pad(os, indent + 2);
    os << "Then:\n";
    thenBranch->dump(os, indent + 4);
    if (elseBranch) {
      pad(os, indent + 2);
      os << "Else:\n";
      elseBranch->dump(os, indent + 4);
    }
  }
};

struct WhileStmt : Stmt {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> body;
  WhileStmt(std::unique_ptr<Expr> c, std::unique_ptr<Stmt> b)
      : cond(std::move(c)), body(std::move(b)) {}
  void dump(std::ostream &os, int indent) const override {
    pad(os, indent);
    os << "While\n";
    pad(os, indent + 2);
    os << "Cond:\n";
    cond->dump(os, indent + 4);
    pad(os, indent + 2);
    os << "Body:\n";
    body->dump(os, indent + 4);
  }
};
