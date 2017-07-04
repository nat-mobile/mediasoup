#define MS_CLASS "RTC::Room"
// #define MS_LOG_DEV

#include "RTC/Room.hpp"
#include "Logger.hpp"
#include "Settings.hpp"
#include "MediaSoupError.hpp"
#include "Utils.hpp"
#include "RTC/VP9Filter.hpp"
#include <cmath> // std::lround()
#include <set>
#include <string>
#include <vector>
#include <iostream>


namespace RTC
{
	/* Static. */

	static constexpr uint64_t AudioLevelsInterval{ 1000 }; // In ms.
    
    static constexpr uint8_t ActiveSpeakerVoiceDiff{ 15 }; // In ms.

	/* Class variables. */

	RTC::RtpCapabilities Room::supportedRtpCapabilities;

	/* Class methods. */

	void Room::ClassInit()
	{
		MS_TRACE();

		// Parse all RTP capabilities.
		{
			// NOTE: These lines are auto-generated from data/supportedCapabilities.js.
			const std::string supportedRtpCapabilities =
			    R"({"codecs":[{"kind":"audio","name":"audio/opus","clockRate":48000,"numChannels":2,"rtcpFeedback":[]},{"kind":"audio","name":"audio/PCMU","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/PCMA","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/ISAC","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/ISAC","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/G722","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/iLBC","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":24000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":12000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":48000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":8000,"rtcpFeedback":[]},{"kind":"video","name":"video/VP8","clockRate":90000,"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/VP9","clockRate":90000,"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/H264","clockRate":90000,"parameters":{"packetizationMode":0},"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/H264","clockRate":90000,"parameters":{"packetizationMode":1},"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/H265","clockRate":90000,"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]}],"headerExtensions":[{"kind":"audio","uri":"urn:ietf:params:rtp-hdrext:ssrc-audio-level","preferredId":1,"preferredEncrypt":false},{"kind":"video","uri":"urn:ietf:params:rtp-hdrext:toffset","preferredId":2,"preferredEncrypt":false},{"kind":"","uri":"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time","preferredId":3,"preferredEncrypt":false},{"kind":"video","uri":"urn:3gpp:video-orientation","preferredId":4,"preferredEncrypt":false},{"kind":"","uri":"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id","preferredId":5,"preferredEncrypt":false}],"fecMechanisms":[]})";

			Json::CharReaderBuilder builder;
			Json::Value settings = Json::nullValue;

			builder.strictMode(&settings);

			Json::CharReader* jsonReader = builder.newCharReader();
			Json::Value json;
			std::string jsonParseError;

			if (!jsonReader->parse(
			        supportedRtpCapabilities.c_str(),
			        supportedRtpCapabilities.c_str() + supportedRtpCapabilities.length(),
			        &json,
			        &jsonParseError))
			{
				delete jsonReader;

				MS_THROW_ERROR_STD(
				    "JSON parsing error in supported RTP capabilities: %s", jsonParseError.c_str());
			}
			else
			{
				delete jsonReader;
			}

			try
			{
				Room::supportedRtpCapabilities = RTC::RtpCapabilities(json, RTC::Scope::ROOM_CAPABILITY);
			}
			catch (const MediaSoupError& error)
			{
				MS_THROW_ERROR_STD("wrong supported RTP capabilities: %s", error.what());
			}
            
		}
	}

	/* Instance methods. */

