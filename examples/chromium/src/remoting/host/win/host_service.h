// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_HOST_SERVICE_H_
#define REMOTING_HOST_WIN_HOST_SERVICE_H_

#include <windows.h>

#include <list>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/win/message_window.h"
#include "remoting/host/win/wts_terminal_monitor.h"

class CommandLine;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AutoThreadTaskRunner;
class Stoppable;
class WtsTerminalObserver;

class HostService : public win::MessageWindow::Delegate,
                    public WtsTerminalMonitor {
 public:
  static HostService* GetInstance();

  // This function parses the command line and selects the action routine.
  bool InitWithCommandLine(const CommandLine* command_line);

  // Invoke the choosen action routine.
  int Run();

  // WtsTerminalMonitor implementation
  virtual bool AddWtsTerminalObserver(const net::IPEndPoint& client_endpoint,
                                      WtsTerminalObserver* observer) OVERRIDE;
  virtual void RemoveWtsTerminalObserver(
      WtsTerminalObserver* observer) OVERRIDE;

 private:
  HostService();
  ~HostService();

  // Notifies the service of changes in session state.
  void OnSessionChange(uint32 event, uint32 session_id);

  // Creates the process launcher.
  void CreateLauncher(scoped_refptr<AutoThreadTaskRunner> task_runner);

  void OnChildStopped();

  // This function handshakes with the service control manager and starts
  // the service.
  int RunAsService();

  // Runs the service on the service thread. A separate routine is used to make
  // sure all local objects are destoyed by the time |stopped_event_| is
  // signalled.
  void RunAsServiceImpl();

  // This function starts the service in interactive mode (i.e. as a plain
  // console application).
  int RunInConsole();

  // win::MessageWindow::Delegate interface.
  virtual bool HandleMessage(HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             LPARAM lparam,
                             LRESULT* result) OVERRIDE;

  static BOOL WINAPI ConsoleControlHandler(DWORD event);

  // The control handler of the service.
  static DWORD WINAPI ServiceControlHandler(DWORD control,
                                            DWORD event_type,
                                            LPVOID event_data,
                                            LPVOID context);

  // The main service entry point.
  static VOID WINAPI ServiceMain(DWORD argc, WCHAR* argv[]);

  struct RegisteredObserver {
    // Specifies the client address of an RDP connection or IPEndPoint() for
    // the physical console.
    net::IPEndPoint client_endpoint;

    // Specifies ID of the attached session or |kInvalidSession| if no session
    // is attached to the WTS console.
    uint32 session_id;

    // Points to the observer receiving notifications about the WTS console
    // identified by |client_endpoint|.
    WtsTerminalObserver* observer;
  };

  // The list of observers receiving session notifications.
  std::list<RegisteredObserver> observers_;

  scoped_ptr<Stoppable> child_;

  // Service message loop.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The action routine to be executed.
  int (HostService::*run_routine_)();

  // The service status handle.
  SERVICE_STATUS_HANDLE service_status_handle_;

  // A waitable event that is used to wait until the service is stopped.
  base::WaitableEvent stopped_event_;

  // Singleton.
  friend struct DefaultSingletonTraits<HostService>;

  DISALLOW_COPY_AND_ASSIGN(HostService);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_HOST_SERVICE_H_
