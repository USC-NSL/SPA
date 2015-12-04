#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"

#include <spa/SPA.h>
#include <spa/PathLoader.h>
#include <spa/Util.h>

#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<std::string> Directory(
    "dir", llvm::cl::init("."),
    llvm::cl::desc(
        "The directory to place teh generated documentation in (default: .)."));

llvm::cl::opt<bool> Debug("d", llvm::cl::desc("Enable debug messages."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));
}

typedef struct {
  std::string name;
  std::string ipPort;
} participant_t;
#define API_PREFIX "api_"

typedef struct {
  std::shared_ptr<participant_t> from;
  std::shared_ptr<participant_t> to;
  bool received;
  std::set<std::string> symbolNames;
} message_t;

std::string getIpPort(SPA::Symbol *symbol) {
  std::string symbolName = symbol->getName();
  // Remove counter.
  symbolName = symbolName.substr(0, symbolName.rfind(SPA_SYMBOL_DELIMITER));
  // Remove participant.
  symbolName = symbolName.substr(0, symbolName.rfind(SPA_SYMBOL_DELIMITER));
  // Remove prefixes.
  symbolName = symbolName.substr(symbolName.rfind(SPA_SYMBOL_DELIMITER) + 1);
  // Convert ip.port -> ip:port
  symbolName = symbolName.replace(symbolName.rfind("."), 1, ":");

  return symbolName;
}

std::string sanitize(std::string name) {
  // Sanitize name to make valid token.
  size_t pos;
  while ((pos = name.find_first_of("-.:")) != std::string::npos) {
    name.replace(pos, 1, "_");
  }
  return "node_" + name;
}

