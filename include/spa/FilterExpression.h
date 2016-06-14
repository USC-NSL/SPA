#include <llvm/ADT/OwningPtr.h>
#include <spa/Path.h>
#include <spa/PathFilter.h>

#define REACHED "REACHED "
#define CONVERSATION "CONVERSATION "

namespace SPA {
class FilterExpression : public PathFilter {
public:
  static FilterExpression *parse(std::string str);

  virtual bool checkPath(Path &p) = 0;
  virtual std::string dbg_str() = 0;

  virtual ~FilterExpression() {}
};

class AndFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> l, r;

public:
  AndFE(FilterExpression *l, FilterExpression *r);
  bool checkPath(Path &p);
  std::string dbg_str();
};

class OrFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> l, r;

public:
  OrFE(FilterExpression *l, FilterExpression *r);
  bool checkPath(Path &p);
  std::string dbg_str();
};

class NotFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> subExpr;

public:
  NotFE(FilterExpression *subExpr);
  bool checkPath(Path &p);
  std::string dbg_str();
};

class ConstFE : public FilterExpression {
private:
  bool c;

public:
  ConstFE(bool c);
  bool checkPath(Path &p);
  std::string dbg_str();
};

class ReachedFE : public FilterExpression {
private:
  std::string dbgStr;

public:
  ReachedFE(std::string dbgStr) : dbgStr(dbgStr) {}
  bool checkPath(Path &p) { return p.isCovered(dbgStr); }
  std::string dbg_str() { return std::string("(" REACHED) + dbgStr + ")"; }
};

class ConversationFE : public FilterExpression {
private:
  std::string conversation;

public:
  ConversationFE(std::string conversation) : conversation(conversation) {}
  bool checkPath(Path &p) {
    std::string c;
    std::string pathUUID = "";
    for (auto it : p.getSymbolLog()) {
      if (it->getPathUUID() != pathUUID) {
        c += " " + it->getParticipant();
        pathUUID = it->getPathUUID();
      }
    }
    if (!c.empty()) {
      c = c.substr(1);
    }

    return conversation.compare(0, c.length(), c) == 0;
  }
  std::string dbg_str() {
    return std::string("(" CONVERSATION) + conversation + ")";
  }
};
}
