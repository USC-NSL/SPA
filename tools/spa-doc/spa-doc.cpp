#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"
#include "mongoose.h"

#include <spa/SPA.h>
#include <spa/PathLoader.h>
#include <spa/FilterExpression.h>
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
llvm::cl::opt<bool> Debug("d", llvm::cl::desc("Enable debug messages."));

llvm::cl::opt<std::string> Directory(
    "dir", llvm::cl::init("."),
    llvm::cl::desc(
        "The directory to place the generated documentation in (default: .)."));

llvm::cl::opt<int>
    NumProcesses("j", llvm::cl::init(1),
                 llvm::cl::desc("Number of worker processes to spawn."));

llvm::cl::list<std::string> SrcDirMap(
    "map-src",
    llvm::cl::desc(
        "Remaps source directories when searching for source code files."));

llvm::cl::list<std::string> ColorFilter(
    "color-filter",
    llvm::cl::desc("Format: 'color:filter'. "
                   "Annotates paths that match filter with the given color."));

llvm::cl::opt<int>
    ServeHttp("serve-http", llvm::cl::init(0),
              llvm::cl::desc("Serve documentation via HTTP on given port, "
                             "instead of generating static files."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));
}

#define DOT_CMD "unflatten | dot -Tsvg"

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

std::set<SPA::Path *> allPaths;
// uuid -> path
std::map<std::string, SPA::Path *> pathsByUUID;
// path -> path-id.
std::map<SPA::Path *, unsigned long> pathIDs;
// participants -> paths
std::map<std::vector<std::string>, std::set<SPA::Path *> > conversations;
// uuid -> [participant, uuid]s.
std::map<SPA::Path *, std::set<SPA::Path *> > childrenPaths;
// filter -> color
std::set<std::pair<SPA::FilterExpression *, std::string> > colorFilters;
// path -> colors
std::map<SPA::Path *, std::set<std::string> > pathColors;
// participants -> colors
std::map<std::vector<std::string>, std::set<std::string> > conversationColors;
// file -> (line -> (covered -> paths))
std::map<std::string,
         std::map<unsigned long, std::map<bool, std::set<SPA::Path *> > > >
    coverage;
// src file -> index file
std::map<std::string, std::string> coverageIndexes;
// file -> generating function
std::map<std::string, std::function<std::string()> > files;

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

