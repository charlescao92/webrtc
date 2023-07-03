#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "examples/crtc/crtcplayer/rtcplayer.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "test/vcm_capturer.h"
#include "rtc_base/strings/json.h"

namespace {

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static DummySetSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
  virtual void OnFailure(webrtc::RTCError error) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
                  << error.message();
  }
};

class CapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<CapturerTrackSource> Create() {
    const size_t kWidth = 640;
    const size_t kHeight = 480;
    const size_t kFps = 30;
    std::unique_ptr<webrtc::test::VcmCapturer> capturer;
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
      return nullptr;
    }
    int num_devices = info->NumberOfDevices();
    for (int i = 0; i < num_devices; ++i) {
      capturer = absl::WrapUnique(
          webrtc::test::VcmCapturer::Create(kWidth, kHeight, kFps, i));
      if (capturer) {
        return new rtc::RefCountedObject<CapturerTrackSource>(
            std::move(capturer));
      }
    }

    return nullptr;
  }

 protected:
  explicit CapturerTrackSource(
      std::unique_ptr<webrtc::test::VcmCapturer> capturer)
      : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return capturer_.get();
  }
  std::unique_ptr<webrtc::test::VcmCapturer> capturer_;
};

}  // namespace

namespace rtcplayer {

WebRtcPlayer::WebRtcPlayer(MainWindow* main_wnd) : main_wnd_(main_wnd) {}

WebRtcPlayer::~WebRtcPlayer() {
  RTC_DCHECK(!peer_connection_);
}

void WebRtcPlayer::Initalize() {
  RTC_LOG(INFO) << __FUNCTION__;
  peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
      nullptr /* network_thread */, nullptr /* worker_thread */,
      nullptr /* signaling_thread */, nullptr /* default_adm */,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
      nullptr /* audio_processing */);

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kPlanB;
  config.enable_dtls_srtp = true;
  

  peer_connection_ = peer_connection_factory_->CreatePeerConnection(
      config, nullptr, nullptr, this);

  //这里的顺序很重要，必须先设置音频，再设置视频，否则在调用peer_connection_->SetRemoteDescription设置Answer SDP时会报媒体类型匹配顺序不一致的错误
  //SRS回复的SDP中，是先定义音频m=行，再定义视频m=行的
  // /顺序不对的报错
  //[000:278][12372] (sdp_offer_answer.cc:3081): The order of m-lines in answer doesn't match order in offer. Rejecting answer. (INVALID_PARAMETER)
  // [000:278][12372] (sdp_offer_answer.cc:2192): Failed to set remote answer sdp: The order of m-lines in answer doesn't match order in offer. Rejecting answer.
  // ***
  // [000:293][16088] (rtcplayer.cc:60): OnFailure INVALID_PARAMETER: Failed to set remote answer sdp: The order of m-lines in answer doesn't match order in offer. Rejecting answer.
  webrtc::RtpTransceiverInit rtpTransceiverInit;
  rtpTransceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
  peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO,
                                   rtpTransceiverInit);
  peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, 
                                    rtpTransceiverInit);


  //创建Offer SDP成功的话，会回调void OnSuccess(webrtc::SessionDescriptionInterface* desc)
  peer_connection_->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

}

// PeerConnectionObserver implementation.
void WebRtcPlayer::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(INFO) << __FUNCTION__;
}

void WebRtcPlayer::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) {
  RTC_LOG(INFO) << __FUNCTION__;

  auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(
      receiver->track().release());

  if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
    auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
    //渲染远端视频
    main_wnd_->StartRemoteRenderer(video_track);
  }

  track->Release(); 
}

void WebRtcPlayer::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  RTC_LOG(INFO) << __FUNCTION__;
}

void WebRtcPlayer::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  RTC_LOG(INFO) << __FUNCTION__;
}

void WebRtcPlayer::OnRenegotiationNeeded() {
  RTC_LOG(INFO) << __FUNCTION__;
}
void WebRtcPlayer::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(INFO) << __FUNCTION__;
}
void WebRtcPlayer::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(INFO) << __FUNCTION__;
}

// SRS不需要
void WebRtcPlayer::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(INFO) << __FUNCTION__;

}

