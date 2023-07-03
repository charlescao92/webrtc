
#ifndef CRTC_RTC_PUSHER_HPP
#define CRTC_RTC_PUSHER_HPP

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "examples/crtc/crtcpusher/win/main_wnd.h"
#include "examples/crtc/tools/utils.h"

namespace rtcpusher {

class CRtcStatsCollector : public webrtc::RTCStatsCollectorCallback {
 public:
  virtual void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
};

class WebRtcPusher : public webrtc::PeerConnectionObserver,
                     public webrtc::CreateSessionDescriptionObserver {
 public:
  WebRtcPusher(MainWindow* main_wnd);

  void Initalize();

   void GetRtcStats();

  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;

  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams) override;

  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;

  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;

  void OnRenegotiationNeeded() override;

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;

  // SRS²»ÐèÒª
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  void OnIceConnectionReceivingChange(bool receiving) override;

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;

  void OnFailure(webrtc::RTCError error) override;


protected:
    ~WebRtcPusher();

     void DeletePeerConnection();

 private:
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    MainWindow* main_wnd_;
    Tools tools_;
};
}

#endif  // CRTC_RTC_PUSHER_HPP
