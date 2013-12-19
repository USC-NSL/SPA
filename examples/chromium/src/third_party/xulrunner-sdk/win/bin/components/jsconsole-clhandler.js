//@line 41 "e:\builds\tinderbox\XR-Trunk\WINNT_5.2_Depend\mozilla\toolkit\components\console\jsconsole-clhandler.js"
const Cc = Components.classes;
const Ci = Components.interfaces;
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function jsConsoleHandler() {}
jsConsoleHandler.prototype = {
  handle: function clh_handle(cmdLine) {
    if (!cmdLine.handleFlag("jsconsole", false))
      return;

    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
    var console = wm.getMostRecentWindow("global:console");
    if (!console) {
      var wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                   getService(Ci.nsIWindowWatcher);
      wwatch.openWindow(null, "chrome://global/content/console.xul", "_blank",
                        "chrome,dialog=no,all", cmdLine);
    } else {
      console.focus(); // the Error console was already open
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO)
      cmdLine.preventDefault = true;
  },

  helpInfo : "  -jsconsole           Open the Error console.\n",

  classDescription: "jsConsoleHandler",
  classID: Components.ID("{2cd0c310-e127-44d0-88fc-4435c9ab4d4b}"),
  contractID: "@mozilla.org/toolkit/console-clh;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
  _xpcom_categories: [{category: "command-line-handler", entry: "b-jsconsole"}]
};

function NSGetModule(compMgr, fileSpec)
  XPCOMUtils.generateModule([jsConsoleHandler]);
