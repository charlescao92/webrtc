/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// clang-format off
// clang formating would change include order.
#include <windows.h>
#include <shellapi.h>  // must come after windows.h
// clang-format on

#include <string>
#include <vector>

#include "absl/flags/parse.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/string_utils.h"  // For ToUtf8
#include "rtc_base/win32_socket_init.h"
#include "rtc_base/win32_socket_server.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"

#include "examples/crtc/tools/flag_defs.h"
#include "examples/crtc/tools/logstreamfile.hpp"

#include "examples/crtc/crtcplayer/win/main_wnd.h"
#include "examples/crtc/crtcplayer/rtcplayer.h"

int PASCAL wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* cmd_line,
                    int cmd_show) {
  // 初始化日记
  rtc::LogMessage::LogToDebug(rtc::LS_INFO);
  rtc::LogMessage::SetLogToStderr(true);
  rtc::LogMessage::LogTimestamps(true);
  rtc::LogMessage::LogThreads(true);
  std::unique_ptr<utils::LogStreamFile> pLogStreamFile_ = std::make_unique<utils::LogStreamFile>("crtcplayer");
  pLogStreamFile_->Init();

  // 初始化网络库
  rtc::WinsockInitializer winsock_init;
  rtc::Win32SocketServer w32_ss;
  rtc::Win32Thread w32_thread(&w32_ss);
  rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
  rtc::InitializeSSL();

  MainWnd wnd;
  if (!wnd.Create()) {
    RTC_NOTREACHED();
    return -1;
  }

  rtc::scoped_refptr<crtcplayer::WebRtcPlayer> webrtcPlayer(
      new rtc::RefCountedObject<crtcplayer::WebRtcPlayer>(&wnd));
  webrtcPlayer->Initalize();

  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
    if (!wnd.PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  rtc::CleanupSSL();
  return 0;
}