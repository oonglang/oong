#include "ast.h"
#include <sstream>

std::string stmtToString(const Stmt* s) {
  if (!s) return "<null>";
  if (auto p = dynamic_cast<const PrintStmt*>(s)) {
    std::ostringstream os;
    if (auto lit = dynamic_cast<LiteralExpr*>(p->expr.get())) {
      os << "Print(" << lit->value << ")";
    } else if (auto call = dynamic_cast<CallExpr*>(p->expr.get())) {
      os << "Print(" << call->callee << "()" << ")";
    } else {
      os << "Print(<expr>)";
    }
    return os.str();
  }
  return "<unknown-stmt>";
}

std::string typeToString(const Type* t) {
  if (!t) return "<null-type>";
  if (auto n = dynamic_cast<const NamedType*>(t)) {
    return n->Name;
  }
  if (auto g = dynamic_cast<const GenericType*>(t)) {
    std::string out = "";
    if (g->Base) out += typeToString(g->Base.get());
    out += "<";
    for (size_t i = 0; i < g->Args.size(); ++i) {
      if (i) out += ",";
      out += typeToString(g->Args[i].get());
    }
    out += ">";
    return out;
  }
  if (auto a = dynamic_cast<const ArrayType*>(t)) {
    return typeToString(a->Element.get()) + "[]";
  }
  if (auto u = dynamic_cast<const UnionType*>(t)) {
    std::string out;
    for (size_t i = 0; i < u->Options.size(); ++i) {
      if (i) out += "|";
      out += typeToString(u->Options[i].get());
    }
    return out;
  }
  if (auto it = dynamic_cast<const IntersectionType*>(t)) {
    std::string out;
    for (size_t i = 0; i < it->Parts.size(); ++i) {
      if (i) out += "&";
      out += typeToString(it->Parts[i].get());
    }
    return out;
  }
  if (auto r = dynamic_cast<const RawType*>(t)) {
    return "raw(" + r->Raw + ")";
  }
  return "<unknown-type>";
}
