/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_H_
#define MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct SdpVideoFormat;

#ifdef WEBRTC_USE_H265_REMAIN
class RTC_EXPORT H265Encoder : public VideoEncoder {
 public:
  static std::unique_ptr<H265Encoder> Create(const cricket::VideoCodec& codec);
  // If H.264 is supported (any implementation).
  static bool IsSupported();
  static bool SupportsScalabilityMode(absl::string_view scalability_mode);

  ~H265Encoder() override {}
};
#endif

class RTC_EXPORT H265Decoder : public VideoDecoder {
 public:
  static std::unique_ptr<H265Decoder> Create();
  static bool IsSupported();

  ~H265Decoder() override {}
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_H_
