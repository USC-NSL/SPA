#include <spa/FilterExpression.h>
#include <spa/Util.h>

namespace SPA {
FilterExpression *parseParFE(std::string str);
template <class C>
FilterExpression *parseBinaryFE(std::string str, std::string op);
FilterExpression *parseNotFE(std::string str);
FilterExpression *parseConstFE(std::string str);
FilterExpression *parseReachedFE(std::string str);
FilterExpression *parseConversationFE(std::string str);
FilterExpression *parseUUIDFE(std::string str);
FilterExpression *parseInterleavingsFE(std::string str);

FilterExpression *FilterExpression::parse(std::string str) {
  // Trim
  str.erase(0, str.find_first_not_of(" \n\r\t"));
  str.erase(str.find_last_not_of(" \n\r\t") + 1);

  FilterExpression *result;
  if ((result = parseParFE(str)))
    return result;
  if ((result = parseBinaryFE<AndFE>(str, BOOL_OP_AND)))
    return result;
  if ((result = parseBinaryFE<OrFE>(str, BOOL_OP_OR)))
    return result;
  if ((result = parseNotFE(str)))
    return result;
  if ((result = parseConstFE(str)))
    return result;
  if ((result = parseReachedFE(str)))
    return result;
  if ((result = parseConversationFE(str)))
    return result;
  if ((result = parseUUIDFE(str)))
    return result;
  if ((result = parseInterleavingsFE(str)))
    return result;
  return NULL;
}

FilterExpression *parseParFE(std::string str) {
  if (str.front() == '(' && str.back() == ')') {
    // Check if parenthesis are balanced.
    int count = 0;
    for (char it : str) {
      if (it == '(') {
        count++;
      } else if (it == ')') {
        count--;
      }
    }

    if (count == 0) {
      return FilterExpression::parse(str.substr(1, str.length() - 2));
    } else {
      return NULL;
    }
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
  if (str.substr(0, strlen(BOOL_OP_NOT)) != BOOL_OP_NOT)
    return NULL;
  llvm::OwningPtr<FilterExpression> subExpr(
      FilterExpression::parse(str.substr(strlen(BOOL_OP_NOT))));
  if (!subExpr)
    return NULL;
  return new NotFE(subExpr.take());
}

FilterExpression *parseConstFE(std::string str) {
  if (str == BOOL_OP_TRUE)
    return new ConstFE(true);
  if (str == BOOL_OP_FALSE)
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

FilterExpression *parseUUIDFE(std::string str) {
  if (str.substr(0, strlen(UUID)) != UUID)
    return NULL;
  return new UUIDFE(str.substr(strlen(UUID)));
}

FilterExpression *parseInterleavingsFE(std::string str) {
  if (str.substr(0, strlen(INTERLEAVINGS)) != INTERLEAVINGS)
    return NULL;
  str = str.substr(strlen(INTERLEAVINGS));
  auto delim = str.find(" ");
  std::string op = str.substr(0, delim);
  unsigned int metric = strToNum<unsigned int>(str.substr(delim + 1));
  return new InterleavingsFE(op, metric);
}
}