bool checkMessageDropped(std::shared_ptr<message_t> message) {
  // Check if any of the sender content symbols were dropped.
  for (auto sit : message->symbols) {
    if (sit.first && sit.first->isDropped()) {
      return true;
    }
  }
  return false;
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

std::string generatePathDot(std::set<SPA::Path *> paths,
                            std::set<SPA::Path *> selectedPaths =
                                std::set<SPA::Path *>(),
                            std::string direction = "TB") {
  std::string dot = "digraph G {\n"
                    "  rankdir = \"" + direction + "\"\n";
  dot += "  root [label=\"Path 0\"]\n";

  for (auto it : paths) {
    std::string pathID = SPA::numToStr(pathIDs[it]);
    std::vector<std::string> colors(pathColors[it].begin(),
                                    pathColors[it].end());
    dot += "  path_" + pathID + " [label=\"Path " + pathID + "\\n" +
           it->getParticipants().back()->getName() + "\" tooltip=\"" +
           it->getUUID() + "\" URL=\"" + it->getUUID() + ".html\" style=\"" +
           (colors.size() > 1 ? "wedged" : "filled") + "\" fillcolor=\"" +
           SPA::strJoin(colors, ":") + "\" penwidth=\"" +
           (selectedPaths.count(it) ? "3" : "1") + "\"]\n";

    if (it->getParticipants().size() > 1) {
      dot +=
          "  path_" +
          SPA::numToStr(pathIDs[
              pathsByUUID[it->getParticipants().rbegin()[1]->getPathUUID()]]) +
          " -> path_" + pathID + "\n";
    } else {
      dot += "  root -> path_" + pathID + "\n";
    }

    if (!it->getDerivedFromUUID().empty() &&
        paths.count(pathsByUUID[it->getDerivedFromUUID()])) {
      dot += "  path_" +
             SPA::numToStr(pathIDs[pathsByUUID[it->getDerivedFromUUID()]]) +
             " -> path_" + pathID + " [style=\"dashed\" constraint=false]\n";
    }
  }
  dot += "}\n";

  return dot;
}

std::string generatePathIndex(std::set<SPA::Path *> paths,
                              std::set<SPA::Path *> selectedPaths =
                                  std::set<SPA::Path *>()) {
  klee::klee_message("Processing path index.");

  std::string dot = generatePathDot(paths, selectedPaths);

  return "<!doctype html>\n"
         "<html lang=\"en\">\n"
         "  <head>\n"
         "    <meta charset=\"utf-8\">\n"
         "    <title>Paths in conversation</title>\n"
         "    <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
         "  </head>\n"
         "  <body>\n"
         "    <div id=\"header\">\n"
         "      <a href=\"index.html\">All Conversations</a>\n"
         "      <a href=\"paths.html\">All Paths</a>\n"
         "      <a href=\"coverage.html\">Coverage</a>\n"
         "    </div>\n" + SPA::runCommand(DOT_CMD, dot) + "\n"
                                                          "  </body>\n"
                                                          "</html>\n";
}

std::string generatePathHTML(SPA::Path *path) {
  std::map<std::string, std::shared_ptr<participant_t> > participantByName;
  std::map<std::string, std::shared_ptr<participant_t> > participantByBinding;

  std::vector<std::shared_ptr<message_t> > messages;

  klee::klee_message("Processing path %ld (%s).", pathIDs[path],
                     path->getUUID().c_str());

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

  std::string messageLogDot = "digraph G {\n";
  for (auto pit : participantByName) {
    std::string participantToken = sanitizeToken(pit.first);
    if (pit.first.compare(0, strlen(API_PREFIX), API_PREFIX) == 0) {
      messageLogDot += "  " + participantToken + " [style=\"invis\"]\n";
    } else {
      messageLogDot +=
          "  " + participantToken + " [label=\"" + pit.first + "\\n";
      for (auto bit : pit.second->bindings) {
        messageLogDot += bit + "\\n";
      }
      messageLogDot += "\"]\n";
    }
  }

  for (unsigned mi = 0; mi < messages.size(); mi++) {
    std::string messageLabel = SPA::numToStr(mi + 1);
    messageLogDot +=
        "  " + sanitizeToken(messages[mi]->fromParticipant->name) + " -> " +
        sanitizeToken(messages[mi]->toParticipant->name) + "[label=\"" +
        messageLabel + "\" arrowhead=\"" +
        (checkMessageDropped(messages[mi])
             ? "tee"
             : (checkMessageReceived(messages[mi]) ? "normal" : "dot")) +
        "\" href=\"#msg" + messageLabel + "\"]\n";
  }
  messageLogDot += "}\n";

  std::set<SPA::Path *> fullConversation, worklist;
  // Show entire conversation up to the children of the current path.
  worklist = childrenPaths[path];
  worklist.insert(path);
  while (!worklist.empty()) {
    SPA::Path *path = *worklist.begin();
    worklist.erase(path);
    if (path && !fullConversation.count(path)) {
      fullConversation.insert(path);
      if (path->getParticipants().size() > 1) {
        worklist.insert(
            pathsByUUID[path->getParticipants().rbegin()[1]->getPathUUID()]);
      }
      if (!path->getDerivedFromUUID().empty()) {
        worklist.insert(pathsByUUID[path->getDerivedFromUUID()]);
      }
    }
  }
  std::string lineageDot =
      generatePathDot(fullConversation, std::set<SPA::Path *>({
    path
  }),
                      "LR");

  std::string htmlFile =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"utf-8\">\n"
      "    <title>Path " + SPA::numToStr(pathIDs[path]) + "</title>\n";
  htmlFile +=
      "    <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"header\">\n"
      "      <a href=\"#metadata\">Path Meta-data</a>\n"
      "      <a href=\"#messages\">Message Log</a>\n"
      "      <a href=\"#constraints\">Symbolic Contraints</a>\n"
      "      <a href=\"#coverage\">Coverage</a>\n"
      "      <a href=\"#src\">Path Source</a>\n"
      "      <a href=\"index.html\">All Conversations</a>\n"
      "    </div>\n"
      "    <a class='anchor' id='metadata'></a>\n"
      "    <h1>Path " + SPA::numToStr(pathIDs[path]) +
      "</h1>\n"
      "    <h2>Path Meta-data</h2>\n"
      "    <b>UUID:</b> " + path->getUUID() + "<br /><br />\n";
  htmlFile += "    <b>Lineage:</b><br />\n" +
              SPA::runCommand(DOT_CMD, lineageDot) + "\n";
  htmlFile += "    <br /><br />\n"
              "    <b>Conversation:</b><br />\n"
              "    <ol>\n";
  std::vector<std::string> conversation;
  for (auto it : path->getParticipants()) {
    conversation.push_back(it->getName());
    htmlFile += "    <li><a href='" + SPA::strJoin(conversation, "_") +
                ".html'>" + it->getName() + "</a></li>\n";
  }
  htmlFile += "    </ol><br />\n"
              "    <b>Tags:</b><br />\n"
              "    <table border='1'>\n"
              "      <tr><th>Key</th><th>Value</th></tr>\n";
  for (auto it : path->getTags()) {
    htmlFile +=
        "      <tr><td>" + it.first + "</td><td>" + it.second + "</td></tr>\n";
  }
  htmlFile +=
      "    </table><br />\n"
      "    <a class='anchor' id='messages'></a>\n"
      "    <h2>Message Log</h2>\n" + SPA::runCommand(DOT_CMD, messageLogDot) +
      "\n"
      "    <a class='anchor' id='constraints'></a>\n"
      "    <h2>Symbolic Constraint</h2>\n"
      "    <b>Input Symbols:</b><br />\n"
      "    <table border='1'>\n"
      "      <tr><th>Name</th><th>Instances</th></tr>\n";
  for (auto it : path->getInputSymbols()) {
    htmlFile += "      <tr>\n"
                "        <td>" + it.first + "</td>\n"
                                            "        <td>\n";
    for (unsigned long i = 0; i < it.second.size(); i++) {
      htmlFile += "          <a href='#" + it.second[i]->getFullName() + "'>" +
                  SPA::numToStr(i + 1) + "</a>\n";
    }
    htmlFile += "        </td>\n"
                "      </tr>\n";
  }
  htmlFile += "    </table><br />\n"
              "    <b>Constraints:</b><br />\n";
  for (auto it : path->getConstraints()) {
    std::string exprStr;
    llvm::raw_string_ostream exprROS(exprStr);
    it->print(exprROS);
    htmlFile += "    " + exprROS.str() + "<br />\n";
  }
  htmlFile += "    <br />\n"
              "    <b>Output Symbols:</b><br />\n"
              "    <table border='1'>\n"
              "      <tr><th>Name</th><th>Instances</th></tr>\n";
  for (auto it : path->getOutputSymbols()) {
    htmlFile += "      <tr>\n"
                "        <td>" + it.first + "</td>\n"
                                            "        <td>\n";
    for (unsigned long i = 0; i < it.second.size(); i++) {
      htmlFile += "          <a href='#" + it.second[i]->getFullName() + "'>" +
                  SPA::numToStr(i + 1) + "</a>\n";
    }
    htmlFile += "        </td>\n"
                "      </tr>\n";
  }
  htmlFile += "    </table><br />\n";

  for (unsigned mi = 0; mi < messages.size(); mi++) {
    htmlFile += "    <div class='box' id='msg" + SPA::numToStr(mi + 1) +
                "'>\n"
                "      <a class='closebutton' href='#'>&#x2715;</a>\n"
                "        <h1>Message " + SPA::numToStr(mi + 1) + "</h1>\n";
    if (mi > 0) {
      htmlFile += "      <a href='#msg" + SPA::numToStr(mi) +
                  "'>&lt;&lt; Previous Message</a>\n";
    }
    if (mi < messages.size() - 1) {
      htmlFile += "      <a href='#msg" + SPA::numToStr(mi + 2) +
                  "'>Next Message &gt;&gt;</a>\n";
    }
    htmlFile += "      <br />\n"
                "      <div class='content'>\n";
    if (messages[mi]->content.size() > 0) {
      htmlFile +=
          "        <b>From:</b> " + messages[mi]->sourceIP + ":" +
          messages[mi]->protocol + ":" + messages[mi]->sourcePort + " (" +
          messages[mi]->fromParticipant->name + ")<br />\n"
                                                "        <b>To:</b> " +
          messages[mi]->destinationIP + ":" + messages[mi]->protocol + ":" +
          messages[mi]->destinationPort + " (" +
          messages[mi]->toParticipant->name + ")<br />\n"
                                              "        <b>Status:</b> " +
          (checkMessageDropped(messages[mi])
               ? "Dropped"
               : (checkMessageReceived(messages[mi]) ? "Received"
                                                     : "In Flight")) +
          "<br />\n"
          "        <b>Size:</b> " +
          SPA::numToStr(messages[mi]->content.size()) +
          "<br />\n"
          "        <b>Text Representation:</b><br />\n"
          "        <pre>\n";
      for (auto it : messages[mi]->content) {
        if (klee::ConstantExpr *ce = llvm::dyn_cast<klee::ConstantExpr>(it)) {
          htmlFile += escapeChar(ce->getLimitedValue());
        } else {
          htmlFile += "&#9608;";
        }
      }
      htmlFile += "        </pre>\n"
                  "        <b>Byte Values:</b><br />\n"
                  "        <table border='1'>\n"
                  "          <tr><th>Index</th><th>Expression</th></tr>\n";
      for (unsigned long si = 0; si < messages[mi]->content.size(); si++) {
        std::string exprStr;
        llvm::raw_string_ostream exprROS(exprStr);
        messages[mi]->content[si]->print(exprROS);
        htmlFile += "          <tr><td>" + SPA::numToStr(si) + "</td><td>" +
                    exprROS.str() + "</td></tr>\n";
      }
      htmlFile += "        </table>\n";
    }

    htmlFile += "        <b>Symbols:</b><br />\n"
                "        <table border='1'>\n"
                "          <tr><th>Sent</th><th>Received</th></tr>\n";
    for (auto it : messages[mi]->symbols) {
      htmlFile += "          <tr><td>" +
                  (it.first ? "<a href='#" + it.first->getFullName() + "'>" +
                                  it.first->getFullName() + "</a>"
                            : "") + "</td><td>" +
                  (it.second ? "<a href='#" + it.second->getFullName() + "'>" +
                                   it.second->getFullName() + "</a>"
                             : "") + "</td></tr>\n";
    }
    htmlFile += "        </table>\n"
                "      </div>\n"
                "    </div>\n";
  }

  for (auto sit : path->getSymbolLog()) {
    htmlFile += "    <div class='box' id='" + sit->getFullName() +
                "'>\n"
                "      <a class='closebutton' href='#'>&#x2715;</a>\n"
                "      <h1>Symbol " + sit->getFullName() +
                "</h1>\n"
                "      <div class='content'>\n";
    if (sit->isInput()) {
      htmlFile += "      <b>Type:</b> input<br />\n"
                  "        <b>Size:</b> " +
                  SPA::numToStr(sit->getInputArray()->size) + "<br />\n";
      if (path->getTestInputs().count(sit->getFullName())) {
        std::stringstream value;
        copy(path->getTestInput(sit->getFullName()).begin(),
             path->getTestInput(sit->getFullName()).end(),
             std::ostream_iterator<int>(value, " "));
        htmlFile +=
            "        <b>Test Case Value:</b> " + value.str() + "<br />\n";
      }
    } else if (sit->isOutput()) {
      htmlFile += "        <b>Type:</b> output<br />\n"
                  "        <b>Size:</b> " +
                  SPA::numToStr(sit->getOutputValues().size()) +
                  "<br />\n"
                  "        <b>Text Representation:</b><br />\n"
                  "        <pre>\n";
      for (auto it : sit->getOutputValues()) {
        if (klee::ConstantExpr *ce = llvm::dyn_cast<klee::ConstantExpr>(it)) {
          htmlFile += escapeChar(ce->getLimitedValue());
        } else {
          htmlFile += "&#9608;";
        }
      }
      htmlFile += "        </pre>\n"
                  "        <b>Byte Values:</b><br />\n"
                  "        <table border='1'>\n"
                  "          <tr><th>Index</th><th>Expression</th></tr>\n";
      for (unsigned long i = 0; i < sit->getOutputValues().size(); i++) {
        std::string exprStr;
        llvm::raw_string_ostream exprROS(exprStr);
        sit->getOutputValues()[i]->print(exprROS);
        htmlFile += "          <tr><td>" + SPA::numToStr(i) + "</td><td>" +
                    exprROS.str() + "</td></tr>\n";
      }
      htmlFile += "        </table>\n";
    } else {
      assert(false && "Unknown symbol type.");
    }
    htmlFile += "      </div>\n"
                "    </div>\n";
  }

  htmlFile += "    <a class='anchor' id='coverage'></a>\n"
              "    <h2>Coverage</h2>\n"
              "    <b>Files:</b><br />\n";
  for (auto it : path->getExploredLineCoverage()) {
    std::string srcFileName = remapSrcFileName(it.first);
    htmlFile +=
        "    <a href='#" + srcFileName + "'>" + srcFileName + "</a><br />\n";
  }

  for (auto fit : path->getExploredLineCoverage()) {
    std::string srcFileName = remapSrcFileName(fit.first);
    htmlFile += "    <div class='box' id='" + srcFileName +
                "'>\n"
                "      <a class='closebutton' href='#'>&#x2715;</a>\n"
                "      <b>" + srcFileName + "</b>\n"
                                            "      <div class='content'>\n"
                                            "        <div class='src'>\n";
    std::ifstream srcFile(srcFileName);
    unsigned long lastLineNum =
        fit.second.empty() ? 0 : fit.second.rbegin()->first;
    for (unsigned long srcLineNum = 1;
         srcFile.good() || srcLineNum <= lastLineNum; srcLineNum++) {
      std::string srcLine;
      std::getline(srcFile, srcLine);
      htmlFile += "          <div class='line ";
      htmlFile += (fit.second.count(srcLineNum)
                       ? (fit.second[srcLineNum] ? "covered" : "uncovered")
                       : "unknown");
      htmlFile += "'>\n";
      if (fit.second.count(srcLineNum)) {
        htmlFile += "<a href='" + coverageIndexes[fit.first] + "#l" +
                    SPA::numToStr(srcLineNum) + "'>\n";
      }
      htmlFile +=
          "            <div class='number'>" + SPA::numToStr(srcLineNum) +
          "</div>\n"
          "            <div class='content'>" + sanitizeHTML(srcLine) +
          "</div>\n";
      if (fit.second.count(srcLineNum)) {
        htmlFile += "          </a>\n";
      }
      htmlFile += "          </div>\n";
    }
    htmlFile += "        </div>\n"
                "      </div>\n"
                "    </div>\n";
  }

  htmlFile += "    <a class='anchor' id='src'></a>\n"
              "    <h2>Path Source</h2>\n"
              "<pre>\n" + path->getPathSource() + "</pre>\n"
                                                  "  </body>\n"
                                                  "</html>\n";

  return htmlFile;
}

std::string generateCSS() {
  return "body {\n"
         "  padding-top: 50px;\n"
         "}\n"
         "a.anchor {\n"
         "  display: block;\n"
         "  position: relative;\n"
         "  top: -50px;\n"
         "  visibility: hidden;\n"
         "}\n"
         "div#header {\n"
         "  position: fixed;\n"
         "  width: 100%;\n"
         "  top: 0; \n"
         "  left: 0; \n"
         "  margin: 0;\n"
         "  padding: 0;\n"
         "  background-color: lightgray;\n"
         "  display: table;\n"
         "  table-layout: fixed;\n"
         "}\n"
         "div#header a {\n"
         "  display: table-cell;\n"
         "  padding: 14px 16px;\n"
         "  text-align: center;\n"
         "  vertical-align: middle;\n"
         "  text-decoration: none;\n"
         "  font-weight: bold;\n"
         "  color: black;\n"
         "}\n"
         "div#header a:hover {\n"
         "  background-color: black;\n"
         "  color: white;\n"
         "}\n"
         "div.box {\n"
         "  display: none;\n"
         "  position: fixed;\n"
         "  left: 0;\n"
         "  right: 0;\n"
         "  top: 0;\n"
         "  margin-left: auto;\n"
         "  margin-right: auto;\n"
         "  margin-top: 50px;\n"
         "  max-width: 90vw;\n"
         "  border-radius: 10px;\n"
         "  padding: 10px;\n"
         "  border: 5px solid black;\n"
         "  background-color: white;\n"
         "}\n"
         "div.box:target {\n"
         "  display: table;\n"
         "}\n"
         "div.box div.content {\n"
         "  max-height: calc(100vh - 200px);\n"
         "  overflow-y: scroll;\n"
         "}\n"
         "div.src {\n"
         "  line-height: 100%;\n"
         "}\n"
         "div.src div.line {\n"
         "  clear: left;\n"
         "}\n"
         "div.src div.line a {\n"
         "  text-decoration: none;\n"
         "}\n"
         "div.src div.line div.number {\n"
         "  width: 50px;\n"
         "  float: left;\n"
         "  text-align: right;\n"
         "  padding-right: 5px;\n"
         "  background-color: lightgray;\n"
         "}\n"
         "div.src div.line div.content {\n"
         "  overflow: hidden;\n"
         "  unicode-bidi: embed;\n"
         "  font-family: monospace;\n"
         "  white-space: pre;\n"
         "}\n"
         "div.src div.covered div.content {\n"
         "  background-color: lightgreen;\n"
         "}\n"
         "div.src div.uncovered div.content {\n"
         "  background-color: lightcoral;\n"
         "}\n"
         "div.src div.alwayscovered div.content {\n"
         "  background-color: lightgreen;\n"
         "}\n"
         "div.src div.nevercovered div.content {\n"
         "  background-color: lightcoral;\n"
         "}\n"
         "div.src div.sometimescovered div.content {\n"
         "  background-color: khaki;\n"
         "}\n"
         "a.closebutton {\n"
         "  position: absolute;\n"
         "  top: 3px;\n"
         "  right: 8px;\n"
         "  font-size: 20pt;\n"
         "}\n"
         "iframe.src {\n"
         "  position: fixed;\n"
         "  width: 50%;\n"
         "  height: calc(100vh - 60px);\n"
         "  top: 50px;\n"
         "  right: 0;\n"
         "}\n";
}

std::string generateConversationIndex() {
  klee::klee_message("Generating conversation index.");
  std::string conversationsDot = "digraph G {\n"
                                 "  root [label=\"Root\"]\n";

  for (auto cit : conversations) {
    std::string name = SPA::strJoin(cit.first, "_");
    std::vector<std::string> colors(conversationColors[cit.first].begin(),
                                    conversationColors[cit.first].end());
    conversationsDot +=
        "  " + sanitizeToken(name) + " [label=\"" + cit.first.back() + "\\n(" +
        SPA::numToStr(cit.second.size()) + " Paths)\" URL=\"" + name +
        ".html\" style=\"" + (colors.size() > 1 ? "wedged" : "filled") +
        "\" fillcolor=\"" + SPA::strJoin(colors, ":") + "\"]\n";
    if (cit.first.size() > 1) {
      auto parentConversation = cit.first;
      parentConversation.pop_back();

      conversationsDot +=
          "  " + sanitizeToken(SPA::strJoin(parentConversation, "_")) + " -> " +
          sanitizeToken(name) + "\n";
    } else {
      conversationsDot += "  root -> " + sanitizeToken(name) + "\n";
    }
  }
  conversationsDot += "}\n";

  unsigned long numDerived = 0;
  for (auto pit : pathsByUUID) {
    if (!pit.second->getDerivedFromUUID().empty()) {
      numDerived++;
    }
  }

  std::string conversationIndex =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"utf-8\">\n"
      "    <title>All Conversations</title>\n"
      "    <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"header\">\n"
      "      <a href=\"index.html\">All Conversations</a>\n"
      "      <a href=\"paths.html\">All Paths</a>\n"
      "      <a href=\"coverage.html\">Coverage</a>\n"
      "    </div>\n";
  conversationIndex +=
      "    Documented " + SPA::numToStr(pathsByUUID.size()) + " paths (" +
      SPA::numToStr(numDerived) + " derived and " +
      SPA::numToStr(pathsByUUID.size() - numDerived) + " explored), in " +
      SPA::numToStr(conversations.size()) + " conversations.<br />\n" +
      SPA::runCommand(DOT_CMD, conversationsDot) + "\n";
  conversationIndex += "  </body>\n"
                       "</html>\n";
  return conversationIndex;
}

std::string generateCoverageIndex() {
  klee::klee_message("Generating global coverage index.");
  std::string result =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"utf-8\">\n"
      "    <title>Coverage</title>\n"
      "    <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"header\">\n"
      "      <a href=\"index.html\">All Conversations</a>\n"
      "      <a href=\"paths.html\">All Paths</a>\n"
      "      <a href=\"coverage.html\">Coverage</a>\n"
      "    </div>\n"
      "    <iframe src='about:blank' name='src' class='src'></iframe>\n"
      "    <h1>Coverage</h1>\n"
      "    <b>Files:</b><br />\n";
  for (auto it : coverageIndexes) {
    result += "    <a href='" + it.second + "' target='src'>" +
              remapSrcFileName(it.first) + "</a><br />\n";
  }
  result += "  </body>\n"
            "</html>\n";

  return result;
}

std::string generateCoverageFile(std::string origSrcFile) {
  std::string srcFileName = remapSrcFileName(origSrcFile);

  klee::klee_message("Generating coverage index for %s.", srcFileName.c_str());

  std::string coverageHtml =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"utf-8\">\n"
      "    <title>Coverage for " + srcFileName +
      "</title>\n"
      "    <link rel=\"stylesheet\" type=\"text/css\" "
      "href=\"style.css\">\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"header\">\n"
      "      <a href=\"index.html\">All Conversations</a>\n"
      "      <a href=\"paths.html\">All Paths</a>\n"
      "      <a href=\"coverage.html\">Coverage</a>\n"
      "    </div>\n";
  coverageHtml += "    <b>" + srcFileName + "</b>\n";
  coverageHtml += "    <div class='content'>\n"
                  "      <div class='src'>\n";

  std::ifstream srcFile(srcFileName);
  unsigned long lastLineNum =
      coverage[origSrcFile].empty() ? 0 : coverage[origSrcFile].rbegin()->first;
  for (unsigned long srcLineNum = 1;
       srcFile.good() || srcLineNum <= lastLineNum; srcLineNum++) {
    std::string srcLine;
    std::getline(srcFile, srcLine);
    coverageHtml += "        <div class='line ";
    if (coverage[origSrcFile].count(srcLineNum)) {
      if (coverage[origSrcFile][srcLineNum][false].empty()) {
        coverageHtml += "alwayscovered";
      } else if (coverage[origSrcFile][srcLineNum][true].empty()) {
        coverageHtml += "nevercovered";
      } else {
        coverageHtml += "sometimescovered";
      }
    } else {
      coverageHtml += "unknown";
    }
    coverageHtml +=
        "' title='Covered by " +
        SPA::numToStr(coverage[origSrcFile][srcLineNum][true].size()) +
        " paths.&#10;Not covered by " +
        SPA::numToStr(coverage[origSrcFile][srcLineNum][false].size()) +
        " paths.'>\n";
    if (coverage[origSrcFile].count(srcLineNum)) {
      coverageHtml += "<a href='#l" + SPA::numToStr(srcLineNum) + "'>\n";
    }
    coverageHtml +=
        "          <div class='number'>" + SPA::numToStr(srcLineNum) +
        "</div>\n"
        "          <div class='content'>" + sanitizeHTML(srcLine) + "</div>\n";
    if (coverage[origSrcFile].count(srcLineNum)) {
      coverageHtml += "        </a>\n";
    }
    coverageHtml += "        </div>\n";
  }
  coverageHtml += "      </div>\n"
                  "    </div>\n";

  for (auto lit : coverage[origSrcFile]) {
    coverageHtml +=
        "    <div class='box' id='l" + SPA::numToStr(lit.first) + "'>\n";
    coverageHtml +=
        "      <a class='closebutton' href='#'>&#x2715;</a>\n"
        "      <h2>" + srcFileName + ":" + SPA::numToStr(lit.first) + "</h2>\n";
    coverageHtml += "      <div class='content'>\n"
                    "        <b>Other paths that reached here:</b><br />\n";
    unsigned long counter = 1;
    for (auto pit : lit.second[true]) {
      coverageHtml += "        <a href='" + pit->getUUID() + ".html'>" +
                      SPA::numToStr(counter++) + "</a>\n";
    }
    coverageHtml +=
        "        <br />\n"
        "        <b>Other paths that didn't reach here:</b><br />\n";
    counter = 1;
    for (auto pit : lit.second[false]) {
      coverageHtml += "        <a href='" + pit->getUUID() + ".html'>" +
                      SPA::numToStr(counter++) + "</a>\n";
    }
    coverageHtml += "        <br />\n"
                    "      </div>\n"
                    "    </div>\n";
  }
  coverageHtml += "  </body>\n"
                  "</html>\n";

  return coverageHtml;
}

static void mongoose_event_handler(struct mg_connection *nc, int ev,
                                   void *ev_data) {
  switch (ev) {
  case MG_EV_HTTP_REQUEST: {
    struct http_message *hm = (struct http_message *)ev_data;
    std::string uri(hm->uri.p, hm->uri.len);
    if (uri[0] == '/') {
      uri = uri.substr(1);
    }
    if (uri == "") {
      uri = "index.html";
    }

    if (files.count(uri)) {
      klee::klee_message("HTTP Request: %s, 200 OK", uri.c_str());
      mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\n"
                          "Transfer-Encoding: chunked\r\n\r\n");
      mg_printf_http_chunk(nc, "%s", files[uri]().c_str());
      mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
    } else {
      klee::klee_message("HTTP Request: %s, 404 Not Found", uri.c_str());
      mg_printf(nc, "%s", "HTTP/1.0 404 Not Found\r\n"
                          "Content-Length: 0\r\n\r\n");
    }
  } break;
  default:
    break;
  }
}

