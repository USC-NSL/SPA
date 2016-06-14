#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <stdint.h>
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <chrono>

#include <llvm/ADT/OwningPtr.h>

#include <spa/Path.h>
#include <spa/PathLoader.h>

#define NETCALC_CLIENT_BC "netcalc-client.bc"
#define NETCALC_SERVER_BC "netcalc-server.bc"
#define SPDYLAY_CLIENT_BC "spa-client.bc"
#define SPDYLAY_SERVER_BC "spa-server.bc"
#define NGINX_BC "nginx"
#define EXOSIP_CLIENT_BC "client.bc"
#define EXOSIP_SERVER_BC "server.bc"
#define EXOSIP_OPTIONIONS_SERVER_BC "options-server.bc"
#define PJSIP_CLIENT_BC "pjsua.spa.bc"
#define PJSIP_SERVER_BC "pjsua.spa.bc"

#define LOG()                                                                  \
  std::cerr << "[" << (std::chrono::duration_cast<std::chrono::milliseconds>(  \
                          std::chrono::steady_clock::now() - programStartTime) \
                           .count() / 1000.0) << "] "

auto programStartTime = std::chrono::steady_clock::now();

typedef bool (*TestClassifier)(SPA::Path *path);

bool netcalcDiv0(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == NETCALC_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NETCALC_SERVER_BC) {
    assert(path->getTestInputs().count("spa_in_api_op_netcalc-client.bc_0") &&
           path->getTestInputs().count("spa_in_api_arg2_netcalc-client.bc_2"));
    return std::set<std::vector<uint8_t> >({
      std::vector<uint8_t>({
        3, 0, 0, 0
      }),
          std::vector<uint8_t>({
        4, 0, 0, 0
      })
    })
               .count(
                   path->getTestInput("spa_in_api_op_netcalc-client.bc_0")) &&
           path->getTestInput("spa_in_api_arg2_netcalc-client.bc_2") ==
               std::vector<uint8_t>({
      0, 0, 0, 0, 0, 0, 0, 0
    });
  }
  return false;
}

bool netcalcImplicitArg(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == NETCALC_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NETCALC_SERVER_BC) {
    assert(path->getTestInputs().count("spa_in_api_op_netcalc-client.bc_0") &&
           path->getTestInputs().count("spa_in_api_arg1_netcalc-client.bc_1") &&
           path->getTestInputs().count("spa_in_api_arg2_netcalc-client.bc_2"));
    return (path->getTestInput("spa_in_api_op_netcalc-client.bc_0") ==
                std::vector<uint8_t>({
      0, 0, 0, 0
    }) &&
            (path->getTestInput("spa_in_api_arg1_netcalc-client.bc_1") ==
                 std::vector<uint8_t>({
      1, 0, 0, 0, 0, 0, 0, 0
    }) ||
             path->getTestInput("spa_in_api_arg2_netcalc-client.bc_2") ==
                 std::vector<uint8_t>({
      1, 0, 0, 0, 0, 0, 0, 0
    }))) ||
           (path->getTestInput("spa_in_api_op_netcalc-client.bc_0") ==
                std::vector<uint8_t>({
      1, 0, 0, 0
    }) &&
            path->getTestInput("spa_in_api_arg2_netcalc-client.bc_2") ==
                std::vector<uint8_t>({
      1, 0, 0, 0, 0, 0, 0, 0
    }));
  }
  return false;
}

bool spdylayDiffVersion(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == SPDYLAY_SERVER_BC) {
    assert(path->getTestInputs().count("spa_in_api_clientVersion") &&
           path->getTestInputs().count("spa_in_api_serverVersion"));
    return (path->getTestInput("spa_in_api_clientVersion") ==
            std::vector<uint8_t>({
      2, 0
    })) !=
           (path->getTestInput("spa_in_api_serverVersion") ==
            std::vector<uint8_t>({
      2, 0
    }));
  }
  return false;
}

bool spdylayBadName(SPA::Path *path) {
  std::set<std::string> names = { "spa_in_api_name", "spa_in_api_name1",
                                  "spa_in_api_name2", "spa_in_api_name3",
                                  "spa_in_api_name4", "spa_in_api_name5" };
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC) {
    for (auto name : names) {
      if (path->getTestInputs().count(name)) {
        for (auto it : path->getTestInput(name)) {
          if (it < 0x20)
            return true;
        }
      }
    }
  }
  return false;
}

