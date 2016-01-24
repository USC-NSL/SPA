#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"

#include <spa/SPA.h>
#include <spa/PathLoader.h>
#include <spa/Util.h>

#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
        "The directory to place the generated documentation in (default: .)."));

llvm::cl::opt<int> OnlyPathID(
    "only-path",
    llvm::cl::desc("Only generate index and documentation for given path ID."));

llvm::cl::opt<int> StartFromPathID(
    "start-from-path", llvm::cl::init(1),
    llvm::cl::desc(
        "Only generate index and documentation from given path ID onward."));

llvm::cl::opt<int>
    NumProcesses("j", llvm::cl::init(1),
                 llvm::cl::desc("Number of worker processes to spawn."));

llvm::cl::opt<bool> Debug("d", llvm::cl::desc("Enable debug messages."));

llvm::cl::list<std::string> SrcDirMap(
    "map-src",
    llvm::cl::desc(
        "Remaps source directories when searching for source code files."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));
}

typedef struct {
  std::string name;
  std::set<std::string> bindings;
} participant_t;
#define API_PREFIX "api_"
#define API_INPUT_PREFIX API_PREFIX "in_"
#define API_OUTPUT_PREFIX API_PREFIX "out_"

typedef struct {
  std::shared_ptr<participant_t> fromParticipant;
  std::shared_ptr<participant_t> toParticipant;
  std::string sourceIP, sourcePort, protocol, destinationIP, destinationPort;
  std::vector<klee::ref<klee::Expr> > content;
  // Sending symbol and corresponding receiving symbol.
  std::vector<std::pair<std::shared_ptr<SPA::Symbol>,
                        std::shared_ptr<SPA::Symbol> > > symbols;
} message_t;

// participants -> uuids.
std::map<std::vector<std::string>, std::set<std::string> > conversations;

// uuid -> path-id.
std::map<std::string, unsigned long> uuidToId;

// uuid -> participant.
std::map<std::string, std::string> uuidToParticipant;

// uuid -> uuid.
std::map<std::string, std::string> parentPath;

// uuid -> [participant, uuid]s.
std::map<std::string, std::set<std::pair<std::string, std::string> > >
    childrenPaths;

// file -> (line -> (covered -> uuids))
std::map<std::string,
         std::map<unsigned long, std::map<bool, std::set<std::string> > > >
    coverage;

bool compareMessage5Tuple(std::shared_ptr<message_t> message,
                          std::shared_ptr<SPA::Symbol> symbol) {
  assert(symbol->isMessage());
  if (message->destinationIP == symbol->getMessageDestinationIP() &&
      message->protocol == symbol->getMessageProtocol() &&
      message->destinationPort == symbol->getMessageDestinationPort()) {
    if (symbol->getMessageSourceIP() == SPA_ANY_IP &&
        symbol->getMessageSourcePort() == SPA_ANY_PORT) {
      return true;
    } else {
      return message->sourceIP == symbol->getMessageSourceIP() &&
             message->sourcePort == symbol->getMessageSourcePort();
    }
  } else {
    return false;
  }
}

bool checkMessageReceived(std::shared_ptr<message_t> message) {
  // Check if all non size and source symbols were received.
  for (auto sit : message->symbols) {
    if ((!sit.second) && sit.first->getFullName().compare(
                             0, strlen(SPA_MESSAGE_OUTPUT_SIZE_PREFIX),
                             SPA_MESSAGE_OUTPUT_SIZE_PREFIX) != 0 &&
        sit.first->getFullName().compare(
            0, strlen(SPA_MESSAGE_OUTPUT_SOURCE_PREFIX),
            SPA_MESSAGE_OUTPUT_SOURCE_PREFIX) != 0) {
      return false;
    }
  }
  return true;
}

std::string join(std::vector<std::string> strings, std::string delimiter) {
  std::string result;
  for (auto it = strings.begin(), ie = strings.end(); it != ie; it++) {
    result += (it == strings.begin() ? "" : delimiter) + *it;
  }
  return result;
}

std::string remapSrcFileName(std::string srcFileName) {
  for (auto mapping : SrcDirMap) {
    auto d = mapping.find("=");
    assert(d != std::string::npos &&
           "Incorrectly formatted source directory mapping.");
    std::string from = mapping.substr(0, d);
    std::string to = mapping.substr(d + 1);
    if (srcFileName.compare(0, from.length(), from) == 0) {
      std::string old = srcFileName;
      srcFileName.replace(0, from.length(), to);
      if (Debug) {
        klee::klee_message("Mapping source file %s to %s.", old.c_str(),
                           srcFileName.c_str());
      }
      break;
    }
  }
  return srcFileName;
}

std::string sanitizeToken(std::string name) {
  // Sanitize name to make valid token.
  size_t pos;
  while ((pos = name.find_first_of("-.:")) != std::string::npos) {
    name.replace(pos, 1, "_");
  }
  return "node_" + name;
}

