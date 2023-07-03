class WebrtcClientPlayer
    : public webrtc::PeerConnectionObserver
    , public webrtc::CreateSessionDescriptionObserver
{
public:
    WebrtcClientPlayer() {}

    void Initalize()
    {
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
        //webrtc::PeerConnectionInterface::IceServer server;
        //server.uri = "stun:stun.l.google.com:19302";
        //config.servers.push_back(server);

        peer_connection_ = peer_connection_factory_->CreatePeerConnection(
            config, nullptr, nullptr, this);

        //这里的顺序很重要，必须先设置音频，再设置视频，否则在调用peer_connection_->SetRemoteDescription设置Answer SDP时会报媒体类型匹配顺序不一致的错误
        //SRS回复的SDP中，是先定义音频m=行，再定义视频m=行的
        webrtc::RtpTransceiverInit rtpTransceiverInit;
        rtpTransceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, rtpTransceiverInit);
        peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, rtpTransceiverInit);

        //创建Offer SDP成功的话，会回调void OnSuccess(webrtc::SessionDescriptionInterface* desc)
        peer_connection_->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }


    // PeerConnectionObserver implementation.
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override {
        RTC_LOG(INFO) << __FUNCTION__;
    }
    void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) override {
        RTC_LOG(INFO) << __FUNCTION__;
        auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(receiver->track().release());
        if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
            auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
            remote_renderer_.reset(new VideoRenderer(gWnd, 1, 1, video_track));
        }
        track->Release();
    };
    void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {
        RTC_LOG(INFO) << __FUNCTION__;
    };
    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
        RTC_LOG(INFO) << __FUNCTION__;
    }
    void OnRenegotiationNeeded() override {
        RTC_LOG(INFO) << __FUNCTION__;
    }
    void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
        RTC_LOG(INFO) << __FUNCTION__;
    }
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
        RTC_LOG(INFO) << __FUNCTION__;
    }
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
        RTC_LOG(INFO) << __FUNCTION__;

        std::string sdp;
        candidate->ToString(&sdp);
        json iceCandidate = {
                {"sdpMid", candidate->sdp_mid()},
                {"sdpMLineIndex",  candidate->sdp_mline_index()},
                {"candidate", sdp}
        };
    };
    void OnIceConnectionReceivingChange(bool receiving) override {
        RTC_LOG(INFO) << __FUNCTION__;
    }

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        RTC_LOG(INFO) << __FUNCTION__;
        peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);


        //SRS 4.0 only support h.264 in sdp
        std::string sdpOffer;
        desc->ToString(&sdpOffer);

        //send offer sdp to srs4.0
        json reqMsg = {
                {"api", "http://10.5.223.26:1985/rtc/v1/play/"},
                //{"tid", tid},
                //{"clientip", "null"},
                {"streamurl",  "webrtc://10.5.223.26/live/livestream"},
                {"sdp", sdpOffer}
        };

        std::string url = "http://10.5.223.26:1985/rtc/v1/play/";
        std::string response;
        std::string httpRequestBody = reqMsg.dump();
        RTC_LOG(INFO) << "http request : " << httpRequestBody;

        RTC::ErrorCode rtVal = httpClient.Post(url, httpRequestBody, HttpClient::ContentType::Json, response);
        if (RTC::ErrorCode::EC_OK != rtVal)
        {
            RTC_LOG(INFO) << "http post error : " << rtVal;
            return;
        }
        json jsonContent = json::parse(response);

        RTC_LOG(INFO) << "http response : " << response;

        int code;
        GET_JSON_VAL(jsonContent, code, code, int);
        RTC_LOG(LERROR) << "http post code : " << code;

        if (code == 0) {
            std::string sdpAnswer;
            GET_JSON_VAL(jsonContent, sdp, sdpAnswer, std::string);

            std::string sessionid;
            GET_JSON_VAL(jsonContent, sessionid, sessionid, std::string);

            std::string server;
            GET_JSON_VAL(jsonContent, server, server, std::string);

            webrtc::SdpParseError error;
            webrtc::SdpType type = webrtc::SdpType::kAnswer;
            std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
                webrtc::CreateSessionDescription(type, sdpAnswer, &error);

            //设置Answer SDP成功的话，会回调void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver, 
            //                                            const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
            peer_connection_->SetRemoteDescription(
                DummySetSessionDescriptionObserver::Create(),
                session_description.release());
        }
    };
    void OnFailure(webrtc::RTCError error) override {
        RTC_LOG(INFO) << __FUNCTION__;
    };


private:
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    HttpClient httpClient;
};