bool nginxBadName(SPA::Path *path) {
  std::set<std::string> names = { "spa_in_api_name", "spa_in_api_name1",
                                  "spa_in_api_name2", "spa_in_api_name3",
                                  "spa_in_api_name4", "spa_in_api_name5" };
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC) {
    for (auto name : names) {
      if (path->getTestInputs().count(name)) {
        for (auto it : path->getTestInput(name)) {
          if (it == 0x20)
            return true;
        }
      }
    }
  }
  return false;
}

bool spdylayEmptyValue(SPA::Path *path) {
  std::set<std::string> values = { "spa_in_api_value", "spa_in_api_value1",
                                   "spa_in_api_value2", "spa_in_api_value3",
                                   "spa_in_api_value4", "spa_in_api_value5" };
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC) {
    for (auto value : values) {
      if (path->getTestInputs().count(value)) {
        if (path->getTestInput(value)[0] == '\0') {
          return true;
        }
      }
    }
  }
  return false;
}

// From spdylay_frame.c:
static int VALID_HD_VALUE_CHARS[] = {
  1 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */, 0 /* EOT  */,
  0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */, 0 /* BS   */, 1 /* HT   */,
  0 /* LF   */, 0 /* VT   */, 0 /* FF   */, 0 /* CR   */, 0 /* SO   */,
  0 /* SI   */, 0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
  0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */, 0 /* CAN  */,
  0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */, 0 /* FS   */, 0 /* GS   */,
  0 /* RS   */, 0 /* US   */, 1 /* SPC  */, 1 /* !    */, 1 /* "    */,
  1 /* #    */, 1 /* $    */, 1 /* %    */, 1 /* &    */, 1 /* '    */,
  1 /* (    */, 1 /* )    */, 1 /* *    */, 1 /* +    */, 1 /* ,    */,
  1 /* -    */, 1 /* .    */, 1 /* /    */, 1 /* 0    */, 1 /* 1    */,
  1 /* 2    */, 1 /* 3    */, 1 /* 4    */, 1 /* 5    */, 1 /* 6    */,
  1 /* 7    */, 1 /* 8    */, 1 /* 9    */, 1 /* :    */, 1 /* ;    */,
  1 /* <    */, 1 /* =    */, 1 /* >    */, 1 /* ?    */, 1 /* @    */,
  1 /* A    */, 1 /* B    */, 1 /* C    */, 1 /* D    */, 1 /* E    */,
  1 /* F    */, 1 /* G    */, 1 /* H    */, 1 /* I    */, 1 /* J    */,
  1 /* K    */, 1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
  1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */, 1 /* T    */,
  1 /* U    */, 1 /* V    */, 1 /* W    */, 1 /* X    */, 1 /* Y    */,
  1 /* Z    */, 1 /* [    */, 1 /* \    */, 1 /* ]    */, 1 /* ^    */,
  1 /* _    */, 1 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
  1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */, 1 /* h    */,
  1 /* i    */, 1 /* j    */, 1 /* k    */, 1 /* l    */, 1 /* m    */,
  1 /* n    */, 1 /* o    */, 1 /* p    */, 1 /* q    */, 1 /* r    */,
  1 /* s    */, 1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
  1 /* x    */, 1 /* y    */, 1 /* z    */, 1 /* {    */, 1 /* |    */,
  1 /* }    */, 1 /* ~    */, 0 /* DEL  */, 1 /* 0x80 */, 1 /* 0x81 */,
  1 /* 0x82 */, 1 /* 0x83 */, 1 /* 0x84 */, 1 /* 0x85 */, 1 /* 0x86 */,
  1 /* 0x87 */, 1 /* 0x88 */, 1 /* 0x89 */, 1 /* 0x8a */, 1 /* 0x8b */,
  1 /* 0x8c */, 1 /* 0x8d */, 1 /* 0x8e */, 1 /* 0x8f */, 1 /* 0x90 */,
  1 /* 0x91 */, 1 /* 0x92 */, 1 /* 0x93 */, 1 /* 0x94 */, 1 /* 0x95 */,
  1 /* 0x96 */, 1 /* 0x97 */, 1 /* 0x98 */, 1 /* 0x99 */, 1 /* 0x9a */,
  1 /* 0x9b */, 1 /* 0x9c */, 1 /* 0x9d */, 1 /* 0x9e */, 1 /* 0x9f */,
  1 /* 0xa0 */, 1 /* 0xa1 */, 1 /* 0xa2 */, 1 /* 0xa3 */, 1 /* 0xa4 */,
  1 /* 0xa5 */, 1 /* 0xa6 */, 1 /* 0xa7 */, 1 /* 0xa8 */, 1 /* 0xa9 */,
  1 /* 0xaa */, 1 /* 0xab */, 1 /* 0xac */, 1 /* 0xad */, 1 /* 0xae */,
  1 /* 0xaf */, 1 /* 0xb0 */, 1 /* 0xb1 */, 1 /* 0xb2 */, 1 /* 0xb3 */,
  1 /* 0xb4 */, 1 /* 0xb5 */, 1 /* 0xb6 */, 1 /* 0xb7 */, 1 /* 0xb8 */,
  1 /* 0xb9 */, 1 /* 0xba */, 1 /* 0xbb */, 1 /* 0xbc */, 1 /* 0xbd */,
  1 /* 0xbe */, 1 /* 0xbf */, 1 /* 0xc0 */, 1 /* 0xc1 */, 1 /* 0xc2 */,
  1 /* 0xc3 */, 1 /* 0xc4 */, 1 /* 0xc5 */, 1 /* 0xc6 */, 1 /* 0xc7 */,
  1 /* 0xc8 */, 1 /* 0xc9 */, 1 /* 0xca */, 1 /* 0xcb */, 1 /* 0xcc */,
  1 /* 0xcd */, 1 /* 0xce */, 1 /* 0xcf */, 1 /* 0xd0 */, 1 /* 0xd1 */,
  1 /* 0xd2 */, 1 /* 0xd3 */, 1 /* 0xd4 */, 1 /* 0xd5 */, 1 /* 0xd6 */,
  1 /* 0xd7 */, 1 /* 0xd8 */, 1 /* 0xd9 */, 1 /* 0xda */, 1 /* 0xdb */,
  1 /* 0xdc */, 1 /* 0xdd */, 1 /* 0xde */, 1 /* 0xdf */, 1 /* 0xe0 */,
  1 /* 0xe1 */, 1 /* 0xe2 */, 1 /* 0xe3 */, 1 /* 0xe4 */, 1 /* 0xe5 */,
  1 /* 0xe6 */, 1 /* 0xe7 */, 1 /* 0xe8 */, 1 /* 0xe9 */, 1 /* 0xea */,
  1 /* 0xeb */, 1 /* 0xec */, 1 /* 0xed */, 1 /* 0xee */, 1 /* 0xef */,
  1 /* 0xf0 */, 1 /* 0xf1 */, 1 /* 0xf2 */, 1 /* 0xf3 */, 1 /* 0xf4 */,
  1 /* 0xf5 */, 1 /* 0xf6 */, 1 /* 0xf7 */, 1 /* 0xf8 */, 1 /* 0xf9 */,
  1 /* 0xfa */, 1 /* 0xfb */, 1 /* 0xfc */, 1 /* 0xfd */, 1 /* 0xfe */,
  1 /* 0xff */
};
bool spdylayBadValueChar(SPA::Path *path) {
  std::set<std::string> values = { "spa_in_api_path", "spa_in_api_value",
                                   "spa_in_api_value1", "spa_in_api_value2",
                                   "spa_in_api_value3", "spa_in_api_value4",
                                   "spa_in_api_value5" };
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == SPDYLAY_SERVER_BC) {
    for (auto value : values) {
      if (path->getTestInputs().count(value)) {
        for (auto c : path->getTestInput(value)) {
          if (!VALID_HD_VALUE_CHARS[c])
            return true;
        }
      }
    }
  }
  return false;
}