std::string sanitizeHTML(std::string text) {
  // Escape special chars.
  size_t pos = 0;
  while ((pos = text.find("&", pos)) != std::string::npos) {
    text.replace(pos, 1, "&amp;");
    pos++;
  }
  pos = 0;
  while ((pos = text.find("<", pos)) != std::string::npos) {
    text.replace(pos, 1, "&lt;");
  }
  pos = 0;
  while ((pos = text.find(">", pos)) != std::string::npos) {
    text.replace(pos, 1, "&gt;");
  }
  return text;
}

std::string escapeChar(unsigned char c) {
  return std::string("&#") + SPA::numToStr((int) c) + ";";
}

void makePathIndex(std::string filePrefix, std::set<std::string> uuids) {
  klee::klee_message("Processing path index %s.", filePrefix.c_str());

  std::ofstream dotFile(Directory + "/" + filePrefix + ".dot");
  std::ofstream htmlFile(Directory + "/" + filePrefix + ".html.inc");
  assert(dotFile.good() && htmlFile.good());

  htmlFile << "    <div id=\"header\">" << std::endl;
  htmlFile << "      <a href=\"index.html\">All Conversations</a>" << std::endl;
  htmlFile << "      <a href=\"paths.html\">All Paths</a>" << std::endl;
  htmlFile << "      <a href=\"coverage.html\">Coverage</a>" << std::endl;
  htmlFile << "    </div>" << std::endl;
  htmlFile << "    <img src='" << filePrefix
           << ".svg' alt='Path Dependency Graph' usemap='#G' />" << std::endl;

  dotFile << "digraph G {" << std::endl;
  dotFile << "  root [label=\"Path 0\"]" << std::endl;

  for (auto it : uuids) {
    dotFile << "  path_" << uuidToId[it] << " [label=\"Path " << uuidToId[it]
            << "\\n" << uuidToParticipant[it] << "\" URL=\"" << it << ".html\"]"
            << std::endl;
    if (parentPath.count(it)) {
      dotFile << "  path_" << uuidToId[parentPath[it]] << " -> path_"
              << uuidToId[it] << std::endl;
    } else {
      dotFile << "  root -> path_" << uuidToId[it] << std::endl;
    }
  }
  dotFile << "}" << std::endl;
}

