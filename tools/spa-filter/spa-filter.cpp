#include <fstream>

#include <llvm/ADT/OwningPtr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include <spa/Path.h>
#include <spa/DbgLineIF.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#define AND " AND "
#define OR " OR "
#define TRUE "TRUE"
#define FALSE "FALSE"

namespace {
llvm::cl::opt<bool> EnableDbg("d", llvm::cl::init(false),
                              llvm::cl::desc("Output debug information."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));

llvm::cl::opt<std::string> OutFileName(llvm::cl::Positional, llvm::cl::Required,
                                       llvm::cl::desc("<output path-file>"));

llvm::cl::opt<std::string> Criteria(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<matching criteria>"));

class Expression {
public:
  virtual bool check(SPA::Path *p) = 0;
  virtual std::string dbg_str() = 0;

  virtual ~Expression() {}
  ;
};

class AndExpr : public Expression {
private:
  llvm::OwningPtr<Expression> l, r;

public:
  AndExpr(Expression *l, Expression *r) : l(l), r(r) { assert(l && r); }
  bool check(SPA::Path *p) { return l->check(p) && r->check(p); }
  std::string dbg_str() {
    return "(" + l->dbg_str() + " AND " + r->dbg_str() + ")";
  }
};

class OrExpr : public Expression {
private:
  llvm::OwningPtr<Expression> l, r;

public:
  OrExpr(Expression *l, Expression *r) : l(l), r(r) { assert(l && r); }
  bool check(SPA::Path *p) { return l->check(p) || r->check(p); }
  std::string dbg_str() {
    return "(" + l->dbg_str() + " OR " + r->dbg_str() + ")";
  }
};

class NotExpr : public Expression {
private:
  llvm::OwningPtr<Expression> subExpr;

public:
  NotExpr(Expression *subExpr) : subExpr(subExpr) { assert(subExpr); }
  bool check(SPA::Path *p) { return !subExpr->check(p); }
  std::string dbg_str() { return "(NOT " + subExpr->dbg_str() + ")"; }
};

class ConstExpr : public Expression {
private:
  bool c;

public:
  ConstExpr(bool c) : c(c) {}
  bool check(SPA::Path *p) { return c; }
  std::string dbg_str() { return c ? TRUE : FALSE; }
};

class ReachedExpr : public Expression {
private:
  std::string dbgStr;
  llvm::OwningPtr<SPA::DbgLineIF> dbgIF;

public:
  ReachedExpr(std::string dbgStr) : dbgStr(dbgStr) {
    assert(false && "Not implemented.");
    //         dbgIF(new SPA::DbgLineIF(module, dbgStr))
  }
  bool check(SPA::Path *p) { assert(false && "Not implemented."); }
  std::string dbg_str() { return "(REACHED " + dbgStr + ")"; }
};

Expression *parseParExpr(std::string str);
template <class C> Expression *parseBinaryExpr(std::string str, std::string op);
Expression *parseNotExpr(std::string str);
Expression *parseConstExpr(std::string str);
Expression *parseReachedExpr(std::string str);

Expression *parseExpression(std::string str) {
  // Trim
  str.erase(0, str.find_first_not_of(" \n\r\t"));
  str.erase(str.find_last_not_of(" \n\r\t") + 1);

  Expression *result;
  if ((result = parseParExpr(str)))
    return result;
  if ((result = parseBinaryExpr<AndExpr>(str, AND)))
    return result;
  if ((result = parseBinaryExpr<OrExpr>(str, OR)))
    return result;
  if ((result = parseNotExpr(str)))
    return result;
  if ((result = parseConstExpr(str)))
    return result;
  if ((result = parseReachedExpr(str)))
    return result;
  return NULL;
}

Expression *parseParExpr(std::string str) {
  if (str.front() == '(' && str.back() == ')') {
    return parseExpression(str.substr(1, str.length() - 2));
  } else {
    return NULL;
  }
}

template <class C>
Expression *parseBinaryExpr(std::string str, std::string op) {
  for (auto p = str.find(op); p != std::string::npos; p = str.find(op, p + 1)) {
    llvm::OwningPtr<Expression> l(parseExpression(str.substr(0, p)));
    if (!l)
      continue;
    llvm::OwningPtr<Expression> r(parseExpression(str.substr(p + op.length())));
    if (!r)
      continue;
    return new C(l.take(), r.take());
  }
  return NULL;
}

#define NOT "NOT "
Expression *parseNotExpr(std::string str) {
  if (str.substr(0, strlen(NOT)) != NOT)
    return NULL;
  llvm::OwningPtr<Expression> subExpr(parseExpression(str.substr(strlen(NOT))));
  if (!subExpr)
    return NULL;
  return new NotExpr(subExpr.take());
}

Expression *parseConstExpr(std::string str) {
  if (str == TRUE)
    return new ConstExpr(true);
  if (str == FALSE)
    return new ConstExpr(false);
  return NULL;
}

#define REACHED "REACHED "
Expression *parseReachedExpr(std::string str) {
  if (str.substr(0, strlen(REACHED)) != REACHED)
    return NULL;
  if (!SPA::DbgLineIF::checkSyntax(str.substr(strlen(REACHED))))
    return NULL;
  return new ReachedExpr(str);
}
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Filter");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  std::ifstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  llvm::outs() << "Parsing criteria.\n";
  llvm::OwningPtr<Expression> expr(parseExpression(Criteria));
  assert(expr);

  if (EnableDbg) {
    llvm::outs() << "Expression parsed as: " << expr->dbg_str() << '\n';
  }

  return 0;
}
