#include "llvm/Support/CommandLine.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <set>

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Function.h"

#include <spa/SPA.h>
#include <spa/CG.h>

#define MAIN_ENTRY_FUNCTION				"__user_main"
#define ALTERNATIVE_MAIN_ENTRY_FUNCTION	"main"
#define OLD_ENTRY_FUNCTION				"__spa_old_main"

extern std::string InputFile;

namespace {
	llvm::cl::opt<std::string> Client( "client",
		llvm::cl::desc( "Species the client binary (to transform or run)." ) );

	llvm::cl::opt<std::string> Server( "server",
		llvm::cl::desc( "Specifies the server binary (to transform or to run)." ) );

	llvm::cl::opt<std::string> Input( "input",
		llvm::cl::desc( "Specifies the file to load test data from." ) );

	llvm::cl::opt<std::string> Output( "output",
		llvm::cl::desc( "Specifies the file to output the modified binary or the test results to." ) );
}

/**
 * Generates a new main function that looks like:
 * 
 *	int main( int argc, char **argv ) {
 *		handler();
 *		return 0;
 *	}
 */
void generateMain( llvm::Module *module, llvm::Function *handler ) {
	// Rename old main function
	llvm::Function *oldEntryFunction = module->getFunction( MAIN_ENTRY_FUNCTION );
	if ( ! oldEntryFunction )
		oldEntryFunction = module->getFunction( ALTERNATIVE_MAIN_ENTRY_FUNCTION );
	assert( oldEntryFunction && "No main function found to replace." );
	std::string entryFunctionName = oldEntryFunction->getName().str();
	oldEntryFunction->setName( OLD_ENTRY_FUNCTION );
	// Create new one.
	llvm::Function *entryFunction = llvm::Function::Create(
		oldEntryFunction->getFunctionType(),
		llvm::GlobalValue::ExternalLinkage, entryFunctionName, module );
	entryFunction->setCallingConv( llvm::CallingConv::C );
	// Replace the old with the new.
	oldEntryFunction->replaceAllUsesWith( entryFunction );

	// Declare arguments.
	llvm::Function::arg_iterator args = entryFunction->arg_begin();
	llvm::Value *argcVar = args++;
	argcVar->setName( "argc" );
	llvm::Value *argvVar = args++;
	argvVar->setName( "argv" );

	// Create the entry basic block.
	llvm::BasicBlock* entryBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);

	// Allocate arguments.
	new llvm::StoreInst( argcVar, new llvm::AllocaInst( llvm::IntegerType::get( module->getContext(), 32 ), "", entryBB ), false, entryBB );
	new llvm::StoreInst( argvVar, new llvm::AllocaInst( llvm::PointerType::get( llvm::PointerType::get( llvm::IntegerType::get( module->getContext(), 8 ), 0 ), 0 ), "", entryBB ), false, entryBB );

	// handler();
	llvm::CallInst *handlerCallInst = llvm::CallInst::Create( handler, "", entryBB );
	handlerCallInst->setCallingConv( llvm::CallingConv::C );
	handlerCallInst->setTailCall( false );

	// return 0;
	llvm::ReturnInst::Create( module->getContext(), llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) ), entryBB );
}

void xformClient() {
	std::cerr << "Loading client byte-code from: " << Client << std::endl;
	llvm::OwningPtr<llvm::MemoryBuffer> mb;
	assert( llvm::MemoryBuffer::getFile( Client.c_str(), mb ) == 0 );

	llvm::LLVMContext ctx;
	llvm::Module *module;
	assert( module = llvm::ParseBitcodeFile( mb.get(), ctx ) );

	SPA::CFG cfg( module );
	SPA::CG cg( cfg );

	llvm::Function *fn;
	assert( (fn = module->getFunction( SPA_API_ANNOTATION_FUNCTION )) && "API annotation function not present in module." );

	std::set<llvm::Instruction *> apiCallers = cg.getDefiniteCallers( fn );
	assert( apiCallers.size() == 1 && "Currently only handling one API handler." );

	generateMain( module, (*apiCallers.begin())->getParent()->getParent() );

	std::string outFileName = Output;
	if ( outFileName == "" )
		outFileName = Client + ".chtest.bc";
	std::cerr << "Writing client byte-code to: " << outFileName << std::endl;
	std::string errInfo;
	llvm::raw_fd_ostream outFile( outFileName.c_str(), errInfo );
	assert( errInfo.empty() && "Error opening output file." );
	llvm::WriteBitcodeToFile( module, outFile );
}

