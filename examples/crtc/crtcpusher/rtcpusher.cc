#include "examples/crtc/crtcpusher/rtcpusher.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>
#include <thread>

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
#include "pc/rtc_stats_collector.h"

namespace {
const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamId[] = "stream_id";

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

//定时器获取状态GetStats
static void mytimer(void* args) {
  BOOL bRet = FALSE;
  MSG msg = {0};

  UINT timerid = SetTimer(NULL, 0, 1000, NULL);

  while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
    if (bRet == -1) {
      // handle the error and possibly exit
    } else {
      if (msg.message == WM_TIMER) {
        if (msg.wParam == timerid) {
          rtcpusher::WebRtcPusher* pusher = (rtcpusher::WebRtcPusher*)args;
          pusher->GetRtcStats();
        }
      } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }
  KillTimer(NULL, timerid);
}

}  // namespace

namespace rtcpusher {

    void CRtcStatsCollector::OnStatsDelivered(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  
    Json::Reader reader;

    for (auto it = report->begin(); it != report->end(); ++it) {
      // "type" : "inbound-rtp"
      Json::Value jmessage;
      if (!reader.parse(it->ToJson(), jmessage)) {
        RTC_LOG(WARNING) << "stats report invalid!!!";
        return;
      }

      std::string type = jmessage["type"].asString();
      if (type == "outbound-rtp") {
        RTC_LOG(INFO) << "outbound-rtp : " << it->ToJson();

        uint64_t rtt_ms = jmessage["rttMs"].asUInt64();
        uint64_t packetsLost = jmessage["packetsLost"].asUInt64();
        double fractionLost = jmessage["fractionLost"].asDouble();

        RTC_LOG(INFO) << "rttMs:" << rtt_ms << ", packetsLost:" << packetsLost
                      << ", fractionLost: " << fractionLost;

      }
    }
}

    WebRtcPusher::WebRtcPusher(MainWindow* main_wnd)
        : main_wnd_(main_wnd) {
    }

    WebRtcPusher::~WebRtcPusher() {
        RTC_DCHECK(!peer_connection_);
    }

    void WebRtcPusher::Initalize() {
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
        config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
        config.enable_dtls_srtp = true;
    
        peer_connection_ = peer_connection_factory_->CreatePeerConnection(
            config, nullptr, nullptr, this);

                webrtc::RtpTransceiverInit rtpTransceiverInit;
        rtpTransceiverInit.direction =
            webrtc::RtpTransceiverDirection::kSendOnly;
        peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO,
                                         rtpTransceiverInit);
        peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                                         rtpTransceiverInit);
    
        rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          peer_connection_factory_->CreateAudioTrack(
              kAudioLabel, peer_connection_factory_->CreateAudioSource(
                               cricket::AudioOptions())));
        peer_connection_->AddTrack(audio_track, {kStreamId});
    
        rtc::scoped_refptr<CapturerTrackSource> video_device =
            CapturerTrackSource::Create();
        if (video_device) {
            rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
            peer_connection_factory_->CreateVideoTrack(kVideoLabel, video_device));
    
            //准备渲染本地视频
            main_wnd_->StartLocalRenderer(video_track_);
    
            peer_connection_->AddTrack(video_track_, {kStreamId});
        }

       peer_connection_->CreateOffer(
            this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }

    void WebRtcPusher::GetRtcStats() {
      rtc::scoped_refptr<CRtcStatsCollector> stats(
          new rtc::RefCountedObject<CRtcStatsCollector>());
      peer_connection_->GetStats(stats);
    }

    // PeerConnectionObserver implementation.
    void WebRtcPusher::OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) {
      RTC_LOG(INFO) << __FUNCTION__;
    }
    
    void WebRtcPusher::OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
            streams) {
      RTC_LOG(INFO) << __FUNCTION__;
    
    }

    void WebRtcPusher::OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
      RTC_LOG(INFO) << __FUNCTION__;
    }
    
    void WebRtcPusher::OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
      RTC_LOG(INFO) << __FUNCTION__;
    }
    
    void WebRtcPusher::OnRenegotiationNeeded() {
      RTC_LOG(INFO) << __FUNCTION__;
    }

    void WebRtcPusher::OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state) {
      RTC_LOG(INFO) << __FUNCTION__;
    }

    void WebRtcPusher::OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) {
      RTC_LOG(INFO) << __FUNCTION__;
    }
    
    // SRS不需要
    void WebRtcPusher::OnIceCandidate(
        const webrtc::IceCandidateInterface* candidate) {
        RTC_LOG(INFO) << __FUNCTION__;
    
    }
    
    void WebRtcPusher::OnIceConnectionReceivingChange(bool receiving) {
      RTC_LOG(INFO) << __FUNCTION__;
    }
    
    // CreateSessionDescriptionObserver implementation.
    void WebRtcPusher::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
        
      RTC_LOG(INFO) << __FUNCTION__;

      peer_connection_->SetLocalDescription(
          DummySetSessionDescriptionObserver::Create(), desc);

      // SRS 4.0 only support h.264 in sdp
      std::string sdpOffer;
      desc->ToString(&sdpOffer);
      RTC_LOG(INFO) << "sdp Offer:" << sdpOffer;

      const std::string playUrlApi = "http://1.14.148.67:1985/rtc/v1/publish/";

      // send offer sdp to srs4.0
      std::string userName = tools_.RandString(8);

      Json::StyledWriter writer;
      Json::Value reqMsg;
      reqMsg["api"] = playUrlApi;
      reqMsg["streamurl"] = "webrtc://1.14.148.67/live/livestream";
      reqMsg["sdp"] = sdpOffer;
      Json::String str = writer.write(reqMsg);

      std::string response;
      std::string httpRequestBody = str.c_str();

      RTC_LOG(INFO) << "http request : " << httpRequestBody;

      // HTTP请求使用libcurl
      Http::HTTPERROR errCode =
          net::httpExecRequest("POST", playUrlApi, response, httpRequestBody);
      if (errCode != Http::HTTPERROR_SUCCESS) {
        RTC_LOG(INFO) << "http post error : " << net::httpErrorString(errCode);
        return;
      }

      Json::Reader reader;
      Json::Value jmessage;
      if (!reader.parse(response, jmessage)) {
        RTC_LOG(WARNING) << "Received unknown message. " << response;
        return;
      }

      RTC_LOG(INFO) << "http response : " << response;

      int code;
      rtc::GetIntFromJsonObject(jmessage, "code", &code);

      // 异常直接退出
      if (code != 0) {
        MessageBox(NULL, _T("Play Post Error"), _T("Error"), MB_OK);
        exit(1);
      }

      std::string sdpAnswer;
      rtc::GetStringFromJsonObject(jmessage, "sdp", &sdpAnswer);

      webrtc::SdpParseError error;
      webrtc::SdpType type = webrtc::SdpType::kAnswer;
      std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
          webrtc::CreateSessionDescription(type, sdpAnswer, &error);

      //设置Answer SDP成功的话，会回调OnAddTrack
      peer_connection_->SetRemoteDescription(
          DummySetSessionDescriptionObserver::Create(),
          session_description.release());  

      std::thread td(mytimer, this);
      td.detach();

    }
    
    void WebRtcPusher::OnFailure(webrtc::RTCError error) {
      RTC_LOG(INFO) << __FUNCTION__;
    }

    void WebRtcPusher::DeletePeerConnection() {
      main_wnd_->StopLocalRenderer();
      //main_wnd_->StopRemoteRenderer();
      peer_connection_ = nullptr;
      peer_connection_factory_ = nullptr;
 /*     peer_id_ = -1;
      loopback_ = false;*/
    }

}