void generateFiles() {
  auto result = mkdir(Directory.c_str(), 0755);
  assert(result == 0 || errno == EEXIST);

  for (auto file : files) {
    if (SPA::forkWorker(NumProcesses) == 0) {
      klee::klee_message("Generating file: %s", file.first.c_str());
      std::ofstream fileStream(Directory + "/" + file.first);
      assert(fileStream.good());
      fileStream << file.second();

      if (NumProcesses > 1) {
        exit(0);
      }
    }
  }
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv,
      "Systematic Protocol Analysis - Path File Documentation Generation");

  for (auto cf : ColorFilter) {
    auto d = cf.find(":");
    assert((d != std::string::npos) && "Wrong format for filter color.");
    std::string color = cf.substr(0, d);
    SPA::FilterExpression *fe = SPA::FilterExpression::parse(cf.substr(d + 1));
    assert(fe && "Could not parse filter.");
    colorFilters.insert(std::make_pair(fe, color));
  }

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  struct mg_mgr mgr;
  if (ServeHttp > 0) {
    klee::klee_message("Starting HTTP server on port %d.", (int) ServeHttp);
    mg_mgr_init(&mgr, NULL);

    struct mg_connection *nc =
        mg_bind(&mgr, SPA::numToStr(ServeHttp).c_str(), mongoose_event_handler);
    assert(nc && "Error binding mongoose HTTP server.");
    mg_set_protocol_http_websocket(nc);
  }

  klee::klee_message("Loading paths.");

  files["style.css"] = generateCSS;
  files["index.html"] = generateConversationIndex;
  files["coverage.html"] = generateCoverageIndex;
  files["paths.html"] = [&]() { return generatePathIndex(allPaths); }
  ;

  std::unique_ptr<SPA::PathLoader> pathLoader(new SPA::PathLoader(inFile));
  SPA::Path *path;
  unsigned long pathID = 1;
  while ((path = pathLoader->getPath()) || ServeHttp > 0) {
    if (!path) {
      mg_mgr_poll(&mgr, 1000);
      continue;
    } else {
      mg_mgr_poll(&mgr, 0);
    }

    klee::klee_message("  Loading path %ld (%s).", pathID,
                       path->getUUID().c_str());
    pathsByUUID[path->getUUID()] = path;
    pathIDs[path] = pathID;
    allPaths.insert(path);

    if (path->getParticipants().size() > 1) {
      assert(pathsByUUID.count(path->getParticipants().rbegin()[1]
                                   ->getPathUUID()) && "Unknown parent path.");
      childrenPaths[
          pathsByUUID[path->getParticipants().rbegin()[1]->getPathUUID()]]
          .insert(path);
    }

    for (auto cfit : colorFilters) {
      if (cfit.first->checkPath(*path)) {
        pathColors[path].insert(cfit.second);
      }
    }
    if (pathColors[path].empty()) {
      pathColors[path].insert("white");
    }

    files[path->getUUID() + ".html"] = [ = ]() {
      return generatePathHTML(path);
    }
    ;

    for (auto fit : path->getExploredLineCoverage()) {
      for (auto lit : fit.second) {
        coverage[fit.first][lit.first][lit.second].insert(path);
      }
      if (!coverageIndexes.count(fit.first)) {
        std::string coverageIndex =
            "coverage-" + SPA::numToStr(coverageIndexes.size()) + ".html";
        coverageIndexes[fit.first] = coverageIndex;
        files[coverageIndex] = [ = ]() {
          return generateCoverageFile(fit.first);
        }
        ;
      }
    }

    std::vector<std::string> participants;
    for (auto pit : path->getParticipants()) {
      participants.push_back(pit->getName());
    }
    conversations[participants].insert(path);
    std::set<SPA::Path *> fullConversation, worklist;
    // Add all parent paths to conversation.
    worklist = conversations[participants];
    while (!worklist.empty()) {
      SPA::Path *path = *worklist.begin();
      worklist.erase(path);
      if (path && !fullConversation.count(path)) {
        fullConversation.insert(path);
        if (path->getParticipants().size() > 1) {
          worklist.insert(
              pathsByUUID[path->getParticipants().rbegin()[1]->getPathUUID()]);
        }
        if (!path->getDerivedFromUUID().empty()) {
          assert(pathsByUUID.count(path->getDerivedFromUUID()) &&
                 "Path derived from unknown source.");
          worklist.insert(pathsByUUID[path->getDerivedFromUUID()]);
        }
      }
    }
    conversationColors[participants]
        .insert(pathColors[path].begin(), pathColors[path].end());

    files[SPA::strJoin(participants, "_") + ".html"] = [ = ]() {
      return generatePathIndex(fullConversation);
    }
    ;

    pathID++;
  }

  generateFiles();

  // Wait for all children to finish.
  while (wait(nullptr)) {
    if (errno == ECHILD) {
      break;
    }
  }

  return 0;
}
