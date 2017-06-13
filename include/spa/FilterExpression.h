#include <llvm/ADT/OwningPtr.h>
#include <spa/Path.h>
#include <spa/PathFilter.h>
#include <spa/Util.h>

#define REACHED "REACHED "
#define CONVERSATION "CONVERSATION "
#define UUID "UUID "
#define INTERLEAVINGS "INTERLEAVINGS "

#define BOOL_OP_AND " AND "
#define BOOL_OP_OR " OR "
#define BOOL_OP_NOT "NOT "
#define BOOL_OP_TRUE "TRUE"
#define BOOL_OP_FALSE "FALSE"

#define COMPARE_OP_LT "<"
#define COMPARE_OP_LE "<="
#define COMPARE_OP_GT ">"
#define COMPARE_OP_GE ">="
#define COMPARE_OP_EQ "=="
#define COMPARE_OP_NE "!="

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
  AndFE(FilterExpression *l, FilterExpression *r) : l(l), r(r) {
    assert(l && r);
  }
  bool checkPath(Path &p) { return l->checkPath(p) && r->checkPath(p); }
  std::string dbg_str() {
    return "((" + l->dbg_str() + ")" + BOOL_OP_AND + "(" + r->dbg_str() + "))";
  }
};

class OrFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> l, r;

public:
  OrFE(FilterExpression *l, FilterExpression *r) : l(l), r(r) {
    assert(l && r);
  }
  bool checkPath(Path &p) { return l->checkPath(p) || r->checkPath(p); }
  std::string dbg_str() {
    return "((" + l->dbg_str() + ")" + BOOL_OP_OR + "(" + r->dbg_str() + "))";
  }
};

class NotFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> subExpr;

public:
  NotFE(FilterExpression *subExpr) : subExpr(subExpr) { assert(subExpr); }
  bool checkPath(Path &p) { return !subExpr->checkPath(p); }
  std::string dbg_str() {
    return "(" BOOL_OP_NOT "(" + subExpr->dbg_str() + "))";
  }
};

class ConstFE : public FilterExpression {
private:
  bool c;

public:
  ConstFE(bool c) : c(c) {}
  bool checkPath(Path &p) { return c; }
  std::string dbg_str() { return c ? BOOL_OP_TRUE : BOOL_OP_FALSE; }
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

class UUIDFE : public FilterExpression {
private:
  std::string uuid;

public:
  UUIDFE(std::string uuid) : uuid(uuid) {}
  bool checkPath(Path &p) { return p.getUUID() == uuid; }
  std::string dbg_str() { return std::string("(" UUID) + uuid + ")"; }
};

class InterleavingsFE : public FilterExpression {
private:
  std::string op;
  unsigned int metric;

public:
  InterleavingsFE(std::string op, unsigned int metric)
      : op(op), metric(metric) {}
  bool checkPath(Path &p) {
    unsigned int interleavings = 0;
    std::string participant = "";
    for (auto it : p.getSymbolLog()) {
      if (it->getParticipant() != participant) {
        interleavings++;
        participant = it->getParticipant();
      }
    }

    if (op == COMPARE_OP_LT) {
      return interleavings < metric;
    } else if (op == COMPARE_OP_LE) {
      return interleavings <= metric;
    } else if (op == COMPARE_OP_GT) {
      return interleavings > metric;
    } else if (op == COMPARE_OP_GE) {
      return interleavings >= metric;
    } else if (op == COMPARE_OP_EQ) {
      return interleavings == metric;
    } else if (op == COMPARE_OP_NE) {
      return interleavings != metric;
    } else {
      assert(false && "Bad comparison operator.");
    }
  }
  std::string dbg_str() {
    return std::string("(" INTERLEAVINGS) + op + " " + numToStr(metric) + ")";
  }
};
}