bool spdylayNoDataLength(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_SERVER_BC &&
      path->getSymbolLog().back()->getParticipant() == SPDYLAY_CLIENT_BC) {
    return !path->getTestInputs().count("spa_in_api_dataLength");
  }
  return false;
}

bool nginxSpdy3(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    assert(path->getTestInputs().count("spa_in_api_clientVersion"));
    return path->getTestInput("spa_in_api_clientVersion") !=
           std::vector<uint8_t>({
      2, 0
    });
  }
  return false;
}

bool nginxHttp09(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    assert(path->getTestInputs().count("spa_in_api_versionId"));
    return path->getTestInput("spa_in_api_versionId") == std::vector<uint8_t>({
      2
    });
  }
  return false;
}

bool nginxUnknownColonHeader(SPA::Path *path) {
  std::set<std::string> names = { "spa_in_api_name", "spa_in_api_name1",
                                  "spa_in_api_name2", "spa_in_api_name3",
                                  "spa_in_api_name4", "spa_in_api_name5" };
  // From ngx_http_spdy.c:373
  std::set<std::string> whitelist = { "method", "scheme", "host", "path",
                                      "version" };
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    for (auto name : names) {
      if (path->getTestInputs().count(name)) {
        if (path->getTestInput(name)[0] == ':') {
          std::string header(path->getTestInput(name).begin(),
                             path->getTestInput(name).end());
          if (whitelist.count(header.substr(1)) == 0) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool nginxBadUrlPercent(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    assert(path->getTestInputs().count("spa_in_api_path"));
    for (unsigned i = 0; i < path->getTestInput("spa_in_api_path").size();
         i++) {
      if (path->getTestInput("spa_in_api_path")[i] == '%') {
        if (i > path->getTestInput("spa_in_api_path").size() - 3)
          return true;
        if (!isxdigit(path->getTestInput("spa_in_api_path")[i + 1]))
          return true;
        if (!isxdigit(path->getTestInput("spa_in_api_path")[i + 2]))
          return true;
      }
    }
  }
  return false;
}

bool nginxPercent00(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    assert(path->getTestInputs().count("spa_in_api_path"));
    for (unsigned i = 0; i < path->getTestInput("spa_in_api_path").size() - 2;
         i++) {
      if (path->getTestInput("spa_in_api_path")[i] == '%' &&
          path->getTestInput("spa_in_api_path")[i + 1] == '0' &&
          path->getTestInput("spa_in_api_path")[i + 2] == '0') {
        return true;
      }
    }
  }
  return false;
}

bool nginxValueCrLf(SPA::Path *path) {
  std::set<std::string> values = { "spa_in_api_value", "spa_in_api_value1",
                                   "spa_in_api_value2", "spa_in_api_value3",
                                   "spa_in_api_value4", "spa_in_api_value5",
                                   "spa_in_api_path" };
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    for (auto value : values) {
      if (path->getTestInputs().count(value)) {
        if (std::string(path->getTestInput(value).begin(),
                        path->getTestInput(value).end())
                .find_first_of("\r\n") != std::string::npos) {
          return true;
        }
      }
    }
  }
  return false;
}

bool nginxDotDotPastRoot(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    int depth = 0;

    enum {
      SLASH,
      DIR,
      DOT,
      DOTDOT
    } state = SLASH;
    assert(path->getTestInputs().count("spa_in_api_path"));
    for (auto c : path->getTestInput("spa_in_api_path")) {
      switch (state) {
      case SLASH:
        if (c == '/') {
          state = SLASH;
        } else if (c == '.') {
          state = DOT;
        } else {
          state = DIR;
          depth++;
        }
        break;
      case DIR:
        if (c == '/') {
          state = SLASH;
        } else {
          state = DIR;
        }
        break;
      case DOT:
        if (c == '/') {
          state = SLASH;
        } else if (c == '.') {
          state = DOTDOT;
        } else {
          state = DIR;
        }
        break;
      case DOTDOT:
        if (c == '/') {
          state = SLASH;
          depth--;
        } else {
          state = DIR;
        }
        break;
      default:
        assert(false && "Bad state.");
      }
      if (depth < 0)
        return true;
    }
  }
  return false;
}