	Room::Room(Listener* listener, Channel::Notifier* notifier, uint32_t roomId, Json::Value& data)
	    : roomId(roomId), listener(listener), notifier(notifier)
	{
		MS_TRACE();

		static const Json::StaticString JsonStringMediaCodecs{ "mediaCodecs" };

		// `mediaCodecs` is optional.
		if (data[JsonStringMediaCodecs].isArray())
		{
			auto& jsonMediaCodecs = data[JsonStringMediaCodecs];
			std::vector<RTC::RtpCodecParameters> mediaCodecs;

			for (auto& jsonMediaCodec : jsonMediaCodecs)
			{
				RTC::RtpCodecParameters mediaCodec(jsonMediaCodec, RTC::Scope::ROOM_CAPABILITY);

				// Ignore feature codecs.
				if (mediaCodec.mime.IsFeatureCodec())
					continue;

				// Check whether the given media codec is supported by mediasoup. If not
				// ignore it.
				for (auto& supportedMediaCodec : Room::supportedRtpCapabilities.codecs)
				{
					if (supportedMediaCodec.Matches(mediaCodec))
					{
						// Copy the RTCP feedback.
						mediaCodec.rtcpFeedback = supportedMediaCodec.rtcpFeedback;

						mediaCodecs.push_back(mediaCodec);

						break;
					}
				}
			}

			// Set room RTP capabilities.
			// NOTE: This may throw.
			SetCapabilities(mediaCodecs);
		}

		// Set the audio levels timer.
		this->audioLevelsTimer = new Timer(this);
        
        // create selector if needed
        if (Settings::configuration.vp9MinTemporial < 2 || Settings::configuration.vp9MinSpartial < 1)
            needToFilterLayers = true;
        
        // audio levels filtration
        needToFilterAudioLevels = Settings::configuration.needToFilterAudioLevels;
	}

	Room::~Room()
	{
		MS_TRACE();
        for (auto ls : mapRtpReceiverLayerSelector)
            delete ls.second;
	}

	void Room::Destroy()
	{
		MS_TRACE();

		static const Json::StaticString JsonStringClass{ "class" };

		Json::Value eventData(Json::objectValue);

		// Close all the Peers.
		// NOTE: Upon Peer closure the onPeerClosed() method is called which
		// removes it from the map, so this is the safe way to iterate the map
		// and remove elements.
		for (auto it = this->peers.begin(); it != this->peers.end();)
		{
			auto* peer = it->second;

			it = this->peers.erase(it);
			peer->Destroy();
		}

		// Close the audio level timer.
		this->audioLevelsTimer->Destroy();

		// Notify.
		eventData[JsonStringClass] = "Room";
		this->notifier->Emit(this->roomId, "close", eventData);

		// Notify the listener.
		this->listener->OnRoomClosed(this);

		delete this;
	}

	Json::Value Room::ToJson() const
	{
		MS_TRACE();

		static const Json::StaticString JsonStringRoomId{ "roomId" };
		static const Json::StaticString JsonStringCapabilities{ "capabilities" };
		static const Json::StaticString JsonStringPeers{ "peers" };
		static const Json::StaticString JsonStringMapRtpReceiverRtpSenders{ "mapRtpReceiverRtpSenders" };
		static const Json::StaticString JsonStringMapRtpSenderRtpReceiver{ "mapRtpSenderRtpReceiver" };
		static const Json::StaticString JsonStringAudioLevelsEventEnabled{ "audioLevelsEventEnabled" };

		Json::Value json(Json::objectValue);
		Json::Value jsonPeers(Json::arrayValue);
		Json::Value jsonMapRtpReceiverRtpSenders(Json::objectValue);
		Json::Value jsonMapRtpSenderRtpReceiver(Json::objectValue);

		// Add `roomId`.
		json[JsonStringRoomId] = Json::UInt{ this->roomId };

		// Add `capabilities`.
		json[JsonStringCapabilities] = this->capabilities.ToJson();

		// Add `peers`.
		for (auto& kv : this->peers)
		{
			auto* peer = kv.second;

			jsonPeers.append(peer->ToJson());
		}
		json[JsonStringPeers] = jsonPeers;

		// Add `mapRtpReceiverRtpSenders`.
		for (auto& kv : this->mapRtpReceiverRtpSenders)
		{
			auto rtpReceiver = kv.first;
			auto& rtpSenders = kv.second;
			Json::Value jsonRtpReceivers(Json::arrayValue);

			for (auto& rtpSender : rtpSenders)
			{
				jsonRtpReceivers.append(std::to_string(rtpSender->rtpSenderId));
			}

			jsonMapRtpReceiverRtpSenders[std::to_string(rtpReceiver->rtpReceiverId)] = jsonRtpReceivers;
		}
		json[JsonStringMapRtpReceiverRtpSenders] = jsonMapRtpReceiverRtpSenders;

		// Add `mapRtpSenderRtpReceiver`.
		for (auto& kv : this->mapRtpSenderRtpReceiver)
		{
			auto rtpSender   = kv.first;
			auto rtpReceiver = kv.second;

			jsonMapRtpSenderRtpReceiver[std::to_string(rtpSender->rtpSenderId)] =
			    std::to_string(rtpReceiver->rtpReceiverId);
		}
		json[JsonStringMapRtpSenderRtpReceiver] = jsonMapRtpSenderRtpReceiver;

		json[JsonStringAudioLevelsEventEnabled] = this->audioLevelsEventEnabled;

		return json;
	}

