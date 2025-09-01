#include "ast.h"
#include <sstream>

std::string stmtToString(const Stmt* s) {
  if (!s) return "<null>";
  if (auto p = dynamic_cast<const PrintStmt*>(s)) {
    std::ostringstream os;
    os << "Print(" << p->Value << ")";
    return os.str();
  }
  return "<unknown-stmt>";
}