bool nginxTrace(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == SPDYLAY_CLIENT_BC &&
      path->getSymbolLog().back()->getParticipant() == NGINX_BC) {
    assert(path->getTestInputs().count("spa_in_api_methodId"));
    return path->getTestInput("spa_in_api_methodId") == std::vector<uint8_t>({
      6
    });
  }
  return false;
}

bool sipFromBadChar(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == EXOSIP_CLIENT_BC) {
    assert(path->getTestInputs().count("spa_in_api_from"));
    for (auto it : path->getTestInput("spa_in_api_from")) {
      if (!isprint(it))
        return true;
    }
  }
  return false;
}

bool sipFromNoScheme(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == EXOSIP_CLIENT_BC) {
    assert(path->getTestInputs().count("spa_in_api_from"));
    std::string from(path->getTestInput("spa_in_api_from").begin(),
                     path->getTestInput("spa_in_api_from").end());
    return from.find("sip:") == std::string::npos;
  }
  return false;
}

bool sipFromBadQuote(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == EXOSIP_CLIENT_BC) {
    unsigned long count = 0;
    assert(path->getTestInputs().count("spa_in_api_from"));
    for (auto it : path->getTestInput("spa_in_api_from")) {
      if (it == '\"')
        count++;
    }
    return count != 0 && count != 2;
  }
  return false;
}