void processPath(SPA::Path *path) {
  std::map<std::string, std::shared_ptr<participant_t> > participantByName;
  std::map<std::string, std::shared_ptr<participant_t> > participantByIpPort;

  std::vector<std::shared_ptr<message_t> > messages;

  // Load participant names.
  for (auto it : path->getParticipants()) {
    if (!participantByName.count(it)) {
      if (Debug) {
        klee::klee_message("  Found participant: %s", it.c_str());
      }
      participantByName[it].reset(new participant_t {
        it, ""
      });
      participantByName[API_PREFIX + it].reset(new participant_t {
        API_PREFIX + it, ""
      });
    }
  }

  // Load participant IP/Ports.
  for (auto it : path->getSymbolLog()) {
    if ((it->isMessage() && it->isInput()) ||
        it->getName().compare(0, strlen(SPA_MESSAGE_OUTPUT_SOURCE_PREFIX),
                              SPA_MESSAGE_OUTPUT_SOURCE_PREFIX) == 0) {
      std::string participant = it->getParticipant();
      std::string ipPort;

      if (it->isInput()) {
        ipPort = getIpPort(it.get());
      } else if (it->getName().compare(0,
                                       strlen(SPA_MESSAGE_OUTPUT_SOURCE_PREFIX),
                                       SPA_MESSAGE_OUTPUT_SOURCE_PREFIX) == 0) {
        struct sockaddr_in src;
        assert(it->getOutputValues().size() == sizeof(src));
        for (unsigned i = 0; i < sizeof(src); i++) {
          klee::ConstantExpr *ce =
              llvm::dyn_cast<klee::ConstantExpr>(it->getOutputValues()[i]);
          assert(ce && "Non-constant source IP.");
          ((char *)&src)[i] = ce->getLimitedValue();
        }
        char srcTxt[INET_ADDRSTRLEN];
        assert(inet_ntop(AF_INET, &src.sin_addr, srcTxt, sizeof(srcTxt)));

        ipPort = std::string(srcTxt) + ":" + SPA::numToStr(ntohs(src.sin_port));
      }

      assert(participantByName.count(participant) &&
             (participantByName[participant]->ipPort.empty() ||
              participantByName[participant]->ipPort == ipPort));

      if (Debug) {
        klee::klee_message("  Participant %s listens on: %s",
                           participant.c_str(), ipPort.c_str());
      }
      participantByName[participant]->ipPort = ipPort;
      participantByIpPort[ipPort] = participantByName[participant];
    }
  }

  // Add unknown participants.
  unsigned long unknownId = 1;
  for (auto it : path->getSymbolLog()) {
    if (it->isMessage() && it->isOutput()) {
      std::string ipPort = getIpPort(it.get());
      if (!participantByIpPort.count(ipPort)) {
        if (Debug) {
          klee::klee_message("  Found unknown participant on: %s",
                             ipPort.c_str());
        }
        std::string name = "Unknown-" + SPA::numToStr(unknownId++);
        participantByName[name].reset(new participant_t {
          name, ipPort
        });
        participantByIpPort[ipPort] = participantByName[name];
      }
    }
  }

  // Load messages.
  for (auto sit : path->getSymbolLog()) {
    if (Debug) {
      klee::klee_message("  Symbol: %s", sit->getName().c_str());
    }

    std::string participant = sit->getParticipant();
    assert(participantByName.count(participant) &&
           participantByName.count(API_PREFIX + participant));

    if (sit->isInput()) {
      if (sit->isAPI()) {
        if (Debug) {
          klee::klee_message("  Input API to: %s", participant.c_str());
        }
        messages.emplace_back(new message_t());
        messages.back()->from = participantByName[API_PREFIX + participant];
        messages.back()->to = participantByName[participant];
        messages.back()->received = true;
        messages.back()->symbolNames.insert(sit->getName());
      } else if (sit->isMessage()) {
        // Ignore message size and source symbols.
        if (sit->getName().compare(0, strlen(SPA_MESSAGE_INPUT_SIZE_PREFIX),
                                   SPA_MESSAGE_INPUT_SIZE_PREFIX) == 0 ||
            sit->getName().compare(0, strlen(SPA_MESSAGE_INPUT_SOURCE_PREFIX),
                                   SPA_MESSAGE_INPUT_SOURCE_PREFIX) == 0) {
          continue;
        }
        bool sent = false;
        // Find first outstanding compatible message.
        for (auto mit : messages) {
          if (mit->to->name == participant && !mit->received) {
            if (Debug) {
              klee::klee_message("  Received message: %s -> %s",
                                 mit->from->name.c_str(),
                                 mit->to->name.c_str());
            }
            mit->received = true;
            sent = true;
            break;
          }
        }
        assert(sent && "Message received before being sent.");
      } else {
        assert(false && "Symbol is neither message nor API.");
      }
    } else if (sit->isOutput()) {
      if (sit->isAPI()) {
        if (Debug) {
          klee::klee_message("  Output API from: %s", participant.c_str());
        }
        messages.emplace_back(new message_t());
        messages.back()->from = participantByName[participant];
        messages.back()->to = participantByName[API_PREFIX + participant];
        messages.back()->received = true;
        messages.back()->symbolNames.insert(sit->getName());
      } else if (sit->isMessage()) {
        assert(participantByIpPort.count(getIpPort(sit.get())) &&
               "Message sent to unknown participant.");
        if (Debug) {
          klee::klee_message(
              "  Sent message: %s -> %s", participant.c_str(),
              participantByIpPort[getIpPort(sit.get())]->name.c_str());
        }
        // Collapse content size and source symbols into one message.
        if (sit->getName().compare(0, strlen(SPA_MESSAGE_OUTPUT_SIZE_PREFIX),
                                   SPA_MESSAGE_OUTPUT_SIZE_PREFIX) == 0 ||
            sit->getName().compare(0, strlen(SPA_MESSAGE_OUTPUT_SOURCE_PREFIX),
                                   SPA_MESSAGE_OUTPUT_SOURCE_PREFIX) == 0) {
          assert(messages.back()->from == participantByName[participant] &&
                 messages.back()->to ==
                     participantByIpPort[getIpPort(sit.get())]);
          messages.back()->symbolNames.insert(sit->getName());
        } else {
          messages.emplace_back(new message_t());
          messages.back()->from = participantByName[participant];
          messages.back()->to = participantByIpPort[getIpPort(sit.get())];
          messages.back()->symbolNames.insert(sit->getName());
        }
      } else {
        assert(false && "Symbol is neither message nor API.");
      }
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }

  // Generate CFG DOT file.
  std::ofstream dotFile(Directory + "/" + path->getUUID() + ".dot");
  assert(dotFile.good());

  dotFile << "digraph CFG {" << std::endl;

  for (auto it : participantByName) {
    std::string participantToken = sanitize(it.first);
    if (it.first.compare(0, strlen(API_PREFIX), API_PREFIX) == 0) {
      dotFile << "  " << participantToken << " [style=\"invis\"]" << std::endl;
    } else {
      dotFile << "  " << participantToken << " [label=\"" << it.first << "\\n"
              << it.second->ipPort << "\"]" << std::endl;
    }
  }

  for (unsigned i = 0; i < messages.size(); i++) {
    dotFile << "  " << sanitize(messages[i]->from->name) << " -> "
            << sanitize(messages[i]->to->name) << "[label=\"" << (i + 1)
            << "\" arrowhead=\"" << (messages[i]->received ? "normal" : "dot")
            << "\" tooltip=\"";
    for (auto it : messages[i]->symbolNames) {
      dotFile << it << "&#10;";
    }
    dotFile << "\"]" << std::endl;
  }

  dotFile << "}" << std::endl;
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv,
      "Systematic Protocol Analysis - Path File Documentation Generation");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  auto result = mkdir(Directory.c_str(), 0755);
  assert(result == 0 || errno == EEXIST);

  std::ofstream makefile(Directory + "/Makefile");
  assert(makefile.good());
  makefile << "%.pdf: %.dot" << std::endl;
  makefile << "\tdot -Tpdf -o $@ $<" << std::endl << std::endl;

  makefile << "%.svg: %.dot" << std::endl;
  makefile << "\tdot -Tsvg -o $@ $<" << std::endl << std::endl;

  makefile << "%.cmapx: %.dot" << std::endl;
  makefile << "\tdot -Tcmapx -o $@ $<" << std::endl << std::endl;

  makefile << "%.html: %.cmapx" << std::endl;
  makefile
      << "\techo \"<html><img src='$(@:.html=.svg)' usemap='#CFG' />\" > $@"
      << std::endl;
  makefile << "\tcat $< >> $@" << std::endl;
  makefile << "\techo '</html>' >> $@" << std::endl << std::endl;

  makefile << "%.clean:" << std::endl;
  makefile << "\trm -f $(@:.clean=.html) $(@:.clean=.svg) $(@:.clean=.cmapx)"
           << std::endl << std::endl;

  makefile << "default: all" << std::endl;

  std::ofstream index(Directory + "/index.html");
  assert(index.good());
  index << "<!DOCTYPE html>" << std::endl;
  index << "<html lang=\"en\">" << std::endl;
  index << "  <head>" << std::endl;
  index << "    <meta charset=\"utf-8\">" << std::endl;
  index << "    <title>" << InFileName << "</title>" << std::endl;
  index << "  </head>" << std::endl;
  index << "  <body>" << std::endl;
  index << "    <ol>" << std::endl;

  SPA::PathLoader pathLoader(inFile);
  std::unique_ptr<SPA::Path> path;
  unsigned long numPaths = 0;

  while (path.reset(pathLoader.getPath()), path) {
    klee::klee_message("Processing path %ld.", ++numPaths);

    processPath(path.get());

    makefile << "all: " << path->getUUID() << std::endl;
    makefile << path->getUUID() << ": " << path->getUUID() << ".html "
             << path->getUUID() << ".svg" << std::endl;
    makefile << "clean: " << path->getUUID() << ".clean" << std::endl;

    index << "      <li><a href=\"" << path->getUUID() << ".html\">Path "
          << numPaths << "</a></li>" << std::endl;
  }

  index << "    </ol>" << std::endl;
  index << "  </body>" << std::endl;
  index << "</html>" << std::endl;

  return 0;
}