void processPath(SPA::Path *path, unsigned long pathID) {
  std::map<std::string, std::shared_ptr<participant_t> > participantByName;
  std::map<std::string, std::shared_ptr<participant_t> > participantByBinding;

  std::vector<std::shared_ptr<message_t> > messages;

  klee::klee_message("Processing path %ld.", pathID);

  // Load participant names.
  for (auto it : path->getParticipants()) {
    if (!participantByName.count(it->getName())) {
      if (Debug) {
        klee::klee_message("  Found participant: %s", it->getName().c_str());
      }
      participantByName[it->getName()].reset(new participant_t {
        it->getName(), std::set<std::string>()
      });
      participantByName[API_INPUT_PREFIX + it->getName()]
          .reset(new participant_t {
        API_INPUT_PREFIX + it->getName(), std::set<std::string>()
      });
      participantByName[API_OUTPUT_PREFIX + it->getName()]
          .reset(new participant_t {
        API_OUTPUT_PREFIX + it->getName(), std::set<std::string>()
      });
    }
  }

  // Load participant IP/Ports.
  for (auto sit : path->getSymbolLog()) {
    if (sit->isMessage()) {
      std::string participant = sit->getParticipant();
      std::string binding;
      if (sit->isInput()) {
        binding =
            sit->getMessageDestinationIP() + ":" + sit->getMessageProtocol() +
            ":" + sit->getMessageDestinationPort();
      } else if (sit->isOutput()) {
        binding = sit->getMessageSourceIP() + ":" + sit->getMessageProtocol() +
                  ":" + sit->getMessageSourcePort();
      }

      assert(participantByName.count(participant));

      if (Debug) {
        klee::klee_message("  Participant %s listens on: %s",
                           participant.c_str(), binding.c_str());
      }
      participantByName[participant]->bindings.insert(binding);
      participantByBinding[binding] = participantByName[participant];
    }
  }

  // Add unknown participants.
  unsigned long unknownId = 1;
  for (auto sit : path->getSymbolLog()) {
    if (sit->isMessage() && sit->isOutput()) {
      std::string binding =
          sit->getMessageDestinationIP() + ":" + sit->getMessageProtocol() +
          ":" + sit->getMessageDestinationPort();
      if (!participantByBinding.count(binding)) {
        if (Debug) {
          klee::klee_message("  Found unknown participant on: %s",
                             binding.c_str());
        }
        std::string name = "Unknown-" + SPA::numToStr(unknownId++);
        participantByName[name].reset(new participant_t {
          name, std::set<std::string>({
            binding
          })
        });
        participantByBinding[binding] = participantByName[name];
      }
    }
  }

  // Load messages.
  for (auto sit : path->getSymbolLog()) {
    if (Debug) {
      klee::klee_message("  Symbol: %s", sit->getFullName().c_str());
    }

    std::string participant = sit->getParticipant();
    assert(participantByName.count(participant) &&
           participantByName.count(API_INPUT_PREFIX + participant) &&
           participantByName.count(API_OUTPUT_PREFIX + participant));

    if (sit->isInput()) {
      if (sit->isAPI()) {
        if (Debug) {
          klee::klee_message("  Input API to: %s", participant.c_str());
        }
        messages.emplace_back(new message_t());
        messages.back()->fromParticipant =
            participantByName[API_INPUT_PREFIX + participant];
        messages.back()->toParticipant = participantByName[participant];
        messages.back()->symbols.push_back(std::make_pair(nullptr, sit));
      } else if (sit->isMessage()) {
        bool sent = false;
        for (auto mit : messages) {
          for (auto &msit : mit->symbols) {
            if ((!msit.second) &&
                checkMessageCompatibility(msit.first, sit->getLocalName())) {
              msit.second = sit;
              if (Debug) {
                klee::klee_message("  Received message: %s -> %s",
                                   msit.first->getFullName().c_str(),
                                   sit->getFullName().c_str());
              }
              sent = true;
              break;
            }
          }
          if (sent) {
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
        // Collapse content and size symbols into one message.
        if (sit->getFullName().compare(0, strlen(SPA_API_OUTPUT_SIZE_PREFIX),
                                       SPA_API_OUTPUT_SIZE_PREFIX) == 0) {
          assert(messages.back()->fromParticipant ==
                     participantByName[participant] &&
                 messages.back()->toParticipant ==
                     participantByName[API_OUTPUT_PREFIX + participant]);
          messages.back()->symbols.push_back(std::make_pair(sit, nullptr));
        } else {

          messages.emplace_back(new message_t());
          messages.back()->fromParticipant = participantByName[participant];
          messages.back()->toParticipant =
              participantByName[API_OUTPUT_PREFIX + participant];
          messages.back()->content.insert(messages.back()->content.end(),
                                          sit->getOutputValues().begin(),
                                          sit->getOutputValues().end());
          messages.back()->symbols.push_back(std::make_pair(sit, nullptr));
        }
      } else if (sit->isMessage()) {
        std::string receiverBinding =
            sit->getMessageDestinationIP() + ":" + sit->getMessageProtocol() +
            ":" + sit->getMessageDestinationPort();
        assert(participantByBinding.count(receiverBinding) &&
               "Message sent to unknown participant.");
        if (Debug) {
          klee::klee_message(
              "  Sent message: %s -> %s", participant.c_str(),
              participantByBinding[receiverBinding]->name.c_str());
        }
        // Collapse content size and source symbols into one message.
        if (sit->getFullName().compare(0,
                                       strlen(SPA_MESSAGE_OUTPUT_SIZE_PREFIX),
                                       SPA_MESSAGE_OUTPUT_SIZE_PREFIX) == 0 ||
            sit->getFullName().compare(0,
                                       strlen(SPA_MESSAGE_OUTPUT_SOURCE_PREFIX),
                                       SPA_MESSAGE_OUTPUT_SOURCE_PREFIX) == 0) {
          assert(compareMessage5Tuple(messages.back(), sit));
          messages.back()->symbols.push_back(std::make_pair(sit, nullptr));
        } else {
          if (messages.empty() || sit->getMessageProtocol() == "udp" ||
              (!compareMessage5Tuple(messages.back(), sit))) {
            messages.emplace_back(new message_t());
            messages.back()->fromParticipant = participantByName[participant];
            messages.back()->toParticipant =
                participantByBinding[receiverBinding];
            messages.back()->sourceIP = sit->getMessageSourceIP();
            messages.back()->sourcePort = sit->getMessageSourcePort();
            messages.back()->protocol = sit->getMessageProtocol();
            messages.back()->destinationIP = sit->getMessageDestinationIP();
            messages.back()->destinationPort = sit->getMessageDestinationPort();
          }
          if (sit->getFullName().compare(
                  0, strlen(SPA_MESSAGE_OUTPUT_CONNECT_PREFIX),
                  SPA_MESSAGE_OUTPUT_CONNECT_PREFIX) != 0) {
            messages.back()->content.insert(messages.back()->content.end(),
                                            sit->getOutputValues().begin(),
                                            sit->getOutputValues().end());
          }
          messages.back()->symbols.push_back(std::make_pair(sit, nullptr));
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
  htmlFile << "      <a href=\"#coverage\">Coverage</a>" << std::endl;
  htmlFile << "      <a href=\"#src\">Path Source</a>" << std::endl;
  htmlFile << "      <a href=\"index.html\">All Conversations</a>" << std::endl;
  htmlFile << "    </div>" << std::endl;

  htmlFile << "    <a class='anchor' id='metadata'></a>" << std::endl;
  htmlFile << "    <h1>Path " << pathID << "</h1>" << std::endl;

  htmlFile << "    <h2>Path Meta-data</h2>" << std::endl;
  htmlFile << "    <b>UUID:</b> " << path->getUUID() << "<br /><br />"
           << std::endl;

  htmlFile << "    <b>Lineage:</b><br />" << std::endl;
  htmlFile << "    <ol>" << std::endl;
  std::vector<std::string> conversation;
  for (auto it : path->getParticipants()) {
    conversation.push_back(it->getName());
    htmlFile << "    <li><a href='" << it->getPathUUID() << ".html'>"
             << it->getName() << "</a> (<a href='" << join(conversation, "_")
             << ".html'>conversation</a>)</li>" << std::endl;
  }
  htmlFile << "    </ol><br />" << std::endl;

  htmlFile << "    <b>Children:</b><br />" << std::endl;
  htmlFile << "    <ol>" << std::endl;
  for (auto it : childrenPaths[path->getUUID()]) {
    htmlFile << "    <li><a href='" << it.second << ".html'>" << it.first
             << "</a></li>" << std::endl;
  }
  htmlFile << "    </ol><br />" << std::endl;

  htmlFile << "    <b>Tags:</b><br />" << std::endl;
  htmlFile << "    <table border='1'>" << std::endl;
  htmlFile << "      <tr><th>Key</th><th>Value</th></tr>" << std::endl;
  for (auto it : path->getTags()) {
    htmlFile << "      <tr><td>" << it.first << "</td><td>" << it.second
             << "</td></tr>" << std::endl;
  }
  htmlFile << "    </table><br />" << std::endl;

  htmlFile << "    <a class='anchor' id='messages'></a>" << std::endl;
  htmlFile << "    <h2>Message Log</h2>" << std::endl;
  htmlFile << "    <img src='" << path->getUUID()
           << ".svg' alt='Message Sequence Diagram' usemap='#G' />"
           << std::endl;

  htmlFile << "    <a class='anchor' id='constraints'></a>" << std::endl;
  htmlFile << "    <h2>Symbolic Constraint</h2>" << std::endl;
  htmlFile << "    <b>Input Symbols:</b><br />" << std::endl;
  htmlFile << "    <table border='1'>" << std::endl;
  htmlFile << "      <tr><th>Name</th><th>Instances</th></tr>" << std::endl;
  for (auto it : path->getInputSymbols()) {
    htmlFile << "      <tr>" << std::endl;
    htmlFile << "        <td>" << it.first << "</td>" << std::endl;
    htmlFile << "        <td>" << std::endl;
    for (unsigned long i = 0; i < it.second.size(); i++) {
      htmlFile << "          <a href='#" << it.second[i]->getFullName() << "'>"
               << (i + 1) << "</a>" << std::endl;
    }
    htmlFile << "        </td>" << std::endl;
    htmlFile << "      </tr>" << std::endl;
  }
  htmlFile << "    </table><br />" << std::endl;

  htmlFile << "    <b>Constraints:</b><br />" << std::endl;
  for (auto it : path->getConstraints()) {
    std::string exprStr;
    llvm::raw_string_ostream exprROS(exprStr);
    it->print(exprROS);
    htmlFile << "    " << exprROS.str() << "<br />" << std::endl;
  }
  htmlFile << "    <br />" << std::endl;

  htmlFile << "    <b>Output Symbols:</b><br />" << std::endl;
  htmlFile << "    <table border='1'>" << std::endl;
  htmlFile << "      <tr><th>Name</th><th>Instances</th></tr>" << std::endl;
  for (auto it : path->getOutputSymbols()) {
    htmlFile << "      <tr>" << std::endl;
    htmlFile << "        <td>" << it.first << "</td>" << std::endl;
    htmlFile << "        <td>" << std::endl;
    for (unsigned long i = 0; i < it.second.size(); i++) {
      htmlFile << "          <a href='#" << it.second[i]->getFullName() << "'>"
               << (i + 1) << "</a>" << std::endl;
    }
    htmlFile << "        </td>" << std::endl;
    htmlFile << "      </tr>" << std::endl;
  }
  htmlFile << "    </table><br />" << std::endl;

  dotFile << "digraph G {" << std::endl;
  for (auto pit : participantByName) {
    std::string participantToken = sanitizeToken(pit.first);
    if (pit.first.compare(0, strlen(API_PREFIX), API_PREFIX) == 0) {
      dotFile << "  " << participantToken << " [style=\"invis\"]" << std::endl;
    } else {
      dotFile << "  " << participantToken << " [label=\"" << pit.first << "\\n";
      for (auto bit : pit.second->bindings) {
        dotFile << bit << "\\n";
      }
      dotFile << "\"]" << std::endl;
    }
  }

  for (unsigned mi = 0; mi < messages.size(); mi++) {
    dotFile << "  " << sanitizeToken(messages[mi]->fromParticipant->name)
            << " -> " << sanitizeToken(messages[mi]->toParticipant->name)
            << "[label=\"" << (mi + 1) << "\" arrowhead=\""
            << (checkMessageReceived(messages[mi]) ? "normal" : "dot")
            << "\" href=\"#msg" << (mi + 1) << "\"]" << std::endl;

    htmlFile << "    <div class='box' id='msg" << (mi + 1) << "'>" << std::endl;
    htmlFile << "      <a class='closebutton' href='#'>&#x2715;</a>"
             << std::endl;
    htmlFile << "        <h1>Message " << (mi + 1) << "</h1>" << std::endl;
    if (mi > 0) {
      htmlFile << "      <a href='#msg" << mi
               << "'>&lt;&lt; Previous Message</a>" << std::endl;
    }
    if (mi < messages.size() - 1) {
      htmlFile << "      <a href='#msg" << mi + 2
               << "'>Next Message &gt;&gt;</a>" << std::endl;
    }
    htmlFile << "      <br />" << std::endl;

    htmlFile << "      <div class='content'>" << std::endl;
    if (messages[mi]->content.size() > 0) {
      htmlFile << "        <b>From:</b> " << messages[mi]->sourceIP << ":"
               << messages[mi]->protocol << ":" << messages[mi]->sourcePort
               << " (" << messages[mi]->fromParticipant->name << ")<br />"
               << std::endl;
      htmlFile << "        <b>To:</b> " << messages[mi]->destinationIP << ":"
               << messages[mi]->protocol << ":" << messages[mi]->destinationPort
               << " (" << messages[mi]->toParticipant->name << ")<br />"
               << std::endl;
      htmlFile << "        <b>Size:</b> " << messages[mi]->content.size()
               << "<br />" << std::endl;
      htmlFile << "        <b>Text Representation:</b><br />" << std::endl;
      htmlFile << "        <pre>" << std::endl;
      for (auto it : messages[mi]->content) {
        if (klee::ConstantExpr *ce = llvm::dyn_cast<klee::ConstantExpr>(it)) {
          htmlFile << escapeChar(ce->getLimitedValue());
        } else {
          htmlFile << "&#9608;";
        }
      }
      htmlFile << "        </pre>" << std::endl;
      htmlFile << "        <b>Byte Values:</b><br />" << std::endl;
      htmlFile << "        <table border='1'>" << std::endl;
      htmlFile << "          <tr><th>Index</th><th>Expression</th></tr>"
               << std::endl;
      for (unsigned long si = 0; si < messages[mi]->content.size(); si++) {
        std::string exprStr;
        llvm::raw_string_ostream exprROS(exprStr);
        messages[mi]->content[si]->print(exprROS);
        htmlFile << "          <tr><td>" << si << "</td><td>" << exprROS.str()
                 << "</td></tr>" << std::endl;
      }
      htmlFile << "        </table>" << std::endl;
    }

    htmlFile << "        <b>Symbols:</b><br />" << std::endl;
    htmlFile << "        <table border='1'>" << std::endl;
    htmlFile << "          <tr><th>Sent</th><th>Received</th></tr>"
             << std::endl;
    for (auto it : messages[mi]->symbols) {
      htmlFile << "          <tr><td>"
               << (it.first ? "<a href='#" + it.first->getFullName() + "'>" +
                                  it.first->getFullName() + "</a>"
                            : "") << "</td><td>"
               << (it.second ? "<a href='#" + it.second->getFullName() + "'>" +
                                   it.second->getFullName() + "</a>"
                             : "") << "</td></tr>" << std::endl;
    }
    htmlFile << "        </table>" << std::endl;
    htmlFile << "      </div>" << std::endl;
    htmlFile << "    </div>" << std::endl;
  }
  dotFile << "}" << std::endl;

  for (auto sit : path->getSymbolLog()) {
    htmlFile << "    <div class='box' id='" << sit->getFullName() << "'>"
             << std::endl;
    htmlFile << "      <a class='closebutton' href='#'>&#x2715;</a>"
             << std::endl;
    htmlFile << "      <h1>Symbol " << sit->getFullName() << "</h1>"
             << std::endl;
    htmlFile << "      <div class='content'>" << std::endl;
    if (sit->isInput()) {
      htmlFile << "      <b>Type:</b> input<br />" << std::endl;
      htmlFile << "        <b>Size:</b> " << sit->getInputArray()->size
               << "<br />" << std::endl;
      if (path->getTestInputs().count(sit->getFullName())) {
        std::stringstream value;
        copy(path->getTestInput(sit->getFullName()).begin(),
             path->getTestInput(sit->getFullName()).end(),
             std::ostream_iterator<int>(value, " "));
        htmlFile << "        <b>Test Case Value:</b> " << value.str()
                 << "<br />" << std::endl;
      }
    } else if (sit->isOutput()) {
      htmlFile << "        <b>Type:</b> output<br />" << std::endl;
      htmlFile << "        <b>Size:</b> " << sit->getOutputValues().size()
               << "<br />" << std::endl;
      htmlFile << "        <b>Text Representation:</b><br />" << std::endl;
      htmlFile << "        <pre>" << std::endl;
      for (auto it : sit->getOutputValues()) {
        if (klee::ConstantExpr *ce = llvm::dyn_cast<klee::ConstantExpr>(it)) {
          htmlFile << escapeChar(ce->getLimitedValue());
        } else {
          htmlFile << "&#9608;";
        }
      }
      htmlFile << "        </pre>" << std::endl;
      htmlFile << "        <b>Byte Values:</b><br />" << std::endl;
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
    } else {
      assert(false && "Unknown symbol type.");
    }
    htmlFile << "      </div>" << std::endl;
    htmlFile << "    </div>" << std::endl;
  }

  htmlFile << "    <a class='anchor' id='coverage'></a>" << std::endl;
  htmlFile << "    <h2>Coverage</h2>" << std::endl;
  htmlFile << "    <b>Files:</b><br />" << std::endl;
  for (auto it : path->getExploredLineCoverage()) {
    std::string srcFileName = remapSrcFileName(it.first);
    htmlFile << "    <a href='#" << srcFileName << "'>" << srcFileName
             << "</a><br />" << std::endl;
  }

  for (auto fit : path->getExploredLineCoverage()) {
    std::string srcFileName = remapSrcFileName(fit.first);
    htmlFile << "    <div class='box' id='" << srcFileName << "'>" << std::endl;
    htmlFile << "      <a class='closebutton' href='#'>&#x2715;</a>"
             << std::endl;
    htmlFile << "      <b>" << srcFileName << "</b>" << std::endl;
    htmlFile << "      <div class='content'>" << std::endl;
    htmlFile << "        <div class='src'>" << std::endl;
    std::ifstream srcFile(srcFileName);
    unsigned long lastLineNum =
        fit.second.empty() ? 0 : fit.second.rbegin()->first;
    for (unsigned long srcLineNum = 1;
         srcFile.good() || srcLineNum <= lastLineNum; srcLineNum++) {
      std::string srcLine;
      std::getline(srcFile, srcLine);
      htmlFile << "          <div class='line "
               << (fit.second.count(srcLineNum)
                       ? (fit.second[srcLineNum] ? "covered" : "uncovered")
                       : "unknown") << "'>" << std::endl;
      if (fit.second.count(srcLineNum)) {
        htmlFile << "<a href='coverage.html#" << srcFileName << ":"
                 << srcLineNum << "'>" << std::endl;
      }
      htmlFile << "            <div class='number'>" << srcLineNum << "</div>"
               << std::endl;
      htmlFile << "            <div class='content'>" << sanitizeHTML(srcLine)
               << "</div>" << std::endl;
      if (fit.second.count(srcLineNum)) {
        htmlFile << "          </a>" << std::endl;
      }
      htmlFile << "          </div>" << std::endl;
    }
    htmlFile << "        </div>" << std::endl;
    htmlFile << "      </div>" << std::endl;
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

  klee::klee_message("Generating static content.");

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
  makefile << "all: index paths" << std::endl;
  makefile << "index: index.html index.svg" << std::endl;
  makefile << "paths: paths.html paths.svg" << std::endl;
  makefile << "clean: index.clean paths.clean" << std::endl;

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
  cssFile << "  position: fixed;" << std::endl;
  cssFile << "  left: 0;" << std::endl;
  cssFile << "  right: 0;" << std::endl;
  cssFile << "  top: 0;" << std::endl;
  cssFile << "  margin-left: auto;" << std::endl;
  cssFile << "  margin-right: auto;" << std::endl;
  cssFile << "  margin-top: 50px;" << std::endl;
  cssFile << "  max-width: 90vw;" << std::endl;
  cssFile << "  border-radius: 10px;" << std::endl;
  cssFile << "  padding: 10px;" << std::endl;
  cssFile << "  border: 5px solid black;" << std::endl;
  cssFile << "  background-color: white;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.box:target {" << std::endl;
  cssFile << "  display: table;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.box div.content {" << std::endl;
  cssFile << "  max-height: calc(100vh - 200px);" << std::endl;
  cssFile << "  overflow-y: scroll;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src {" << std::endl;
  cssFile << "  width: 700px;" << std::endl;
  cssFile << "  line-height: 100%;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src div.line {" << std::endl;
  cssFile << "  clear: left;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src div.line a {" << std::endl;
  cssFile << "  text-decoration: none;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src div.line div.number {" << std::endl;
  cssFile << "  width: 50px;" << std::endl;
  cssFile << "  float: left;" << std::endl;
  cssFile << "  text-align: right;" << std::endl;
  cssFile << "  padding-right: 5px;" << std::endl;
  cssFile << "  background-color: lightgray;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src div.line div.content {" << std::endl;
  cssFile << "  width: 645px;" << std::endl;
  cssFile << "  overflow: hidden;" << std::endl;
  cssFile << "  unicode-bidi: embed;" << std::endl;
  cssFile << "  font-family: monospace;" << std::endl;
  cssFile << "  white-space: pre;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src div.covered div.content {" << std::endl;
  cssFile << "  background-color: lightgreen;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "div.src div.uncovered div.content {" << std::endl;
  cssFile << "  background-color: lightcoral;" << std::endl;
  cssFile << "}" << std::endl;
  cssFile << "a.closebutton {" << std::endl;
  cssFile << "  position: absolute;" << std::endl;
  cssFile << "  top: 3px;" << std::endl;
  cssFile << "  right: 8px;" << std::endl;
  cssFile << "  font-size: 20pt;" << std::endl;
  cssFile << "}" << std::endl;

  klee::klee_message("Loading paths.");

  std::unique_ptr<SPA::PathLoader> pathLoader(new SPA::PathLoader(inFile));
  std::unique_ptr<SPA::Path> path;
  std::set<std::string> allUUIDs;

  for (unsigned long pathID = 1; path.reset(pathLoader->getPath()), path;
       pathID++) {
    uuidToId[path->getUUID()] = pathID;
    uuidToParticipant[path->getUUID()] =
        path->getParticipants().back()->getName();
    allUUIDs.insert(path->getUUID());

    std::vector<std::string> participants;
    for (auto pit : path->getParticipants()) {
      participants.push_back(pit->getName());
    }
    conversations[participants].insert(path->getUUID());

    if (path->getParticipants().size() > 1) {
      auto parent = path->getParticipants()[path->getParticipants().size() - 2];
      auto child = path->getParticipants()[path->getParticipants().size() - 1];
      parentPath[child->getPathUUID()] = parent->getPathUUID();
      childrenPaths[parent->getPathUUID()]
          .insert(std::make_pair(child->getName(), child->getPathUUID()));
    }

    for (auto fit : path->getExploredLineCoverage()) {
      for (auto lit : fit.second) {
        coverage[fit.first][lit.first][lit.second].insert(path->getUUID());
      }
    }

    makefile << "all: " << path->getUUID() << std::endl;
    makefile << path->getUUID() << ": " << path->getUUID() << ".html "
             << path->getUUID() << ".svg" << std::endl;
    makefile << "clean: " << path->getUUID() << ".clean" << std::endl;
  }

  makePathIndex("paths", allUUIDs);

  klee::klee_message("Generating conversation index.");

  std::ofstream conversationsDot(Directory + "/index.dot");
  std::ofstream conversationsHtml(Directory + "/index.html.inc");
  assert(conversationsDot.good() && conversationsHtml.good());

  conversationsHtml << "    <div id=\"header\">" << std::endl;
  conversationsHtml << "      <a href=\"index.html\">All Conversations</a>"
                    << std::endl;
  conversationsHtml << "      <a href=\"paths.html\">All Paths</a>"
                    << std::endl;
  conversationsHtml << "      <a href=\"coverage.html\">Coverage</a>"
                    << std::endl;
  conversationsHtml << "    </div>" << std::endl;
  conversationsHtml << "    Documented " << uuidToId.size() << " paths, in "
                    << conversations.size() << " conversations.<br />"
                    << std::endl;
  conversationsHtml << "    <img src='index.svg' alt='Conversation Dependency "
                       "Graph' usemap='#G' />" << std::endl;

  conversationsDot << "digraph G {" << std::endl;
  conversationsDot << "  root [label=\"Root\"]" << std::endl;

  for (auto cit : conversations) {
    std::string name = join(cit.first, "_");
    auto fullConversation = cit.second;
    // Add all parent paths to conversation.
    for (auto pit : cit.second) {
      if (parentPath.count(pit)) {
        std::string parent = parentPath[pit];
        while (!fullConversation.count(parent)) {
          fullConversation.insert(parent);
          if (parentPath.count(parent)) {
            parent = parentPath[parent];
          } else {
            break;
          }
        }
      }
    }

    conversationsDot << "  " << sanitizeToken(name) << " [label=\""
                     << cit.first.back() << "\\n(" << cit.second.size()
                     << " Paths)\" URL=\"" << name << ".html\"]" << std::endl;
    if (cit.first.size() > 1) {
      auto parentConversation = cit.first;
      parentConversation.pop_back();

      conversationsDot << "  " << sanitizeToken(join(parentConversation, "_"))
                       << " -> " << sanitizeToken(name) << std::endl;
    } else {
      conversationsDot << "  root -> " << sanitizeToken(name) << std::endl;
    }

    makePathIndex(name, fullConversation);
    makefile << "all: " << name << std::endl;
    makefile << name << ": " << name << ".html " << name << ".svg" << std::endl;
    makefile << "clean: " << name << ".clean" << std::endl;
  }
  conversationsDot << "}" << std::endl;

  if (NumProcesses == 1 || fork() <= 0) {
    klee::klee_message("Generating global coverage index.");

    std::ofstream coverageHtml(Directory + "/coverage.html");
    assert(coverageHtml.good());
    coverageHtml << "<!doctype html>" << std::endl;
    coverageHtml << "<html lang=\"en\">" << std::endl;
    coverageHtml << "  <head>" << std::endl;
    coverageHtml << "    <meta charset=\"utf-8\">" << std::endl;
    coverageHtml << "    <title>Coverage</title>" << std::endl;
    coverageHtml
        << "    <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">"
        << std::endl;
    coverageHtml << "  </head>" << std::endl;
    coverageHtml << "  <body>" << std::endl;
    coverageHtml << "    <div id=\"header\">" << std::endl;
    coverageHtml << "      <a href=\"index.html\">All Conversations</a>"
                 << std::endl;
    coverageHtml << "      <a href=\"paths.html\">All Paths</a>" << std::endl;
    coverageHtml << "      <a href=\"coverage.html\">Coverage</a>" << std::endl;
    coverageHtml << "    </div>" << std::endl;
    coverageHtml << "    <h1>Coverage</h1>" << std::endl;
    coverageHtml << "    <b>Files:</b><br />" << std::endl;
    for (auto it : coverage) {
      std::string srcFileName = remapSrcFileName(it.first);
      coverageHtml << "    <a href='#" << srcFileName << "'>" << srcFileName
                   << "</a><br />" << std::endl;
    }

    for (auto fit : coverage) {
      std::string srcFileName = remapSrcFileName(fit.first);
      coverageHtml << "    <div class='box' id='" << srcFileName << "'>"
                   << std::endl;
      coverageHtml << "      <a class='closebutton' href='#'>&#x2715;</a>"
                   << std::endl;
      coverageHtml << "      <b>" << srcFileName << "</b>" << std::endl;
      coverageHtml << "      <div class='content'>" << std::endl;
      coverageHtml << "        <div class='src'>" << std::endl;
      std::ifstream srcFile(srcFileName);
      unsigned long lastLineNum =
          fit.second.empty() ? 0 : fit.second.rbegin()->first;
      for (unsigned long srcLineNum = 1;
           srcFile.good() || srcLineNum <= lastLineNum; srcLineNum++) {
        std::string srcLine;
        std::getline(srcFile, srcLine);
        coverageHtml << "          <div class='line'>" << std::endl;
        if (fit.second.count(srcLineNum)) {
          coverageHtml << "<a href='coverage.html#" << srcFileName << ":"
                       << srcLineNum << "'>" << std::endl;
        }
        coverageHtml << "            <div class='number'>" << srcLineNum
                     << "</div>" << std::endl;
        coverageHtml << "            <div class='content'>"
                     << sanitizeHTML(srcLine) << "</div>" << std::endl;
        if (fit.second.count(srcLineNum)) {
          coverageHtml << "          </a>" << std::endl;
        }
        coverageHtml << "          </div>" << std::endl;
      }
      coverageHtml << "        </div>" << std::endl;
      coverageHtml << "      </div>" << std::endl;
      coverageHtml << "    </div>" << std::endl;
    }

    for (auto fit : coverage) {
      std::string srcFileName = remapSrcFileName(fit.first);
      for (auto lit : fit.second) {
        coverageHtml << "    <div class='box' id='" << srcFileName << ":"
                     << lit.first << "'>" << std::endl;
        coverageHtml << "      <a class='closebutton' href='#'>&#x2715;</a>"
                     << std::endl;
        coverageHtml << "      <h2>" << srcFileName << ":" << lit.first
                     << "</h2>" << std::endl;

        coverageHtml << "      <b>Other paths that reached here:</b><br />"
                     << std::endl;
        unsigned long counter = 1;
        for (auto pit : lit.second[true]) {
          coverageHtml << "      <a href='" << pit << ".html'>" << counter++
                       << "</a>" << std::endl;
        }
        coverageHtml << "      <br />" << std::endl;

        coverageHtml << "      <b>Other paths that didn't reach here:</b><br />"
                     << std::endl;
        counter = 1;
        for (auto pit : lit.second[false]) {
          coverageHtml << "      <a href='" << pit << ".html'>" << counter++
                       << "</a>" << std::endl;
        }
        coverageHtml << "      <br />" << std::endl;

        coverageHtml << "    </div>" << std::endl;
      }
    }
    coverageHtml << "  </body>" << std::endl;
    coverageHtml << "</html>" << std::endl;

    if (NumProcesses != 1) {
      return 0;
    }
  }

  if (OnlyPathID) {
    path.reset(pathLoader->getPath(OnlyPathID - 1));
    assert(path && "Specified path does not exist.");
    processPath(path.get(), OnlyPathID);
  } else {
    int workerID;
    for (workerID = 0; workerID < NumProcesses - 1; workerID++) {
      if (fork() == 0) {
        break;
      }
    }
    klee::klee_message("Running worker %d of %d.", workerID + 1,
                       NumProcesses + 0);

    inFile.close();
    inFile.open(InFileName);
    pathLoader.reset(new SPA::PathLoader(inFile));
    assert(pathLoader->skipPaths(StartFromPathID - 1) &&
           "Specified path does not exist.");
    for (unsigned long pathID = StartFromPathID;
         path.reset(pathLoader->getPath()), path; pathID++) {
      if ((int) pathID % NumProcesses == workerID) {
        processPath(path.get(), pathID);
      }
    }
  }

  // Wait for all children to finish.
  while (wait(nullptr)) {
    if (errno == ECHILD) {
      break;
    }
  }

  return 0;
}