bool sipToConfusedScheme(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == EXOSIP_CLIENT_BC) {
    assert(path->getTestInputs().count("spa_in_api_to"));
    std::string to;
    std::transform(path->getTestInput("spa_in_api_to").begin(),
                   path->getTestInput("spa_in_api_to").end(), to.begin(),
                   ::tolower);
    if (to.compare(0, 3, "sip") == 0 && to[4] == ':')
      return true;
    if (to.compare(0, 4, "<sip") == 0 && to[5] == ':')
      return true;
  }
  return false;
}

bool sipEventBadChar(SPA::Path *path) {
  if (path->getSymbolLog().front()->getParticipant() == EXOSIP_CLIENT_BC) {
    if (path->getTestInputs().count("spa_in_api_event")) {
      for (auto it : path->getTestInput("spa_in_api_event")) {
        if (it == '\0')
          break;
        if (!isprint(it))
          return true;
      }
    }
  }
  return false;
}

static std::vector<std::pair<TestClassifier, std::string> > classifiers = {
  { netcalcDiv0, "netcalcDiv0.paths" },
  { netcalcImplicitArg, "netcalcImplicitArg.paths" },
  { spdylayDiffVersion, "spdylayDiffVersion.paths" },
  { spdylayEmptyValue, "spdylayEmptyValue.paths" },
  { spdylayNoDataLength, "spdylayNoDataLength.paths" },
  { nginxUnknownColonHeader, "nginxUnknownColonHeader.paths" },
  { nginxTrace, "nginxTrace.paths" },
  { nginxValueCrLf, "nginxValueCrLf.paths" },
  { nginxBadUrlPercent, "nginxBadUrlPercent.paths" },
  { nginxPercent00, "nginxPercent00.paths" },
  { nginxDotDotPastRoot, "nginxDotDotPastRoot.paths" },
  { nginxHttp09, "nginxHttp09.paths" }, { nginxSpdy3, "nginxSpdy3.paths" },
  { spdylayBadName, "spdylayBadName.paths" },
  { nginxBadName, "nginxBadName.paths" },
  { spdylayBadValueChar, "spdylayBadValueChar.paths" },
  { sipFromBadChar, "sipFromBadChar.paths" },
  { sipFromNoScheme, "sipFromNoScheme.paths" },
  { sipToConfusedScheme, "sipToConfusedScheme.paths" },
  { sipEventBadChar, "sipEventBadChar.paths" }, { NULL, "default.paths" }
};

std::vector<unsigned long> resultCounts;

void displayStats() {
  LOG() << "Breakdown:" << std::endl;
  unsigned long total = 0;
  unsigned numClusters = 0;
  for (unsigned i = 0; i < resultCounts.size(); i++) {
    if (resultCounts[i]) {
      LOG() << "  " << classifiers[i].second << ": " << resultCounts[i]
            << std::endl;
      total += resultCounts[i];
      numClusters++;
    }
  }
  LOG() << "  Total: " << total << std::endl;
  LOG() << "  Number of clusters: " << numClusters << std::endl;
}

int main(int argc, char **argv, char **envp) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " [-f] <input-file>" << std::endl;

    return -1;
  }

  bool follow = false;
  char *inputFileName;

  if (std::string("-f") == argv[1]) {
    follow = true;
    inputFileName = argv[2];
  } else {
    inputFileName = argv[1];
  }

  std::ifstream inputFile(inputFileName);
  assert(inputFile.is_open() && "Unable to open input file.");
  SPA::PathLoader pathLoader(inputFile);

  std::vector<std::ofstream *> outputFiles(classifiers.size());
  resultCounts.resize(classifiers.size());

  llvm::OwningPtr<SPA::Path> path;
  while ((path.reset(pathLoader.getPath()), path.get()) || follow) {
    if (!path) {
      displayStats();
      LOG() << "Reached end of input. Sleeping." << std::endl;
      sleep(1);
      continue;
    }
    for (int i = 0;; i++) {
      if ((!classifiers[i].first) || classifiers[i].first(path.get())) {
        LOG() << "Test-case classified as " << classifiers[i].second
              << std::endl;
        if (!outputFiles[i]) {
          outputFiles[i] = new std::ofstream(classifiers[i].second);
          assert(outputFiles[i]->is_open());
        }
        *outputFiles[i] << *path;
        outputFiles[i]->flush();
        resultCounts[i]++;
        displayStats();
        break;
      }
    }
  }
  displayStats();
}