	void Room::HandleRequest(Channel::Request* request)
	{
		MS_TRACE();

		switch (request->methodId)
		{
			case Channel::Request::MethodId::ROOM_CLOSE:
			{
#ifdef MS_LOG_DEV
				uint32_t roomId = this->roomId;
#endif

				Destroy();

				MS_DEBUG_DEV("Room closed [roomId:%" PRIu32 "]", roomId);

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::ROOM_DUMP:
			{
				auto json = ToJson();

				request->Accept(json);

				break;
			}

			case Channel::Request::MethodId::ROOM_CREATE_PEER:
			{
				static const Json::StaticString JsonStringPeerName{ "peerName" };

				RTC::Peer* peer;
				uint32_t peerId;
				std::string peerName;

				try
				{
					peer = GetPeerFromRequest(request, &peerId);
				}
				catch (const MediaSoupError& error)
				{
					request->Reject(error.what());

					return;
				}

				if (peer != nullptr)
				{
					request->Reject("Peer already exists");

					return;
				}

				if (!request->internal[JsonStringPeerName].isString())
				{
					request->Reject("Request has not string internal.peerName");

					return;
				}

				peerName = request->internal[JsonStringPeerName].asString();

				try
				{
					peer = new RTC::Peer(this, this->notifier, peerId, peerName);
				}
				catch (const MediaSoupError& error)
				{
					request->Reject(error.what());

					return;
				}

				// Store the new Peer.
				this->peers[peerId] = peer;

				MS_DEBUG_DEV("Peer created [peerId:%u, peerName:'%s']", peerId, peerName.c_str());

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::ROOM_SET_AUDIO_LEVELS_EVENT:
			{
				static const Json::StaticString JsonStringEnabled{ "enabled" };

				if (!request->data[JsonStringEnabled].isBool())
				{
					request->Reject("Request has invalid data.enabled");

					return;
				}

				bool audioLevelsEventEnabled = request->data[JsonStringEnabled].asBool();

				if (audioLevelsEventEnabled == this->audioLevelsEventEnabled)
					return;

				// Clear map of audio levels.
				this->mapRtpReceiverAudioLevels.clear();
                this->mapRtpReceiverAudioLevelSelector.clear();

				// Start or stop audio levels periodic timer.
				if (audioLevelsEventEnabled)
					this->audioLevelsTimer->Start(AudioLevelsInterval, AudioLevelsInterval);
				else
					this->audioLevelsTimer->Stop();

				this->audioLevelsEventEnabled = audioLevelsEventEnabled;

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::PEER_CLOSE:
			case Channel::Request::MethodId::PEER_DUMP:
			case Channel::Request::MethodId::PEER_SET_CAPABILITIES:
			case Channel::Request::MethodId::PEER_CREATE_TRANSPORT:
			case Channel::Request::MethodId::PEER_CREATE_RTP_RECEIVER:
			case Channel::Request::MethodId::TRANSPORT_CLOSE:
			case Channel::Request::MethodId::TRANSPORT_DUMP:
			case Channel::Request::MethodId::TRANSPORT_SET_REMOTE_DTLS_PARAMETERS:
			case Channel::Request::MethodId::TRANSPORT_SET_MAX_BITRATE:
			case Channel::Request::MethodId::TRANSPORT_CHANGE_UFRAG_PWD:
			case Channel::Request::MethodId::RTP_RECEIVER_CLOSE:
			case Channel::Request::MethodId::RTP_RECEIVER_DUMP:
			case Channel::Request::MethodId::RTP_RECEIVER_RECEIVE:
			case Channel::Request::MethodId::RTP_RECEIVER_SET_TRANSPORT:
			case Channel::Request::MethodId::RTP_RECEIVER_SET_RTP_RAW_EVENT:
			case Channel::Request::MethodId::RTP_RECEIVER_SET_RTP_OBJECT_EVENT:
			case Channel::Request::MethodId::RTP_SENDER_DUMP:
			case Channel::Request::MethodId::RTP_SENDER_SET_TRANSPORT:
			case Channel::Request::MethodId::RTP_SENDER_DISABLE:
			{
				RTC::Peer* peer;

				try
				{
					peer = GetPeerFromRequest(request);
				}
				catch (const MediaSoupError& error)
				{
					request->Reject(error.what());

					return;
				}

				if (peer == nullptr)
				{
					request->Reject("Peer does not exist");

					return;
				}

				peer->HandleRequest(request);

				break;
			}

			default:
			{
				MS_ERROR("unknown method");

				request->Reject("unknown method");
			}
		}
	}

	RTC::Peer* Room::GetPeerFromRequest(Channel::Request* request, uint32_t* peerId) const
	{
		MS_TRACE();

		static const Json::StaticString JsonStringPeerId{ "peerId" };

		auto jsonPeerId = request->internal[JsonStringPeerId];

		if (!jsonPeerId.isUInt())
			MS_THROW_ERROR("Request has not numeric .peerId field");

		// If given, fill peerId.
		if (peerId != nullptr)
			*peerId = jsonPeerId.asUInt();

		auto it = this->peers.find(jsonPeerId.asUInt());
		if (it != this->peers.end())
		{
			auto* peer = it->second;

			return peer;
		}

		return nullptr;
	}

	void Room::SetCapabilities(std::vector<RTC::RtpCodecParameters>& mediaCodecs)
	{
		MS_TRACE();

		// Set codecs.
		{
			// Available dynamic payload types.
			// clang-format off
			static const std::vector<uint8_t> DynamicPayloadTypes =
			{
				100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
				118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 96,  97,  98,  99,  77,  78,  79,  80,
				81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  35,  36,  37,
				38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
				56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71
			};
			// clang-format om
			// Iterator for available dynamic payload types.
			auto dynamicPayloadTypeIt = DynamicPayloadTypes.begin();
			// Payload types used by the room.
			std::set<uint8_t> roomPayloadTypes;
			// Given media kinds.
			std::set<RTC::Media::Kind> roomKinds;

			// Set the given room codecs.
			for (auto& mediaCodec : mediaCodecs)
			{
				// The room has this kind.
				roomKinds.insert(mediaCodec.kind);

				// Set unique PT.

				// If the codec has PT and it's not already used, let it untouched.
				if (mediaCodec.hasPayloadType &&
				    roomPayloadTypes.find(mediaCodec.payloadType) == roomPayloadTypes.end())
				{
					;
				}
				// Otherwise assign an available PT.
				else
				{
					while (dynamicPayloadTypeIt != DynamicPayloadTypes.end())
					{
						uint8_t payloadType = *dynamicPayloadTypeIt;

						++dynamicPayloadTypeIt;

						if (roomPayloadTypes.find(payloadType) == roomPayloadTypes.end())
						{
							// Assign PT.
							mediaCodec.payloadType    = payloadType;
							mediaCodec.hasPayloadType = true;

							break;
						}
					}

					// If no one found, throw.
					if (!mediaCodec.hasPayloadType)
						MS_THROW_ERROR("no more available dynamic payload types for given media codecs");
				}

				// Store the selected PT.
				roomPayloadTypes.insert(mediaCodec.payloadType);

				// Append the codec to the room capabilities.
				this->capabilities.codecs.push_back(mediaCodec);
			}
		}

		// Add supported RTP header extensions.
		this->capabilities.headerExtensions = Room::supportedRtpCapabilities.headerExtensions;

		// Add supported FEC mechanisms.
		this->capabilities.fecMechanisms = Room::supportedRtpCapabilities.fecMechanisms;
	}

	inline void Room::AddRtpSenderForRtpReceiver(RTC::Peer* senderPeer, const RTC::RtpReceiver* rtpReceiver)
	{
		MS_TRACE();

		MS_ASSERT(senderPeer->HasCapabilities(), "sender peer has no capabilities");
		MS_ASSERT(rtpReceiver->GetParameters(), "rtpReceiver has no parameters");

		uint32_t rtpSenderId = Utils::Crypto::GetRandomUInt(10000000, 99999999);
		auto rtpSender = new RTC::RtpSender(senderPeer, this->notifier, rtpSenderId, rtpReceiver->kind);

		// Store into the maps.
		this->mapRtpReceiverRtpSenders[rtpReceiver].insert(rtpSender);
		this->mapRtpSenderRtpReceiver[rtpSender] = rtpReceiver;

		auto rtpParameters           = rtpReceiver->GetParameters();
		auto associatedRtpReceiverId = rtpReceiver->rtpReceiverId;

		// Attach the RtpSender to the peer.
		senderPeer->AddRtpSender(rtpSender, rtpParameters, associatedRtpReceiverId);
	}
    
    std::string Room::peerByReceiver(const RTC::RtpReceiver* rtpReceiver) const
    {
        MS_TRACE();
        
        for(const auto &peer : peers)
            for (const auto & r : peer.second->GetRtpReceivers())
                if (r == rtpReceiver)
                    return peer.second->peerName;
        return std::string();
    }

	void Room::OnPeerClosed(const RTC::Peer* peer)
	{
		MS_TRACE();

		this->peers.erase(peer->peerId);
	}

	void Room::OnPeerCapabilities(RTC::Peer* peer, RTC::RtpCapabilities* capabilities)
	{
		MS_TRACE();

		// Remove those peer's capabilities not supported by the room.

		// Remove unsupported codecs and set the same PT.
		for (auto it = capabilities->codecs.begin(); it != capabilities->codecs.end();)
		{
			auto& peerCodecCapability = *it;
			auto it2                  = this->capabilities.codecs.begin();

			for (; it2 != this->capabilities.codecs.end(); ++it2)
			{
				auto& roomCodecCapability = *it2;

				if (roomCodecCapability.Matches(peerCodecCapability))
				{
					// Set the same payload type.
					peerCodecCapability.payloadType    = roomCodecCapability.payloadType;
					peerCodecCapability.hasPayloadType = true;

					// Remove the unsupported RTCP feedback from the given codec.
					peerCodecCapability.ReduceRtcpFeedback(roomCodecCapability.rtcpFeedback);

					break;
				}
			}

			if (it2 != this->capabilities.codecs.end())
				++it;
			else
				it = capabilities->codecs.erase(it);
		}

		// Remove unsupported header extensions.
		capabilities->ReduceHeaderExtensions(this->capabilities.headerExtensions);

		// Remove unsupported FEC mechanisms.
		capabilities->ReduceFecMechanisms(this->capabilities.fecMechanisms);

		// Get all the ready RtpReceivers of the others Peers in the Room and
		// create RtpSenders for this new Peer.
		for (auto& kv : this->peers)
		{
			auto* receiverPeer = kv.second;

			for (auto rtpReceiver : receiverPeer->GetRtpReceivers())
			{
				// Skip if the RtpReceiver has not parameters.
				if (rtpReceiver->GetParameters() == nullptr)
					continue;

				AddRtpSenderForRtpReceiver(peer, rtpReceiver);
			}
		}
	}

	void Room::OnPeerRtpReceiverParameters(const RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver)
	{
		MS_TRACE();

		MS_ASSERT(rtpReceiver->GetParameters(), "rtpReceiver->GetParameters() returns no RtpParameters");

		// If this is a new RtpReceiver, iterate all the peers but this one and
		// create a RtpSender associated to this RtpReceiver for each Peer.
		if (this->mapRtpReceiverRtpSenders.find(rtpReceiver) == this->mapRtpReceiverRtpSenders.end())
		{
			// Ensure the entry will exist even with an empty array.
			this->mapRtpReceiverRtpSenders[rtpReceiver];

			for (auto& kv : this->peers)
			{
				auto* senderPeer = kv.second;

				// Skip receiver Peer.
				if (senderPeer == peer)
					continue;

				// Skip Peer with capabilities not set yet.
				if (!senderPeer->HasCapabilities())
					continue;

				AddRtpSenderForRtpReceiver(senderPeer, rtpReceiver);
			}
		}
		// If this is not a new RtpReceiver let's retrieve its updated parameters
		// and update with them all the associated RtpSenders.
		else
		{
			for (auto rtpSender : this->mapRtpReceiverRtpSenders[rtpReceiver])
			{
				// Provide the RtpSender with the parameters of the RtpReceiver.
				rtpSender->Send(rtpReceiver->GetParameters());
			}
		}
	}

	void Room::OnPeerRtpReceiverClosed(const RTC::Peer* /*peer*/, const RTC::RtpReceiver* rtpReceiver)
	{
		MS_TRACE();

		// If the RtpReceiver is in the map, iterate the map and close all the
		// RtpSenders associated to the closed RtpReceiver.
		if (this->mapRtpReceiverRtpSenders.find(rtpReceiver) != this->mapRtpReceiverRtpSenders.end())
		{
			// Make a copy of the set of RtpSenders given that Destroy() will be called
			// in all of them, producing onPeerRtpSenderClosed() that will remove it
			// from the map.
			auto rtpSenders = this->mapRtpReceiverRtpSenders[rtpReceiver];

			// Safely iterate the copy of the set.
			for (auto& rtpSender : rtpSenders)
			{
				rtpSender->Destroy();
			}

			// Finally delete the RtpReceiver entry in the map.
			this->mapRtpReceiverRtpSenders.erase(rtpReceiver);
		}
        
        // clear data
        if (this->mapRtpReceiverLayerSelector.find(rtpReceiver) != this->mapRtpReceiverLayerSelector.end())
        {
            delete this->mapRtpReceiverLayerSelector.find(rtpReceiver)->second;
            mapRtpReceiverLayerSelector.erase(this->mapRtpReceiverLayerSelector.find(rtpReceiver));
        }
        
        if (this->mapRtpReceiverAudioLevels.find(rtpReceiver) != this->mapRtpReceiverAudioLevels.end())
            mapRtpReceiverAudioLevels.erase(this->mapRtpReceiverAudioLevels.find(rtpReceiver));
        
        if (this->mapRtpReceiverAudioLevelSelector.find(rtpReceiver) != this->mapRtpReceiverAudioLevelSelector.end())
        {
            delete this->mapRtpReceiverAudioLevelSelector.find(rtpReceiver)->second;
            mapRtpReceiverAudioLevelSelector.erase(this->mapRtpReceiverAudioLevelSelector.find(rtpReceiver));
        }
        
        if (this->mapRtpReceiverPeer.find(rtpReceiver) != this->mapRtpReceiverPeer.end())
            mapRtpReceiverPeer.erase(this->mapRtpReceiverPeer.find(rtpReceiver));
        
        for (std::map<int, const RTC::RtpReceiver*>::const_iterator it = voiceSpeakers.begin(); it != voiceSpeakers.end(); ++it)
            if(it->second == rtpReceiver)
            {
                voiceSpeakers.erase(it);
                break;
            }

        
        
	}

	void Room::OnPeerRtpSenderClosed(const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender)
	{
		MS_TRACE();

		// Iterate all the receiver/senders map and remove the closed RtpSender from all the
		// RtpReceiver entries.
		for (auto& kv : this->mapRtpReceiverRtpSenders)
		{
			auto& rtpSenders = kv.second;

			rtpSenders.erase(rtpSender);
		}

		// Also remove the entry from the sender/receiver map.
		this->mapRtpSenderRtpReceiver.erase(rtpSender);
	}

	void Room::OnPeerRtpPacket(const RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpReceiverRtpSenders.find(rtpReceiver) != this->mapRtpReceiverRtpSenders.end(),
		    "RtpReceiver not present in the map");
        
        // Update audio levels.
        if (this->audioLevelsEventEnabled)
        {
            uint8_t volume;
            bool voice;
            if (packet->ReadAudioLevel(&volume, &voice))
            {
                const int8_t dBov = volume * -1;
                this->mapRtpReceiverAudioLevels[rtpReceiver].currentTmpValues.push_back(dBov);
            }
        }
        
        // filter packet. Be careful here - filters may not work in the same time.
        bool needToSendPacket = true;
        if ((needToFilterAudioLevels || needToFilterLayers) && packet->GetPayloadType() == 101)
        {
            // filter by layers
            if (needToFilterLayers)
            {
                {
                    // debug info as error
                    VP9::VP9PayloadDescription desc;
                    if (desc.Parse(packet->GetPayload(), 1700))
                        std::cerr << " temporal " << (int)desc.temporalLayerId << " spatial " << (int)desc.spatialLayerId << std::endl;
                }
                // create filter if it is not created yet
                if (mapRtpReceiverLayerSelector.find(rtpReceiver) == mapRtpReceiverLayerSelector.end())
                {
                    mapRtpReceiverLayerSelector[rtpReceiver] = new VP9::VP9LayerSelector();
                    mapRtpReceiverLayerSelector[rtpReceiver]->SelectTemporalLayer(Settings::configuration.vp9MinTemporial);
                    mapRtpReceiverLayerSelector[rtpReceiver]->SelectSpatialLayer(Settings::configuration.vp9MinSpartial);
                }
                // drop packets if needed
                uint32_t extSegNum;
                bool mark;
                if(mapRtpReceiverLayerSelector[rtpReceiver]->Select(packet, extSegNum, mark))
                {
                    packet->SetSequenceNumber(extSegNum);
                    uint16_t cicles = extSegNum >> 16;
                    packet->SetExtendedSequenceNumber(((uint32_t)cicles) << 16 | packet->GetSequenceNumber());
                    packet->SetMarker(mark);
                }
                else
                {
                    std::cerr << "packet was dropped for " << peerByReceiver(rtpReceiver) << " because TS filtering"<< std::endl;
                    needToSendPacket = false;
                }
            }
            // filter by audio level
            if (needToSendPacket && needToFilterAudioLevels)
            {
                bool packetFromActiveSpeaker = voiceSpeakers.size() >= 1 ? false : true;
                for (auto& speaker : voiceSpeakers)
                    if(peerByReceiver(speaker.second) == peerByReceiver(rtpReceiver))
                    {
                        packetFromActiveSpeaker = true;
                        break;
                    }
                // create filter if it is not created yet
                if (mapRtpReceiverAudioLevelSelector.find(rtpReceiver) == mapRtpReceiverAudioLevelSelector.end())
                    mapRtpReceiverAudioLevelSelector[rtpReceiver] = new VP9::VP9AudioLevelSelector();
                // drop packets if needed
                uint32_t extSegNum;
                bool mark;
                if(mapRtpReceiverAudioLevelSelector[rtpReceiver]->Select(packet, packetFromActiveSpeaker, extSegNum, mark))
                {
                    packet->SetSequenceNumber(extSegNum);
                    uint16_t cicles = extSegNum >> 16;
                    packet->SetExtendedSequenceNumber(((uint32_t)cicles) << 16 | packet->GetSequenceNumber());
                    packet->SetMarker(mark);
                    std::cerr << "packet was keept for " << peerByReceiver(rtpReceiver) << " number " << packet->GetSequenceNumber() << std::endl;
                }
                else
                {
                    std::cerr << "packet was dropped for " << peerByReceiver(rtpReceiver) << " because it is not active speaker" << std::endl;
                    needToSendPacket = false;
                }
            }
        }
        
        // send packet if it was not filtered
        if (needToSendPacket)
        {
            // Send the RtpPacket to all the RtpSenders associated to the RtpReceiver from which it was received.
            for (auto& rtpSender : this->mapRtpReceiverRtpSenders[rtpReceiver])
                rtpSender->SendRtpPacket(packet);
        }
	}

	void Room::OnPeerRtcpReceiverReport(
	    const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender, RTC::RTCP::ReceiverReport* report)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		rtpSender->ReceiveRtcpReceiverReport(report);
	}

	void Room::OnPeerRtcpFeedback(
	    const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender, RTC::RTCP::FeedbackPsPacket* packet)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		auto& rtpReceiver = this->mapRtpSenderRtpReceiver[rtpSender];

		rtpReceiver->ReceiveRtcpFeedback(packet);
	}