void xformServer() {
	std::cerr << "Loading server byte-code from: " << Server << std::endl;
	llvm::OwningPtr<llvm::MemoryBuffer> mb;
	assert( llvm::MemoryBuffer::getFile( Server.c_str(), mb ) );

	llvm::LLVMContext ctx;
	llvm::Module *module;
	assert( module = llvm::ParseBitcodeFile( mb.get(), ctx ) );

	SPA::CFG cfg( module );
	SPA::CG cg( cfg );
	
	Function *fn;
	assert( (fn = module->getFunction( SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION )) && "Message handler annotation function not present in module." );

	std::set<llvm::Instruction *> mhCallers = cg.getDefiniteCallers( fn );
	assert( mhCallers.size() == 1 && "Currently only handling one message handler." );

	generateMain( module, (*mhCallers.begin())->getParent()->getParent() );

	std::string outFileName = Output;
	if ( outFileName == "" )
		outFileName = Server + ".shtest.bc";
	std::cerr << "Writing server byte-code to: " << outFileName << std::endl;
	std::string errInfo;
	llvm::raw_fd_ostream outFile( outFileName.c_str(), errInfo );
	assert( errInfo.empty() && "Error opening output file." );
	llvm::WriteBitcodeToFile( module, outFile );
}

void runTests() {
	assert( Input.size() > 0 && "Must specify --input." );
	std::cerr << "Loading test inputs from: " << Input << std::endl;

	std::string outFileName = Output;
	if ( outFileName == "" )
		outFileName = Input + ".tested";
	std::cerr << "Writing test results to: " << outFileName << std::endl;

	std::ifstream inputFile( Input.c_str() );
	std::ofstream outputFile( outFileName.c_str() );

	std::string bundle;
	while ( inputFile.good() ) {
		std::string line;
		std::getline( inputFile, line );

		if ( ! line.empty() ) {
			bundle += line + '\n';

			size_t d = line.find( ' ' );
			std::string name = line.substr( 0, d );
			assert( ! name.empty() && "Empty variable name." );
			std::string value;
			if ( d != std::string::npos )
				value = line.substr( d + 1, std::string::npos );
			setenv( name.c_str(), value.c_str(), 1 );
		} else {
			if ( ! bundle.empty() ) {
				std::cerr << "Processing test case." << std::endl;

				std::string logFileName = tmpnam( NULL );
				std::cerr << "Logging results to: " << logFileName << std::endl;

				pid_t pid = fork();
				assert( pid >= 0 && "Error forking client process." );
				if ( pid == 0 ) {
					setpgid( 0, 0 );
					setenv( "SPA_LOG_FILE", logFileName.c_str(), 1 );
					std::cerr << "Launching client handler: " << Client << std::endl;
					exit( system( Client.c_str() ) );
				}
				sleep( 1 );
				std::cerr << "Killing process." << std::endl;
				kill( pid, SIGTERM );

				int status = 0;
// 				std::cerr << "Waiting for process." << std::endl;
				assert( waitpid( pid, &status, 0 ) != -1 );

				std::cerr << "Processing outputs." << std::endl;
				std::ifstream logFile( logFileName.c_str() );
				std::string prefix = "spa_out_msg_";
				while ( logFile.good() ) {
					std::getline( logFile, line );
					if ( line.compare( 0, prefix.size(), prefix ) == 0 ) {
						std::cerr << "Transforming: " << line << std::endl;
						size_t boundary = line.find( ' ' );
						assert( boundary != std::string::npos && "Malformed output." );
						std::string name = "spa_in_msg_" + line.substr( prefix.size(), boundary - prefix.size() );
						std::string value = line.substr( boundary, line.size() - boundary );
						setenv( name.c_str(), value.c_str(), 1 );
					}
				}
				logFile.close();
				remove( logFileName.c_str() );

				pid = fork();
				assert( pid >= 0 && "Error forking server process." );
				if ( pid == 0 ) {
					setpgid( 0, 0 );
					std::cerr << "Launching server handler: " << Server << std::endl;
					exit( system( Server.c_str() ) );
				}
				sleep( 1 );
				std::cerr << "Killing process." << std::endl;
				kill( pid, SIGTERM );

// 				std::cerr << "Waiting for process." << std::endl;
				assert( waitpid( pid, &status, 0 ) != -1 );

				if ( (! WIFEXITED( status )) || WEXITSTATUS( status ) == 201 ) {
					std::cerr << "Found true positive. Outputting" << std::endl;
					outputFile << bundle << std::endl;
				} else {
					std::cerr << "Found false positive. Filtering." << std::endl;
				}
				bundle.clear();
				std::cerr << "--------------------------------------------------" << std::endl;
			}
		}
	}
}

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	llvm::cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Handler Test Generator/Runner" );

	assert( (Client.size() > 0 || Server.size() > 0) && "Must specify --client or --server." );
	if ( Client.size() > 0 && Server.size() == 0 )
		xformClient();
	else if ( Client.size() == 0 && Server.size() > 0 )
		xformServer();
	else
		runTests();
}
