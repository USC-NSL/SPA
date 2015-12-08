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

void processPath(SPA::Path *path, unsigned long pathID) {
  std::map<std::string, std::shared_ptr<participant_t> > participantByName;
  std::map<std::string, std::shared_ptr<participant_t> > participantByIpPort;

  std::vector<std::shared_ptr<message_t> > messages;

  // Load participant names.
  for (auto it : path->getParticipants()) {
    if (!participantByName.count(it->getName())) {
      if (Debug) {
        klee::klee_message("  Found participant: %s", it->getName().c_str());
      }
      participantByName[it->getName()].reset(new participant_t {
        it->getName(), ""
      });
      participantByName[API_PREFIX + it->getName()].reset(new participant_t {
        API_PREFIX + it->getName(), ""
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

  std::ofstream dotFile(Directory + "/" + path->getUUID() + ".dot");
  std::ofstream htmlFile(Directory + "/" + path->getUUID() + ".html.inc");
  assert(dotFile.good() && htmlFile.good());

  htmlFile << "    <div id=\"header\">" << std::endl;
  htmlFile << "      <a href=\"#metadata\">Path Meta-data</a>" << std::endl;
  htmlFile << "      <a href=\"#messages\">Message Log</a>" << std::endl;
  htmlFile << "      <a href=\"#constraints\">Symbolic Contraints</a>"
           << std::endl;
  htmlFile << "      <a href=\"#src\">Path Source</a>" << std::endl;
  htmlFile << "      <a href=\"index.html\">Path Index</a>" << std::endl;
  htmlFile << "    </div>" << std::endl;

  htmlFile << "    <a class='anchor' id='metadata'></a>" << std::endl;
  htmlFile << "    <h1>Path " << pathID << "</h1>" << std::endl;

  htmlFile << "    <h2>Path Meta-data</h2>" << std::endl;
  htmlFile << "    <p><b>UUID:</b> " << path->getUUID() << "</p>" << std::endl;

  htmlFile << "    <p><b>Lineage:</b>" << std::endl;
  htmlFile << "      <ol>" << std::endl;
  for (auto it : path->getParticipants()) {
    htmlFile << "      <li><a href='" << it->getPathUUID() << ".html'>"
             << it->getName() << "</a></td></tr>" << std::endl;
  }
  htmlFile << "      </ol>" << std::endl;
  htmlFile << "    </p>" << std::endl;

  htmlFile << "    <p><b>Tags:</b>" << std::endl;
  htmlFile << "      <table border='1'>" << std::endl;
  htmlFile << "        <tr><th>Key</th><th>Value</th></tr>" << std::endl;
  for (auto it : path->getTags()) {
    htmlFile << "        <tr><td>" << it.first << "</td><td>" << it.second
             << "</td></tr>" << std::endl;
  }
  htmlFile << "      </table>" << std::endl;
  htmlFile << "    </p>" << std::endl;

  htmlFile << "    <a class='anchor' id='messages'></a>" << std::endl;
  htmlFile << "    <h2>Message Log</h2>" << std::endl;
  htmlFile << "    <img src='" << path->getUUID() << ".svg' usemap='#G' />"
           << std::endl;

  htmlFile << "    <a class='anchor' id='constraints'></a>" << std::endl;
  htmlFile << "    <h2>Symbolic Constraint</h2>" << std::endl;
  htmlFile << "    <p><b>Input Symbols:</b><br />" << std::endl;
  htmlFile << "    <table border='1'>" << std::endl;
  htmlFile << "      <tr><th>Name</th><th>Instances</th></tr>" << std::endl;
  for (auto it : path->getInputSymbols()) {
    htmlFile << "      <tr>" << std::endl;
    htmlFile << "        <td>" << it.first << "</td>" << std::endl;
    htmlFile << "        <td>" << std::endl;
    for (unsigned long i = 0; i < it.second.size(); i++) {
      htmlFile << "          <a href='#" << it.second[i]->getName() << "'>"
               << (i + 1) << "</a>" << std::endl;
    }
    htmlFile << "        </td>" << std::endl;
    htmlFile << "      </tr>" << std::endl;
  }
  htmlFile << "    </table></p>" << std::endl;

  htmlFile << "    <p><b>Constraints:</b><br />" << std::endl;
  for (auto it : path->getConstraints()) {
    std::string exprStr;
    llvm::raw_string_ostream exprROS(exprStr);
    it->print(exprROS);
    htmlFile << "    " << exprROS.str() << "<br />" << std::endl;
  }
  htmlFile << "    </p>" << std::endl;

  htmlFile << "    <p><b>Output Symbols:</b><br />" << std::endl;
  htmlFile << "    <table border='1'>" << std::endl;
  htmlFile << "      <tr><th>Name</th><th>Instances</th></tr>" << std::endl;
  for (auto it : path->getOutputSymbols()) {
    htmlFile << "      <tr>" << std::endl;
    htmlFile << "        <td>" << it.first << "</td>" << std::endl;
    htmlFile << "        <td>" << std::endl;
    for (unsigned long i = 0; i < it.second.size(); i++) {
      htmlFile << "          <a href='#" << it.second[i]->getName() << "'>"
               << (i + 1) << "</a>" << std::endl;
    }
    htmlFile << "        </td>" << std::endl;
    htmlFile << "      </tr>" << std::endl;
  }
  htmlFile << "    </table></p>" << std::endl;

  dotFile << "digraph G {" << std::endl;
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
            << "\" href=\"#msg" << i << "\"]" << std::endl;

    htmlFile << "    <div class='box' id='msg" << i << "'>" << std::endl;
    htmlFile << "      <a class='closebutton' href='#'>&#x2715;</a>"
             << std::endl;
    htmlFile << "      <h1>Message " << i << " symbols</h1>" << std::endl;
    htmlFile << "      <ul>" << std::endl;
    for (auto it : messages[i]->symbolNames) {
      htmlFile << "        <li><a href='#" << it << "'>" << it << "</a></li>"
               << std::endl;
    }
    htmlFile << "      </ul>" << std::endl;
    htmlFile << "    </div>" << std::endl;
  }
  dotFile << "}" << std::endl;

  for (auto sit : path->getSymbolLog()) {
    htmlFile << "    <div class='box' id='" << sit->getName() << "'>"
             << std::endl;
    htmlFile << "      <a class='closebutton' href='#'>&#x2715;</a>"
             << std::endl;
    htmlFile << "      <h1>Symbol " << sit->getName() << "</h1>" << std::endl;
    if (sit->isInput()) {
      htmlFile << "      <p><b>Type:</b> input</p>" << std::endl;
      htmlFile << "      <p><b>Size:</b> " << sit->getInputArray()->size
               << "</p>" << std::endl;
      if (path->getTestInputs().count(sit->getName())) {
        std::stringstream value;
        copy(path->getTestInput(sit->getName()).begin(),
             path->getTestInput(sit->getName()).end(),
             std::ostream_iterator<int>(value, " "));
        htmlFile << "      <p><b>Test Case Value:</b> " << value.str() << "</p>"
                 << std::endl;
      }
    } else if (sit->isOutput()) {
      htmlFile << "      <p><b>Type:</b> output</p>" << std::endl;
      htmlFile << "      <p><b>Size:</b> " << sit->getOutputValues().size()
               << "</p>" << std::endl;
      htmlFile << "      <p><b>Byte Values:</b>" << std::endl;
      htmlFile << "        <table border='1'>" << std::endl;
      htmlFile << "          <tr><th>Index</th><th>Expression</th></tr>"
               << std::endl;
      for (unsigned long i = 0; i < sit->getOutputValues().size(); i++) {
        std::string exprStr;
        llvm::raw_string_ostream exprROS(exprStr);
        sit->getOutputValues()[i]->print(exprROS);
        htmlFile << "          <tr><td>" << i << "</td><td>" << exprROS.str()
                 << "</td></tr>" << std::endl;
      }
      htmlFile << "        </table>" << std::endl;
      htmlFile << "      </p>" << std::endl;
    } else {
      assert(false && "Unknown symbol type.");
    }
    htmlFile << "    </div>" << std::endl;
  }

  htmlFile << "    <a class='anchor' id='src'></a>" << std::endl;
  htmlFile << "    <h2>Path Source</h2>" << std::endl;
  htmlFile << "<pre>" << std::endl;
  htmlFile << *path;
  htmlFile << "</pre>" << std::endl;
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

  makefile << "%.html: %.cmapx %.html.inc" << std::endl;
  makefile << "\techo '<!doctype html>' > $@" << std::endl;
  makefile << "\techo '<html lang=\"en\">' >> $@" << std::endl;
  makefile << "\techo '  <head>' >> $@" << std::endl;
  makefile << "\techo '    <meta charset=\"utf-8\">' >> $@" << std::endl;
  makefile << "\techo '    <title>$(@:.html=)</title>' >> $@" << std::endl;
  makefile << "\techo '    <link rel=\"stylesheet\" type=\"text/css\" "
              "href=\"style.css\">' >> $@" << std::endl;
  makefile << "\techo '  </head>' >> $@" << std::endl;
  makefile << "\techo '  <body>' >> $@" << std::endl;
  makefile << "\tcat $(@:.html=.html.inc) >> $@" << std::endl;
  makefile << "\tcat $(@:.html=.cmapx) >> $@" << std::endl;
  makefile << "\techo '  </body>' >> $@" << std::endl;
  makefile << "\techo '</html>' >> $@" << std::endl;

  makefile << "%.clean:" << std::endl;
  makefile << "\trm -f $(@:.clean=.html) $(@:.clean=.svg) $(@:.clean=.cmapx)"
           << std::endl << std::endl;

  makefile << "default: all" << std::endl;
  makefile << "all: index" << std::endl;
  makefile << "index: index.html index.svg" << std::endl;
  makefile << "clean: index.clean" << std::endl;

  std::ofstream cssFile(Directory + "/style.css");
  assert(cssFile.good());
  cssFile << "body {" << std::endl;
  cssFile << "  padding-top: 50px;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "a.anchor {" << std::endl;
  cssFile << "  display: block;" << std::endl;
  cssFile << "  position: relative;" << std::endl;
  cssFile << "  top: -50px;" << std::endl;
  cssFile << "  visibility: hidden;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div#header {" << std::endl;
  cssFile << "  position: fixed;" << std::endl;
  cssFile << "  width: 100%;" << std::endl;
  cssFile << "  top: 0; " << std::endl;
  cssFile << "  left: 0; " << std::endl;
  cssFile << "  margin: 0;" << std::endl;
  cssFile << "  padding: 0;" << std::endl;
  cssFile << "  background-color: lightgray;" << std::endl;
  cssFile << "  display: table;" << std::endl;
  cssFile << "  table-layout: fixed;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div#header a {" << std::endl;
  cssFile << "  display: table-cell;" << std::endl;
  cssFile << "  padding: 14px 16px;" << std::endl;
  cssFile << "  text-align: center;" << std::endl;
  cssFile << "  vertical-align: middle;" << std::endl;
  cssFile << "  text-decoration: none;" << std::endl;
  cssFile << "  font-weight: bold;" << std::endl;
  cssFile << "  color: black;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div#header a:hover {" << std::endl;
  cssFile << "  background-color: black;" << std::endl;
  cssFile << "  color: white;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.box {" << std::endl;
  cssFile << "  display: none;" << std::endl;
  cssFile << "  width: 1000px;" << std::endl;
  cssFile << "  position: fixed;" << std::endl;
  cssFile << "  top: 100px; " << std::endl;
  cssFile << "  left: 50%;" << std::endl;
  cssFile << "  margin-left: -500px;" << std::endl;
  cssFile << "  border-radius: 10px;" << std::endl;
  cssFile << "  padding: 10px;" << std::endl;
  cssFile << "  border: 5px solid black;" << std::endl;
  cssFile << "  background-color: white;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.box:target {" << std::endl;
  cssFile << "  display: block;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "a.closebutton {" << std::endl;
  cssFile << "  position: absolute;" << std::endl;
  cssFile << "  top: 0;" << std::endl;
  cssFile << "  right: 5px;" << std::endl;
  cssFile << "  font-size: 20pt;" << std::endl;
  cssFile << "}" << std::endl;

  std::ofstream indexDot(Directory + "/index.dot");
  std::ofstream indexHtml(Directory + "/index.html.inc");
  assert(indexDot.good() && indexHtml.good());

  indexHtml << "    <img src='index.svg' usemap='#G' />" << std::endl;

  indexDot << "digraph G {" << std::endl;
  indexDot << "  root [label=\"Path 0\"]" << std::endl;

  SPA::PathLoader pathLoader(inFile);
  std::unique_ptr<SPA::Path> path;
  unsigned long pathID = 0;
  std::map<std::string, unsigned long> uuidToId;

  while (path.reset(pathLoader.getPath()), path) {
    klee::klee_message("Processing path %ld.", ++pathID);

    uuidToId[path->getUUID()] = pathID;

    processPath(path.get(), pathID);

    makefile << "all: " << path->getUUID() << std::endl;
    makefile << path->getUUID() << ": " << path->getUUID() << ".html "
             << path->getUUID() << ".svg" << std::endl;
    makefile << "clean: " << path->getUUID() << ".clean" << std::endl;

    indexDot << "  path_" << pathID << " [label=\"Path " << pathID << "\\n"
             << path->getParticipants().back()->getName() << "\" URL=\""
             << path->getUUID() << ".html\"]" << std::endl;
    if (path->getParticipants().size() == 1) {
      indexDot << "  root -> path_" << pathID << std::endl;
    } else if (path->getParticipants().size() >= 2) {
      auto parent = path->getParticipants().size() - 2;
      indexDot << "  path_"
               << uuidToId[path->getParticipants()[parent]->getPathUUID()]
               << " -> path_" << pathID << std::endl;
    }
  }

  indexDot << "}" << std::endl;

  return 0;
}
