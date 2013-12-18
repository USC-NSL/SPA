// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
#define CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_process_sub_thread.h"

class CommandLine;
class HighResolutionTimerManager;

namespace base {
class MessageLoop;
class PowerMonitor;
class SystemMonitor;
}

namespace media {
class AudioManager;
}

namespace net {
class NetworkChangeNotifier;
}

namespace content {
class AudioMirroringManager;
class BrowserMainParts;
class BrowserOnlineStateObserver;
class BrowserShutdownImpl;
class BrowserThreadImpl;
class MediaStreamManager;
class ResourceDispatcherHostImpl;
class SpeechRecognitionManagerImpl;
class SystemMessageWindowWin;
class WebKitThread;
struct MainFunctionParams;

#if defined(OS_LINUX)
class DeviceMonitorLinux;
#elif defined(OS_MACOSX)
class DeviceMonitorMac;
#endif

// Implements the main browser loop stages called from BrowserMainRunner.
// See comments in browser_main_parts.h for additional info.
// All functions are to be called only on the UI thread unless otherwise noted.
class BrowserMainLoop {
 public:
  class MemoryObserver;
  explicit BrowserMainLoop(const MainFunctionParams& parameters);
  virtual ~BrowserMainLoop();

  void Init();

  void EarlyInitialization();
  void InitializeToolkit();
  void MainMessageLoopStart();

  // Create all secondary threads.
  void CreateThreads();

  // Perform the default message loop run logic.
  void RunMainMessageLoopParts();

  // Performs the shutdown sequence, starting with PostMainMessageLoopRun
  // through stopping threads to PostDestroyThreads.
  void ShutdownThreadsAndCleanUp();

  int GetResultCode() const { return result_code_; }

  // Can be called on any thread.
  static media::AudioManager* GetAudioManager();
  static AudioMirroringManager* GetAudioMirroringManager();
  static MediaStreamManager* GetMediaStreamManager();

 private:
  // For ShutdownThreadsAndCleanUp.
  friend class BrowserShutdownImpl;

  void InitializeMainThread();

  // Called right after the browser threads have been started.
  void BrowserThreadsStarted();

  void MainMessageLoopRun();

  // Members initialized on construction ---------------------------------------
  const MainFunctionParams& parameters_;
  const CommandLine& parsed_command_line_;
  int result_code_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  scoped_ptr<base::MessageLoop> main_message_loop_;
  scoped_ptr<base::SystemMonitor> system_monitor_;
  scoped_ptr<base::PowerMonitor> power_monitor_;
  scoped_ptr<HighResolutionTimerManager> hi_res_timer_manager_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<AudioMirroringManager> audio_mirroring_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  // Per-process listener for online state changes.
  scoped_ptr<BrowserOnlineStateObserver> online_state_observer_;
#if defined(OS_WIN)
  scoped_ptr<SystemMessageWindowWin> system_message_window_;
#elif defined(OS_LINUX)
  scoped_ptr<DeviceMonitorLinux> device_monitor_linux_;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  scoped_ptr<DeviceMonitorMac> device_monitor_mac_;
#endif

  // Destroy parts_ before main_message_loop_ (required) and before other
  // classes constructed in content (but after main_thread_).
  scoped_ptr<BrowserMainParts> parts_;

  // Members initialized in |InitializeMainThread()| ---------------------------
  // This must get destroyed before other threads that are created in parts_.
  scoped_ptr<BrowserThreadImpl> main_thread_;

  // Members initialized in |BrowserThreadsStarted()| --------------------------
  scoped_ptr<ResourceDispatcherHostImpl> resource_dispatcher_host_;
  scoped_ptr<SpeechRecognitionManagerImpl> speech_recognition_manager_;

  // Members initialized in |RunMainMessageLoopParts()| ------------------------
  scoped_ptr<BrowserProcessSubThread> db_thread_;
#if !defined(OS_IOS)
  scoped_ptr<WebKitThread> webkit_thread_;
#endif
  scoped_ptr<BrowserProcessSubThread> file_user_blocking_thread_;
  scoped_ptr<BrowserProcessSubThread> file_thread_;
  scoped_ptr<BrowserProcessSubThread> process_launcher_thread_;
  scoped_ptr<BrowserProcessSubThread> cache_thread_;
  scoped_ptr<BrowserProcessSubThread> io_thread_;
  scoped_ptr<MemoryObserver> memory_observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMainLoop);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
