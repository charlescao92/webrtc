#ifndef MODULES_VIDEO_CODING_CODECS_H265_H265_DECODER_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_H265_H265_DECODER_IMPL_H_

#ifdef WEBRTC_USE_H265

#if defined(WEBRTC_WIN) && !defined(__clang__)
#error "See: bugs.webrtc.org/9213#c13."
#endif

#include <memory>

#include "modules/video_coding/codecs/h265/include/h265.h"

extern "C" {
#include "third_party/ffmpeg/libavcodec/avcodec.h"
}  // extern "C"

#include "common_video/h265/h265_bitstream_parser.h"
#include "common_video/include/video_frame_buffer_pool.h"

namespace webrtc {

struct AVCodecContextDeleter {
  void operator()(AVCodecContext* ptr) const { avcodec_free_context(&ptr); }
};
struct AVFrameDeleter {
  void operator()(AVFrame* ptr) const { av_frame_free(&ptr); }
};

class H265DecoderImpl : public H265Decoder {
 public:
  H265DecoderImpl();
  ~H265DecoderImpl() override;

  bool Configure(const Settings& settings) override;
  int32_t Release() override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  // `missing_frames`, `fragmentation` and `render_time_ms` are ignored.
  int32_t Decode(const EncodedImage& input_image,
                 bool /*missing_frames*/,
                 int64_t render_time_ms = -1) override;

  const char* ImplementationName() const override;

 private:
  // Called by FFmpeg when it needs a frame buffer to store decoded frames in.
  // The `VideoFrame` returned by FFmpeg at `Decode` originate from here. Their
  // buffers are reference counted and freed by FFmpeg using `AVFreeBuffer2`.
  static int AVGetBuffer2(AVCodecContext* context,
                          AVFrame* av_frame,
                          int flags);
  // Called by FFmpeg when it is done with a video frame, see `AVGetBuffer2`.
  static void AVFreeBuffer2(void* opaque, uint8_t* data);

  bool IsInitialized() const;

  // Reports statistics with histograms.
  void ReportInit();
  void ReportError();

  // Used by ffmpeg via `AVGetBuffer2()` to allocate I420 images.
  VideoFrameBufferPool ffmpeg_buffer_pool_;
  // Used to allocate NV12 images if NV12 output is preferred.
  VideoFrameBufferPool output_buffer_pool_;
  std::unique_ptr<AVCodecContext, AVCodecContextDeleter> av_context_;
  std::unique_ptr<AVFrame, AVFrameDeleter> av_frame_;

  DecodedImageCallback* decoded_image_callback_;

  bool has_reported_init_;
  bool has_reported_error_;

  webrtc::H265BitstreamParser h265_bitstream_parser_;

  // Decoder should produce this format if possible.
  const VideoFrameBuffer::Type preferred_output_format_;
};

}  // namespace webrtc

#endif  // WEBRTC_USE_H265

#endif  // MODULES_VIDEO_CODING_CODECS_H265_H265_DECODER_IMPL_H_
