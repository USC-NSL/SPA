#include <llvm/ADT/OwningPtr.h>
#include <spa/Path.h>

namespace SPA {
class FilterExpression {
public:
  static FilterExpression *parse(std::string str);

  virtual bool check(SPA::Path *p) = 0;
  virtual std::string dbg_str() = 0;

  virtual ~FilterExpression() {}
};

class AndFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> l, r;

public:
  AndFE(FilterExpression *l, FilterExpression *r);
  bool check(SPA::Path *p);
  std::string dbg_str();
};

class OrFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> l, r;

public:
  OrFE(FilterExpression *l, FilterExpression *r);
  bool check(SPA::Path *p);
  std::string dbg_str();
};

class NotFE : public FilterExpression {
private:
  llvm::OwningPtr<FilterExpression> subExpr;

public:
  NotFE(FilterExpression *subExpr);
  bool check(SPA::Path *p);
  std::string dbg_str();
};

class ConstFE : public FilterExpression {
private:
  bool c;

public:
  ConstFE(bool c);
  bool check(SPA::Path *p);
  std::string dbg_str();
};

class ReachedFE : public FilterExpression {
private:
  std::string srcFile;
  long srcLine;
  std::string function;

public:
  ReachedFE(std::string dbgStr);
  bool check(SPA::Path *p);
  std::string dbg_str();
};
}
