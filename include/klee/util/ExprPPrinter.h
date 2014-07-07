//===-- ExprPPrinter.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXPRPPRINTER_H
#define KLEE_EXPRPPRINTER_H

#include <map>

#include "klee/Expr.h"
#include "klee/util/PrintContext.h"

namespace llvm {
  class raw_ostream;
}
namespace klee {
  class ConstraintManager;

  class ExprPPrinter {
  protected:
    ExprPPrinter() {}
    
  public:
    static ExprPPrinter *create(llvm::raw_ostream &os);

    virtual ~ExprPPrinter() {}

    virtual void setNewline(const std::string &newline) = 0;
    virtual void setForceNoLineBreaks(bool forceNoLineBreaks) = 0;
    virtual void reset() = 0;
    virtual void scan(const ref<Expr> &e) = 0;
    virtual void print(const ref<Expr> &e, unsigned indent=0) = 0;

    // utility methods

    template<class Container>
    void scan(Container c) {
      scan(c.begin(), c.end());
    }

    template<class InputIterator>
    void scan(InputIterator it, InputIterator end) {
      for (; it!=end; ++it)
        scan(*it);
    }

    /// printOne - Pretty print a single expression prefixed by a
    /// message and followed by a line break.
    static void printOne(llvm::raw_ostream &os, const char *message,
                         const ref<Expr> &e);

    /// printSingleExpr - Pretty print a single expression.
    ///
    /// The expression will not be followed by a line break.
    ///
    /// Note that if the output stream is not positioned at the
    /// beginning of a line then printing will not resume at the
    /// correct position following any output line breaks.
    static void printSingleExpr(llvm::raw_ostream &os, const ref<Expr> &e);

    static void printConstraints(llvm::raw_ostream &os,
                                 const ConstraintManager &constraints);

    static void printQuery(llvm::raw_ostream &os,
                           const ConstraintManager &constraints,
                           const ref<Expr> &q,
                           const ref<Expr> *evalExprsBegin = 0,
                           const ref<Expr> *evalExprsEnd = 0,
                           const Array * const* evalArraysBegin = 0,
                           const Array * const* evalArraysEnd = 0,
                           bool printArrayDecls = true);
  };

  class PPrinter : public ExprPPrinter {
  public:
    std::set<const Array*> usedArrays;
  private:
    std::map<ref<Expr>, unsigned> bindings;
    std::map<const UpdateNode*, unsigned> updateBindings;
    std::set< ref<Expr> > couldPrint, shouldPrint;
    std::set<const UpdateNode*> couldPrintUpdates, shouldPrintUpdates;
    llvm::raw_ostream &os;
    unsigned counter;
    unsigned updateCounter;
    bool hasScan;
    bool forceNoLineBreaks;
    std::string newline;

    bool shouldPrintWidth(ref<Expr> e);
    bool isVerySimple(const ref<Expr> &e);
    bool isVerySimpleUpdate(const UpdateNode *un);
    bool isSimple(const ref<Expr> &e);
    bool hasSimpleKids(const Expr *ep);
    void scanUpdate(const UpdateNode *un);
    void scan1(const ref<Expr> &e);
    void printUpdateList(const UpdateList &updates, PrintContext &PC);
    void printWidth(PrintContext &PC, ref<Expr> e);
    bool isReadExprAtOffset(ref<Expr> e, const ReadExpr *base, ref<Expr> offset);
    const ReadExpr* hasOrderedReads(ref<Expr> e, int stride);
    void printRead(const ReadExpr *re, PrintContext &PC, unsigned indent);
    void printExtract(const ExtractExpr *ee, PrintContext &PC, unsigned indent);
    void printExpr(const Expr *ep, PrintContext &PC, unsigned indent, bool printConstWidth=false);

  public:
    PPrinter(llvm::raw_ostream &_os);
    void setNewline(const std::string &_newline);
    void setForceNoLineBreaks(bool _forceNoLineBreaks);
    void reset();
    void scan(const ref<Expr> &e);
    void print(const ref<Expr> &e, unsigned level=0);
    void printConst(const ref<ConstantExpr> &e, PrintContext &PC, 
                    bool printWidth);
    void print(const ref<Expr> &e, PrintContext &PC, bool printConstWidth=false);
    void printSeparator(PrintContext &PC, bool simple, unsigned indent);
  };
}

#endif
