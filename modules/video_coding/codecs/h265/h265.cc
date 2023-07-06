#include "modules/video_coding/codecs/h265/include/h265.h"

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/media_constants.h"
#include "rtc_base/trace_event.h"

#if defined(WEBRTC_USE_H265)
#include "modules/video_coding/codecs/h265/h265_decoder_impl.h"
#include "modules/video_coding/codecs/h265/h265_encoder_impl.h"
#endif

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

#ifdef WEBRTC_USE_H265_REMAIN

constexpr absl::string_view kSupportedScalabilityModes[] = {"L1T2", "L1T3"};

std::unique_ptr<H265Encoder> H265Encoder::Create(
    const cricket::VideoCodec& codec) {
  RTC_DCHECK(H265Encoder::IsSupported());
#if defined(WEBRTC_USE_H264)
  RTC_LOG(LS_INFO) << "Creating H265EncoderImpl.";
  return std::make_unique<H265EncoderImpl>(codec);
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

bool H265Encoder::IsSupported() {
  return true;
}

bool H265Encoder::SupportsScalabilityMode(absl::string_view scalability_mode) {
  for (const auto& entry : kSupportedScalabilityModes) {
    if (entry == scalability_mode) {
      return true;
    }
  }
  return false;
}

#endif

std::unique_ptr<H265Decoder> H265Decoder::Create() {
  RTC_DCHECK(H265Decoder::IsSupported());
#if defined(WEBRTC_USE_H265)
  RTC_LOG(LS_INFO) << "Creating H265DecoderImpl.";
  return std::make_unique<H265DecoderImpl>();
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

bool H265Decoder::IsSupported() {
  return true;
}

}  // namespace webrtc
