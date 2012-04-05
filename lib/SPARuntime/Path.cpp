/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Support/MemoryBuffer.h"

#include "../../lib/Core/Memory.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>

#include <spa/Path.h>

namespace {
	typedef enum {
		START,
		PATH,
		SYMBOL_NAME,
		SYMBOL_ARRAY,
		SYMBOL_VALUE,
		TAG_KEY,
		TAG_VALUE,
		CONSTRAINTS,
		PATH_DONE
	} LoadState_t;
}

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
				if ( name.compare( 0, strlen( SPA_OUTPUT_PREFIX ), SPA_OUTPUT_PREFIX ) == 0 )
					if ( const klee::ObjectState *os = kState->addressSpace().findObject( (*it).first ) )
						for ( unsigned int i = 0; i < os->size; i++ )
							symbolValues[name].push_back( os->read8( i ) );
			}
		}
		constraints = kState->constraints();
	}

	std::ostream& operator<<( std::ostream &stream, const Path &path ) {
		stream << SPA_PATH_START << std::endl;
		for ( std::map<std::string, const klee::Array *>::const_iterator it = path.symbolNames.begin(), ie = path.symbolNames.end(); it != ie; it++ ) {
			stream << SPA_PATH_SYMBOL_START << std::endl;
			stream << it->first << std::endl;
			stream << it->second->name << std::endl;
			// Symbolic value.
			if ( path.symbolValues.count( it->first ) ) {
				for ( std::set<const klee::Array *>::iterator it2 = path.symbols.begin(), ie2 = path.symbols.end(); it2 != ie2; it2++ )
					stream << "array " << (*it2)->name << "[" << (*it2)->size << "] : w32 -> w8 = symbolic" << std::endl;
				stream << SPA_PATH_KQUERY_EXPRESSIONS_START << std::endl;
				for ( std::vector<klee::ref<klee::Expr> >::const_iterator it2 = path.symbolValues.find( it->first )->second.begin(), ie2 = path.symbolValues.find( it->first )->second.end(); it2 != ie2; it2++ ) {
					if ( (*it2)->getKind() == klee::Expr::Constant )
						stream << "(w8 " << *it2 << ")" << std::endl;
					else
						stream << *it2 << std::endl;
				}
				stream << SPA_PATH_KQUERY_EXPRESSIONS_END << std::endl;
			} else {
				stream << "array " << it->second->name << "[" << it->second->size << "] : w32 -> w8 = symbolic" << std::endl;
			}
			stream << SPA_PATH_SYMBOL_END << std::endl;
		}

		for ( std::map<std::string, std::string>::const_iterator it = path.tags.begin(), ie = path.tags.end(); it != ie; it++ ) {
			stream << SPA_PATH_TAG_START << std::endl;
			stream << it->first << std::endl;
			stream << it->second << std::endl;
			stream << SPA_PATH_TAG_END << std::endl;
		}

		stream << SPA_PATH_CONSTRAINTS_START << std::endl;
		for ( std::set<const klee::Array *>::iterator it2 = path.symbols.begin(), ie2 = path.symbols.end(); it2 != ie2; it2++ )
			stream << "array " << (*it2)->name << "[" << (*it2)->size << "] : w32 -> w8 = symbolic" << std::endl;
		stream << SPA_PATH_KQUERY_CONSTRAINTS_START << std::endl;
		for ( klee::ConstraintManager::constraint_iterator it = path.getConstraints().begin(), ie = path.getConstraints().end(); it != ie; it++)
			stream << *it << std::endl;
		stream << SPA_PATH_KQUERY_CONSTRAINTS_END << std::endl;
		stream << SPA_PATH_CONSTRAINTS_END << std::endl;
		return stream << SPA_PATH_END << std::endl;
	}

	std::string cleanUpLine( std::string line ) {
		// Remove comments.
		line = line.substr( 0, line.find( SPA_PATH_COMMENT ) );
		// Remove trailing white space.
		line = line.substr( 0, line.find_last_not_of( SPA_PATH_WHITE_SPACE ) + 1 );
		// Remove leading white space.
		if ( line.find_first_not_of( SPA_PATH_WHITE_SPACE ) != line.npos )
			line = line.substr( line.find_first_not_of( SPA_PATH_WHITE_SPACE ), line.npos );
		
		return line;
	}

	klee::ref<klee::Expr> replaceArrays( klee::ref<klee::Expr> expr, std::map<std::string, const klee::Array *> &arrays ) {
		if ( expr->getKind() == klee::Expr::Read ) {
			klee::ref<klee::ReadExpr> readExpr = dyn_cast<klee::ReadExpr>( expr );
			readExpr->updates.root = arrays[readExpr->updates.root->name];
			return readExpr;
		} else {
			klee::ref<klee::Expr> *kids = new klee::ref<klee::Expr>[expr->getNumKids()];
			for ( unsigned i = 0; i < expr->getNumKids(); i++ )
				kids[i] = replaceArrays( expr->getKid( i ), arrays );
			return expr->rebuild( kids );
		}
	}

	#define changeState( from, to ) \
		if ( state != from ) { CLOUD9_ERROR( "Invalid path file. Error near line " << lineNumber << "." ); \
			assert( false && "Invalid path file." ); \
		} \
		state = to;

	std::set<Path *> Path::loadPaths( std::istream &pathFile ) {
		std::map<std::string, const klee::Array *> arrays;
		std::set<Path *> paths;
		int lineNumber = 0;

		LoadState_t state = START;
		Path *path = NULL;
		std::string symbolName, symbolArray, tagKey, tagValue, kQuery;
		while ( pathFile.good() ) {
			std::string line;
			getline( pathFile, line );
			line = cleanUpLine( line );
			lineNumber++;
			if ( line.empty() )
				continue;

			if ( line == SPA_PATH_START ) {
				changeState( START, PATH );
				path = new Path();
			} else if ( line == SPA_PATH_SYMBOL_START ) {
				changeState( PATH, SYMBOL_NAME );
				symbolName = "";
				symbolArray = "";
				kQuery = "";
			} else if ( line == SPA_PATH_SYMBOL_END ) {
				changeState( SYMBOL_VALUE, PATH );
				assert( (! symbolName.empty()) && (! symbolArray.empty()) && "Invalid path file." );
				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( "", MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing symbol value in path file." );
					if ( klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>( D ) ) {
						if ( ! arrays.count( AD->Root->name ) )
							arrays[AD->Root->name] = AD->Root;
						path->symbols.insert( arrays[AD->Root->name] );
						if ( symbolArray == AD->Root->name )
							path->symbolNames[symbolName] = arrays[AD->Root->name];
					} else if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						path->symbolValues[symbolName] = QC->Values;
						for ( std::vector<klee::ref<klee::Expr> >::iterator it = path->symbolValues[symbolName].begin(), ie = path->symbolValues[symbolName].end(); it != ie; it++ )
							*it = replaceArrays( *it, arrays );
						delete D;
						break;
					}
				}
				delete P;
				delete Builder;
				delete MB;
			} else if ( line == SPA_PATH_TAG_START ) {
				changeState( PATH, TAG_KEY );
				tagKey = "";
				tagValue = "";
			} else if ( line == SPA_PATH_TAG_END ) {
				changeState( TAG_VALUE, PATH );
				assert( (! tagKey.empty()) && (! tagValue.empty()) && "Invalid path file." );
				path->tags[tagKey] = tagValue;
			} else if ( line == SPA_PATH_CONSTRAINTS_START ) {
				changeState( PATH, CONSTRAINTS );
				kQuery = "";
			} else if ( line == SPA_PATH_CONSTRAINTS_END ) {
				changeState( CONSTRAINTS, PATH_DONE );

				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( "", MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing constraints in path file." );
					if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						for ( klee::ConstraintManager::constraint_iterator it = QC->Constraints.begin(), ie = QC->Constraints.end(); it != ie; it++ )
							path->constraints.addConstraint( replaceArrays( *it, arrays ) );
						delete D;
						break;
					}
				}
				delete P;
				delete Builder;
				delete MB;
			} else if ( line == SPA_PATH_END ) {
				changeState( PATH_DONE, START );
				paths.insert( path );
			} else {
				switch ( state ) {
					case SYMBOL_NAME : {
						assert( symbolName.empty() && "Invalid path file." );
						symbolName = line;
						changeState( SYMBOL_NAME, SYMBOL_ARRAY );
					} break;
					case SYMBOL_ARRAY : {
						assert( symbolArray.empty() && "Invalid path file." );
						symbolArray = line;
						changeState( SYMBOL_ARRAY, SYMBOL_VALUE );
					} break;
					case SYMBOL_VALUE : {
						kQuery += " " + line;
					} break;
					case TAG_KEY : {
						assert( tagKey.empty() && "Invalid path file." );
						tagKey = line;
						changeState( TAG_KEY, TAG_VALUE );
					} break;
					case TAG_VALUE : {
						assert( tagValue.empty() && "Invalid path file." );
						tagValue = line;
					} break;
					case CONSTRAINTS : {
						kQuery += " " + line;
					} break;
					default : {
						assert( false && "Invalid path file." );
					} break;
				}
			}
		}

		std::cerr << "Loaded " << paths.size() << " paths." << std::endl;

		return paths;
	}
}
