/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Support/MemoryBuffer.h"

#include "../../lib/Core/Memory.h"
#include "klee/ExecutionState.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>

#include <spa/SPA.h>

#include <spa/Path.h>

namespace SPA {
	Path::Path() { }

	Path::Path( klee::ExecutionState *kState ) {
		for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::iterator it = kState->symbolics.begin(), ie = kState->symbolics.end(); it != ie; it++ ) {
			std::string name = (*it).first->name;

			symbols.insert( it->second );

			if ( name.compare( 0, strlen( SPA_TAG_PREFIX ), SPA_TAG_PREFIX ) == 0 ) {
				const klee::ObjectState *addrOS = kState->addressSpace().findObject( (*it).first );
				assert( addrOS && "Tag not set." );

				klee::ref<klee::Expr> addrExpr = addrOS->read( 0, klee::Context::get().getPointerWidth() );
				assert( isa<klee::ConstantExpr>( addrExpr ) && "Tag address is symbolic." );
				klee::ref<klee::ConstantExpr> address = cast<klee::ConstantExpr>(addrExpr);
				klee::ObjectPair op;
				assert( kState->addressSpace().resolveOne( address, op ) && "Tag address is not uniquely defined." );
				const klee::MemoryObject *mo = op.first;
				const klee::ObjectState *os = op.second;

				char *buf = new char[mo->size];
				unsigned ioffset = 0;
				klee::ref<klee::Expr> offset_expr = klee::SubExpr::create( address, op.first->getBaseExpr() );
				assert( isa<klee::ConstantExpr>( offset_expr ) && "Tag is an invalid string." );
				klee::ref<klee::ConstantExpr> value = cast<klee::ConstantExpr>( offset_expr.get() );
				ioffset = value.get()->getZExtValue();
				assert( ioffset < mo->size );

				unsigned i;
				for ( i = 0; i < mo->size - ioffset - 1; i++ ) {
					klee::ref<klee::Expr> cur = os->read8( i + ioffset );
					assert( isa<klee::ConstantExpr>( cur ) && "Symbolic character in tag value." );
					buf[i] = cast<klee::ConstantExpr>(cur)->getZExtValue( 8 );
				}
				buf[i] = 0;

				name = name.substr( strlen( SPA_TAG_PREFIX ) );
				tags[name] = std::string( buf );
// 				CLOUD9_DEBUG( "	Tag: " << name << " = " << buf );
				delete buf;
			} else {
				symbolNames[name] = it->second;

				// Symbolic value.
				if ( name.compare( 0, strlen( SPA_OUTPUT_PREFIX ), SPA_OUTPUT_PREFIX ) == 0 || name.compare( 0, strlen( SPA_STATE_PREFIX ), SPA_STATE_PREFIX ) == 0 )
					if ( const klee::ObjectState *os = kState->addressSpace().findObject( (*it).first ) )
						for ( unsigned int i = 0; i < os->size; i++ )
							symbolValues[name].push_back( os->read8( i ) );
			}
		}
		constraints = kState->constraints();
	}

	std::ostream& operator<<( std::ostream &stream, const Path &path ) {
		stream << SPA_PATH_START << std::endl;
		stream << SPA_PATH_SYMBOLS_START << std::endl;
		for ( std::map<std::string, const klee::Array *>::const_iterator it = path.symbolNames.begin(), ie = path.symbolNames.end(); it != ie; it++ )
			stream << it->first << SPA_PATH_SYMBOL_DELIMITER
				<< it->second->name << SPA_PATH_SYMBOL_DELIMITER
				<< (path.symbolValues.count( it->first ) ? SPA_PATH_SYMBOL_HASVALUE : SPA_PATH_SYMBOL_NOVALUE) << std::endl;
		stream << SPA_PATH_SYMBOLS_END << std::endl;

		stream << SPA_PATH_TAGS_START << std::endl;
		for ( std::map<std::string, std::string>::const_iterator it = path.tags.begin(), ie = path.tags.end(); it != ie; it++ )
			stream << it->first << SPA_PATH_TAG_DELIMITER << it->second << std::endl;
		stream << SPA_PATH_TAGS_END << std::endl;

		stream << SPA_PATH_KQUERY_START << std::endl;
		for ( std::set<const klee::Array *>::iterator it2 = path.symbols.begin(), ie2 = path.symbols.end(); it2 != ie2; it2++ )
			stream << "array " << (*it2)->name << "[" << (*it2)->size << "] : w32 -> w8 = symbolic" << std::endl;

		stream << SPA_PATH_KQUERY_CONSTRAINTS_START << std::endl;
		for ( klee::ConstraintManager::constraint_iterator it = path.getConstraints().begin(), ie = path.getConstraints().end(); it != ie; it++)
			stream << *it << std::endl;

		stream << SPA_PATH_KQUERY_EXPRESSIONS_START << std::endl;
		// Symbolic values.
		for ( std::map<std::string, const klee::Array *>::const_iterator it = path.symbolNames.begin(), ie = path.symbolNames.end(); it != ie; it++ ) {
			if ( path.symbolValues.count( it->first ) ) {
				for ( std::vector<klee::ref<klee::Expr> >::const_iterator it2 = path.symbolValues.find( it->first )->second.begin(), ie2 = path.symbolValues.find( it->first )->second.end(); it2 != ie2; it2++ ) {
					if ( (*it2)->getKind() == klee::Expr::Constant )
						stream << "(w8 " << *it2 << ")" << std::endl;
					else
						stream << *it2 << std::endl;
				}
			}
		}
		stream << SPA_PATH_KQUERY_EXPRESSIONS_END << std::endl;
		stream << SPA_PATH_KQUERY_END << std::endl;

		return stream << SPA_PATH_END << std::endl;
	}
}
