#include <spa/FilterExpression.h>

#define AND " AND "
#define OR " OR "
#define NOT "NOT "
#define TRUE "TRUE"
#define FALSE "FALSE"

namespace SPA {
AndFE::AndFE(FilterExpression *l, FilterExpression *r) : l(l), r(r) {
  assert(l && r);
}
bool AndFE::checkPath(Path &p) { return l->checkPath(p) && r->checkPath(p); }
std::string AndFE::dbg_str() {
  return "(" + l->dbg_str() + AND + r->dbg_str() + ")";
}

OrFE::OrFE(FilterExpression *l, FilterExpression *r) : l(l), r(r) {
  assert(l && r);
}
bool OrFE::checkPath(Path &p) { return l->checkPath(p) || r->checkPath(p); }
std::string OrFE::dbg_str() {
  return "(" + l->dbg_str() + OR + r->dbg_str() + ")";
}

NotFE::NotFE(FilterExpression *subExpr) : subExpr(subExpr) { assert(subExpr); }
bool NotFE::checkPath(Path &p) { return !subExpr->checkPath(p); }
std::string NotFE::dbg_str() { return "(" NOT + subExpr->dbg_str() + ")"; }

ConstFE::ConstFE(bool c) : c(c) {}
bool ConstFE::checkPath(Path &p) { return c; }
std::string ConstFE::dbg_str() { return c ? TRUE : FALSE; }

FilterExpression *parseParFE(std::string str);
template <class C>
FilterExpression *parseBinaryFE(std::string str, std::string op);
FilterExpression *parseNotFE(std::string str);
FilterExpression *parseConstFE(std::string str);
FilterExpression *parseReachedFE(std::string str);
FilterExpression *parseConversationFE(std::string str);

FilterExpression *FilterExpression::parse(std::string str) {
  // Trim
  str.erase(0, str.find_first_not_of(" \n\r\t"));
  str.erase(str.find_last_not_of(" \n\r\t") + 1);

  FilterExpression *result;
  if ((result = parseParFE(str)))
    return result;
  if ((result = parseBinaryFE<AndFE>(str, AND)))
    return result;
  if ((result = parseBinaryFE<OrFE>(str, OR)))
    return result;
  if ((result = parseNotFE(str)))
    return result;
  if ((result = parseConstFE(str)))
    return result;
  if ((result = parseReachedFE(str)))
    return result;
  if ((result = parseConversationFE(str)))
    return result;
  return NULL;
}

FilterExpression *parseParFE(std::string str) {
  if (str.front() == '(' && str.back() == ')') {
    return FilterExpression::parse(str.substr(1, str.length() - 2));
  } else {
    return NULL;
  }
}

template <class C>
FilterExpression *parseBinaryFE(std::string str, std::string op) {
  for (auto p = str.find(op); p != std::string::npos; p = str.find(op, p + 1)) {
    llvm::OwningPtr<FilterExpression> l(
        FilterExpression::parse(str.substr(0, p)));
    if (!l)
      continue;
    llvm::OwningPtr<FilterExpression> r(
        FilterExpression::parse(str.substr(p + op.length())));
    if (!r)
      continue;
    return new C(l.take(), r.take());
  }
  return NULL;
}

FilterExpression *parseNotFE(std::string str) {
  if (str.substr(0, strlen(NOT)) != NOT)
    return NULL;
  llvm::OwningPtr<FilterExpression> subExpr(
      FilterExpression::parse(str.substr(strlen(NOT))));
  if (!subExpr)
    return NULL;
  return new NotFE(subExpr.take());
}

FilterExpression *parseConstFE(std::string str) {
  if (str == TRUE)
    return new ConstFE(true);
  if (str == FALSE)
    return new ConstFE(false);
  return NULL;
}

FilterExpression *parseReachedFE(std::string str) {
  if (str.substr(0, strlen(REACHED)) != REACHED)
    return NULL;
  return new ReachedFE(str.substr(strlen(REACHED)));
}

FilterExpression *parseConversationFE(std::string str) {
  if (str.substr(0, strlen(CONVERSATION)) != CONVERSATION)
    return NULL;
  return new ConversationFE(str.substr(strlen(CONVERSATION)));
}
}
