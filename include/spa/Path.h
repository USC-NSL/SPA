/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATH_H__
#define __PATH_H__

#include <map>

#include <klee/Constraints.h>
#include "klee/ExecutionState.h"

#define SPA_PATH_START						"--- PATH START ---"
#define SPA_PATH_SYMBOL_START				"--- SYMBOL START ---"
#define SPA_PATH_KQUERY_EXPRESSIONS_START	"(query [] false ["
#define SPA_PATH_KQUERY_EXPRESSIONS_END		"])"
#define SPA_PATH_SYMBOL_END					"--- SYMBOL END ---"
#define SPA_PATH_TAG_START					"--- TAG START ---"
#define SPA_PATH_TAG_END					"--- TAG END ---"
#define SPA_PATH_CONSTRAINTS_START			"--- CONSTRAINTS START ---"
#define SPA_PATH_KQUERY_CONSTRAINTS_START	"(query ["
#define SPA_PATH_KQUERY_CONSTRAINTS_END		"] false)"
#define SPA_PATH_CONSTRAINTS_END			"--- CONSTRAINTS END ---"
#define SPA_PATH_END						"--- PATH END ---"
#define SPA_PATH_COMMENT					"#"
#define SPA_PATH_WHITE_SPACE				" 	\r\n"

#define SPA_INPUT_PREFIX		"spa_input_"
#define SPA_OUTPUT_PREFIX		"spa_output_"
#define SPA_TAG_PREFIX			"spa_tag_"

namespace SPA {
	class Path {
	private:
		std::set<const klee::Array *> symbols;
		std::map<std::string, const klee::Array *> symbolNames;
		std::map<std::string, std::vector<klee::ref<klee::Expr> > > symbolValues;
		std::map<std::string, std::string> tags;
		klee::ConstraintManager constraints;

		Path();

	public:
		Path( klee::ExecutionState *kState );

		const klee::Array *getSymbol( std::string name ) const {
			return symbolNames.count( name ) ? symbolNames.find( name )->second : NULL;
		}

		int getSymbolValueSize( std::string name ) const {
			return symbolValues.count( name ) ? symbolValues.find( name )->second.size() : 0;
		}

		const klee::ref<klee::Expr> getSymbolValue( std::string name, int offset ) const {
			assert( offset >=0 && offset < getSymbolValueSize( name ) && "Symbol offset out of bounds." );
			return symbolValues.find( name )->second[offset];
		}

		std::string getTag( std::string key ) const {
			return tags.count( key ) ? tags.find( key )->second : std::string();
		}
		
		const klee::ConstraintManager &getConstraints() const {
			return constraints;
		}

		friend class PathLoader;
		friend std::ostream& operator<<( std::ostream &stream, const Path &path );
	};

	std::ostream& operator<<( std::ostream &stream, const Path &path );
}

#endif // #ifndef __PATH_H__
