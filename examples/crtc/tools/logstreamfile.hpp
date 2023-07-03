/**
 * Copyright (c) 2018 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef LOG_STREAM_HPP
#define LOG_STREAM_HPP

#include <string>
#include <memory>
#include "rtc_base/log_sinks.h"

namespace utils {

class LogStreamFile {
 public:
  LogStreamFile(const std::string& logPreffix = "");
  virtual ~LogStreamFile();

  void Init();

 private:
  std::unique_ptr<rtc::FileRotatingLogSink> fileLogSink_ = NULL;
};

}

#endif