void WebRtcPlayer::OnIceConnectionReceivingChange(bool receiving) {
  RTC_LOG(INFO) << __FUNCTION__;
}

bool StartPostRequest(const std::string& requestUrl, Json::Value& jsonObject) {
  RTC_LOG(INFO) << "http request : " << requestUrl;

  std::string response;

  // HTTP请求使用libcurl
  Http::HTTPERROR errCode = net::httpExecRequest("POST", requestUrl, response);
  if (errCode != Http::HTTPERROR_SUCCESS) {
    RTC_LOG(INFO) << "http post error : " << net::httpErrorString(errCode);
    return false;
  }

  Json::Reader reader;
  if (!reader.parse(response, jsonObject)) {
    RTC_LOG(WARNING) << "Received unknown message： " << response;
    return false;
  }

  RTC_LOG(INFO) << "http response : " << response;

  int errNo;
  if (!rtc::GetIntFromJsonObject(jsonObject, "errNo", &errNo)) {
    RTC_LOG(WARNING) << "Message no has errNo param";
    return false;
  }

  // 异常直接退出
  if (errNo != 0) {
    // MessageBox(NULL, _T("推流申请Post返回失败"), _T("警告"), MB_OK);
    MessageBox(NULL, _T("Play Post Error"), _T("Error"), MB_OK);
    return false;
  }
  return true;
}

// CreateSessionDescriptionObserver implementation.
void WebRtcPlayer::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    RTC_LOG(INFO) << __FUNCTION__ ;

    peer_connection_->SetLocalDescription(
    DummySetSessionDescriptionObserver::Create(), desc);

    std::string sdpAnswer;
    desc->ToString(&sdpAnswer);
    RTC_LOG(INFO) << "sdp answer:" << sdpAnswer;

    //const std::string playUrlApi = "http://1.14.148.67:1985/rtc/v1/play/";
    //const std::string playUrlApi = "http://192.168.1.19:1985/rtc/v1/play/";

    std::string playUrlApi = "http://1.14.148.67:8080/signaling/pull";

    const std::string streamName = "xrtc1992";
    const int uid = 111;
    std::string strUid = std::to_string(uid);

    std::string requestPullUrl = playUrlApi +
        "?uid=" + strUid + "&streamName=" + streamName + "&audio=1&video=1";

    Json::Value jmessage;
    bool ok = StartPostRequest(requestPullUrl, jmessage);
    if (!ok) {
      MessageBox(NULL, _T("Send pull request error"), _T("Error"), MB_OK);
      exit(1);
    }

    std::string offer;
    Json::Value json_object = jmessage["data"];

    // {"errNo":0,"errMsg":"success","data":{"type":"offer","sdp":"v=0\r\no=- 0 2 IN ****
    rtc::GetStringFromJsonObject(json_object, "sdp", &offer);
    if (offer.empty()) {
      RTC_LOG(WARNING) << "No offer info......";
      return;
    }

     webrtc::SdpParseError error;
     webrtc::SdpType type = webrtc::SdpType::kOffer;
     std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
          webrtc::CreateSessionDescription(type, offer, &error);

     //设置Answer SDP成功的话，会回调OnAddTrack
     peer_connection_->SetRemoteDescription(
          DummySetSessionDescriptionObserver::Create(),
          session_description.release());
        
     // 发送answer给xrtcserver
     std::string strType = "pull";
     std::string requestAnswerUrl = playUrlApi + 
                                    "?uid=" + strUid +
                                    "&streamName=" + streamName +
                                    "&answer=" + sdpAnswer + 
                                    "&type=" + strType;
     Json::Value jmessage1;
     ok = StartPostRequest(requestAnswerUrl, jmessage1);
     if (!ok) {
        MessageBox(NULL, _T("Send Answer Error"), _T("Error"), MB_OK);
        exit(1);
     }
}

void WebRtcPlayer::OnFailure(webrtc::RTCError error) {
  RTC_LOG(INFO) << __FUNCTION__;
}

void WebRtcPlayer::DeletePeerConnection() {
  main_wnd_->StopLocalRenderer();
  main_wnd_->StopRemoteRenderer();
  peer_connection_ = nullptr;
  peer_connection_factory_ = nullptr;
}

}  // namespace rtcplayer