	void Room::OnPeerRtcpFeedback(
	    const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender, RTC::RTCP::FeedbackRtpPacket* packet)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		auto& rtpReceiver = this->mapRtpSenderRtpReceiver[rtpSender];

		rtpReceiver->ReceiveRtcpFeedback(packet);
	}

	void Room::OnPeerRtcpSenderReport(
	    const RTC::Peer* /*peer*/, RTC::RtpReceiver* rtpReceiver, RTC::RTCP::SenderReport* report)
	{
		MS_TRACE();

		// RtpReceiver needs the sender report in order to generate it's receiver report.
		rtpReceiver->ReceiveRtcpSenderReport(report);

		MS_ASSERT(
		    this->mapRtpReceiverRtpSenders.find(rtpReceiver) != this->mapRtpReceiverRtpSenders.end(),
		    "RtpReceiver not present in the map");
	}

	void Room::OnFullFrameRequired(RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		auto& rtpReceiver = this->mapRtpSenderRtpReceiver[rtpSender];

		rtpReceiver->RequestFullFrame();
	}

	inline void Room::OnTimer(Timer* timer)
	{
		MS_TRACE();
		// Audio levels timer.
		if (timer == this->audioLevelsTimer)
		{
            //std::cerr << "=========== on audio level timer ==================" << std::endl;
            // calculate average value for each reciever
            voiceSpeakers.clear();
			for (auto& lv : this->mapRtpReceiverAudioLevels)
			{
                // calculate min, max and current value
				auto& dBovs = lv.second.currentTmpValues;
				int8_t avgdBov{ -127 };
                lv.second.minValue = 127;
                lv.second.maxValue = -127;
                //std::cerr << peerByReceiver(lv.first);
				if (!dBovs.empty())
				{
                    int16_t sumdBovs{ 0 };
					for (auto& dBov : dBovs)
                    {
						sumdBovs += dBov;
                        if (dBov < lv.second.minValue)
                            lv.second.minValue = dBov;
                        if (dBov > lv.second.maxValue)
                            lv.second.maxValue = dBov;
                        //std::cerr << " " << (int)dBov;
                    }
					avgdBov = static_cast<int8_t>(std::lround(sumdBovs / static_cast<int16_t>(dBovs.size())));
				}
				lv.second.value = avgdBov;
                // Clear map for future use
                lv.second.currentTmpValues.clear();
                const double diff = lv.second.maxValue - lv.second.minValue; //-127 + 254 * (lv.second.value - lv.second.minValue) / (lv.second.maxValue - lv.second.minValue);
                lv.second.normalizedValue = (int8_t)diff;
                //std::cerr << " value " << (int)lv.second.value << " diff " << (int)lv.second.normalizedValue << std::endl;
                if (lv.second.normalizedValue > ActiveSpeakerVoiceDiff && lv.second.value > -50)
                    voiceSpeakers[lv.second.normalizedValue] = lv.first;
            }
			// Emit event.
            static const Json::StaticString JsonStringClass{ "class" };
            static const Json::StaticString JsonStringEntries{ "entries" };
			Json::Value eventData(Json::objectValue);
			eventData[JsonStringClass] = "Room";
			eventData[JsonStringEntries] = Json::arrayValue;
            if(voiceSpeakers.size() >= 1)
            {
                Json::Value entry(Json::arrayValue);
                entry.append(Json::UInt{ voiceSpeakers.begin()->second->rtpReceiverId});
                entry.append(Json::Int{ this->mapRtpReceiverAudioLevels[voiceSpeakers.begin()->second].value });
                eventData[JsonStringEntries].append(entry);
            }
            /*for (auto& lv : this->mapRtpReceiverAudioLevels)
            {
                Json::Value entry(Json::arrayValue);
                entry.append(Json::UInt{ lv.first->rtpReceiverId });
                entry.append(Json::Int{ lv.second.normalizedValue });
                eventData[JsonStringEntries].append(entry);
            }*/
			this->notifier->Emit(this->roomId, "audiolevels", eventData);
		}
	}
} // namespace RTC
