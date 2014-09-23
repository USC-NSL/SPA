/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATH_H__
#define __PATH_H__

#include <map>

#include <klee/Constraints.h>
#include "klee/ExecutionState.h"
#include <klee/Solver.h>

#define SPA_PATH_START            "--- PATH START ---"
#define SPA_PATH_SYMBOLS_START    "--- SYMBOLS START ---"
#define SPA_PATH_SYMBOL_DELIMITER "	"
#define SPA_PATH_SYMBOLS_END      "--- SYMBOLS END ---"
#define SPA_PATH_TAGS_START       "--- TAGS START ---"
#define SPA_PATH_TAG_DELIMITER    "	"
#define SPA_PATH_TAGS_END         "--- TAGS END ---"
#define SPA_PATH_KQUERY_START     "--- KQUERY START ---"
#define SPA_PATH_KQUERY_END       "--- KQUERY END ---"
#define SPA_PATH_TESTCASE_START   "--- TESTCASE START ---"
#define SPA_PATH_TESTCASE_END     "--- TESTCASE END ---"
#define SPA_PATH_END              "--- PATH END ---"
#define SPA_PATH_COMMENT          "#"
#define SPA_PATH_WHITE_SPACE      " 	\r\n"

namespace SPA {
	class Path {
	private:
		std::set<const klee::Array *> symbols;
		std::map<std::string, const klee::Array *> symbolNames;
		std::map<std::string, std::vector<klee::ref<klee::Expr> > > outputValues;
		std::map<std::string, std::string> tags;
		klee::ConstraintManager constraints;
    std::map<std::string, std::vector<uint8_t>> testCase;

		Path();

	public:
    Path(klee::ExecutionState *kState, klee::Solver *solver);

		const klee::Array *getSymbol( std::string name ) const {
			return symbolNames.count( name ) ? symbolNames.find( name )->second : NULL;
		}

		bool hasOutput( std::string name ) const {
			return outputValues.count( name ) > 0;
		}

		size_t getOutputSize( std::string name ) const {
			return outputValues.count( name ) ? outputValues.find( name )->second.size() : 0;
		}

		const klee::ref<klee::Expr> getOutputValue( std::string name, unsigned int offset ) const {
			assert( offset < getOutputSize( name ) && "Symbol offset out of bounds." );
			return outputValues.find( name )->second[offset];
		}

		std::map<std::string, const klee::Array *>::const_iterator beginSymbols() const { return symbolNames.begin(); }

		std::map<std::string, const klee::Array *>::const_iterator endSymbols() const { return symbolNames.end(); }

		std::map<std::string, std::vector<klee::ref<klee::Expr> > >::const_iterator beginOutputs() { return outputValues.begin(); }

		std::map<std::string, std::vector<klee::ref<klee::Expr> > >::const_iterator endOutputs() { return outputValues.end(); }

		std::string getTag( std::string key ) const {
			return tags.count( key ) ? tags.find( key )->second : std::string();
		}

		const klee::ConstraintManager &getConstraints() const {
			return constraints;
		}

		const std::map<std::string, std::vector<uint8_t>> &getTestCase() const {
      return testCase;
    }

    friend class PathLoader;
		friend std::ostream& operator<<( std::ostream &stream, const Path &path );
	};

	std::ostream& operator<<( std::ostream &stream, const Path &path );
}

#endif // #ifndef __PATH_H__
