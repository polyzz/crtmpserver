/*
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAS_PROTOCOL_RTMP
#include "protocols/rtmp/streaming/innetrtmpstream.h"
#include "streaming/baseoutstream.h"
#include "protocols/rtmp/basertmpprotocol.h"
#include "protocols/rtmp/messagefactories/streammessagefactory.h"
#include "protocols/rtmp/streaming/baseoutnetrtmpstream.h"
#include "protocols/rtmp/streaming/outfilertmpflvstream.h"
#include "streaming/streamstypes.h"
#include "stdlib.h"

#define META_SERVER_FILE_NAME "fileName"
#define META_SERVER_FILE_NAME_LEN 8
#define META_SERVER_FULL_PATH "fullPath"
#define META_SERVER_FULL_PATH_LEN 8
#define META_SERVER_MEDIA_DIR "mediaDir"
#define META_SERVER_MEDIA_DIR_LEN 8

InNetRTMPStream::InNetRTMPStream(BaseProtocol *pProtocol, string name,
		uint32_t rtmpStreamId, uint32_t chunkSize, uint32_t channelId)
: BaseInNetStream(pProtocol, ST_IN_NET_RTMP, name) {
	_rtmpStreamId = rtmpStreamId;
	_chunkSize = chunkSize;
	_channelId = channelId;
	_clientId = format("%d_%d_%"PRIz"u", _pProtocol->GetId(), _rtmpStreamId, (size_t)this);
	_videoCts = 0;
	_pOutFileRTMPFLVStream = NULL;

	_audioPacketsCount = 0;
	_audioDroppedPacketsCount = 0;
	_audioBytesCount = 0;
	_audioDroppedBytesCount = 0;
	_videoPacketsCount = 0;
	_videoDroppedPacketsCount = 0;
	_videoBytesCount = 0;
	_videoDroppedBytesCount = 0;

	_audioCapabilitiesInitialized = false;
	_lastAudioCodec = 0;
	_videoCapabilitiesInitialized = false;
	_lastVideoCodec = 0;
}

InNetRTMPStream::~InNetRTMPStream() {
	if (_pOutFileRTMPFLVStream != NULL) {
		delete _pOutFileRTMPFLVStream;
		_pOutFileRTMPFLVStream = NULL;
	}
}

StreamCapabilities * InNetRTMPStream::GetCapabilities() {
	return &_streamCapabilities;
}

void InNetRTMPStream::ReadyForSend() {
	ASSERT("Operation not supported");
}

bool InNetRTMPStream::IsCompatibleWithType(uint64_t type) {
	return TAG_KIND_OF(type, ST_OUT_NET_RTMP_4_RTMP)
			|| TAG_KIND_OF(type, ST_OUT_FILE_RTMP)
			|| TAG_KIND_OF(type, ST_OUT_NET_RTP)
			|| TAG_KIND_OF(type, ST_OUT_FILE_RTMP_FLV);

}

void InNetRTMPStream::GetStats(Variant &info, uint32_t namespaceId) {
	BaseInNetStream::GetStats(info, namespaceId);
	info["audio"]["packetsCount"] = _audioPacketsCount;
	info["audio"]["droppedPacketsCount"] = (uint64_t) 0;
	info["audio"]["bytesCount"] = _audioBytesCount;
	info["audio"]["droppedBytesCount"] = (uint64_t) 0;
	info["video"]["packetsCount"] = _videoPacketsCount;
	info["video"]["droppedPacketsCount"] = (uint64_t) 0;
	info["video"]["bytesCount"] = _videoBytesCount;
	info["video"]["droppedBytesCount"] = (uint64_t) 0;
}

uint32_t InNetRTMPStream::GetRTMPStreamId() {
	return _rtmpStreamId;
}

uint32_t InNetRTMPStream::GetChunkSize() {
	return _chunkSize;
}

void InNetRTMPStream::SetChunkSize(uint32_t chunkSize) {
	_chunkSize = chunkSize;
	LinkedListNode<BaseOutStream *> *pTemp = _pOutStreams;
	while (pTemp != NULL) {
		if (!pTemp->info->IsEnqueueForDelete()) {
			if (TAG_KIND_OF(pTemp->info->GetType(), ST_OUT_NET_RTMP)) {
				((BaseRTMPProtocol *) pTemp->info->GetProtocol())->TrySetOutboundChunkSize(chunkSize);
			}
		}
		pTemp = pTemp->pPrev;
	}
}

bool InNetRTMPStream::SendStreamMessage(Variant &completeMessage, bool persistent) {
	//2. Loop on the subscribed streams and send the message
	LinkedListNode<BaseOutStream *> *pTemp = _pOutStreams;
	while ((pTemp != NULL) && (!IsEnqueueForDelete())) {
		if (pTemp->info->IsEnqueueForDelete()) {
			FINEST("IsEnqueueForDelete is true. Move ahead....");
			pTemp = pTemp->pPrev;
			continue;
		}
		if (TAG_KIND_OF(pTemp->info->GetType(), ST_OUT_NET_RTMP)) {
			if (!((BaseOutNetRTMPStream *) pTemp->info)->SendStreamMessage(completeMessage)) {
				FATAL("Unable to send notify on stream. The connection will go down");
				pTemp->info->EnqueueForDelete();
			}
		}
		pTemp = pTemp->pPrev;
	}

	//3. Test to see if we are still alive. One of the target streams might
	//be on the same RTMP connection as this stream is and our connection
	//here might be enque for delete
	if (IsEnqueueForDelete())
		return false;

	//4. Save the message for future use if necessary
	if (persistent)
		_lastStreamMessage = completeMessage;

	if ((uint32_t) VH_MT(completeMessage) == RM_HEADER_MESSAGETYPE_NOTIFY) {
		Variant &params = M_NOTIFY_PARAMS(completeMessage);
		if ((params == V_MAP) && (params.MapSize() >= 2)) {
			Variant &notify = MAP_VAL(params.begin());
			if ((notify == V_STRING) && (lowerCase((string) notify) == "onmetadata")) {
				//ASSERT("\n%s", STR(completeMessage.ToString()));
				Variant &metadata = MAP_VAL(++params.begin());
				if (metadata == V_MAP) {
					if (metadata.HasKeyChain(_V_NUMERIC, false, 1, "bandwidth")) {
						_streamCapabilities.SetTransferRate((double) metadata["bandwidth"]*1024.0);
					} else {
						double transferRate = -1;
						if (metadata.HasKeyChain(_V_NUMERIC, false, 1, "audiodatarate")) {
							transferRate += (double) metadata["audiodatarate"]*1024.0;
						}
						if (metadata.HasKeyChain(_V_NUMERIC, false, 1, "videodatarate")) {
							transferRate += (double) metadata["videodatarate"]*1024;
						}
						if (transferRate >= 0) {
							transferRate += 1;
							_streamCapabilities.SetTransferRate(transferRate);
						}
					}
				}
			}
		}
	}
	//5. Done
	return true;
}

bool InNetRTMPStream::SendStreamMessage(string functionName, Variant &parameters,
		bool persistent) {

	//1. Prepare the message
	Variant message = StreamMessageFactory::GetFlexStreamSend(0, 0, 0, true,
			functionName, parameters);

	return SendStreamMessage(message, persistent);
}

bool InNetRTMPStream::SendOnStatusStreamPublished() {
	Variant response = StreamMessageFactory::GetInvokeOnStatusStreamPublished(
			_channelId,
			_rtmpStreamId,
			0, false,
			0,
			RM_INVOKE_PARAMS_ONSTATUS_LEVEL_STATUS,
			RM_INVOKE_PARAMS_ONSTATUS_CODE_NETSTREAMPUBLISHSTART,
			format("Stream `%s` is now published", STR(GetName())),
			GetName(),
			_clientId);

	if (!GetRTMPProtocol()->SendMessage(response)) {
		FATAL("Unable to send message");
		return false;
	}
	return true;
}

bool InNetRTMPStream::RecordFLV(Variant &meta, bool append) {

	//1. Compute the file name
		string fileName;
        string lastFileName;
        int i = 0;
		fileName = "/data/guoshou_app/web/public/video/" + GetName()+".flv";
		FINEST("fileName: %s", STR(fileName));
	//2. Delete the old file
		if (append) {
			WARN("append not supported yet. File will be overwritten");
		}
                
                if(fileExists(fileName)) {
                  do {
                    lastFileName = format("/data/guoshou_app/web/public/video/%s_%d.flv",STR(GetName()), i);
                    i++;
                  } while(fileExists(lastFileName));

                  moveFile(fileName, lastFileName);
                }


		//deleteFile(fileName);

	//3. Create the out file
		_pOutFileRTMPFLVStream = new OutFileRTMPFLVStream(_pProtocol,
				GetName(), fileName);
		if (!_pOutFileRTMPFLVStream->SetStreamsManager(_pStreamsManager)) {
			FATAL("Unable to set the streams manager");
			delete _pOutFileRTMPFLVStream;
			_pOutFileRTMPFLVStream = NULL;
			return false;
		}

	//4. Link it
		return _pOutFileRTMPFLVStream->Link(this);



}

bool InNetRTMPStream::RecordMP4(Variant &meta) {
	NYIR;
}

void InNetRTMPStream::SignalOutStreamAttached(BaseOutStream *pOutStream) {
	if (_lastStreamMessage != V_NULL) {
		if (TAG_KIND_OF(pOutStream->GetType(), ST_OUT_NET_RTMP)) {
			if (!((BaseOutNetRTMPStream *) pOutStream)->SendStreamMessage(
					_lastStreamMessage)) {
				FATAL("Unable to send notify on stream. The connection will go down");
				pOutStream->EnqueueForDelete();
			}
		}
	}
}

void InNetRTMPStream::SignalOutStreamDetached(BaseOutStream *pOutStream) {
	//FINEST("outbound stream %u detached from inbound stream %u",
	//		pOutStream->GetUniqueId(), GetUniqueId());
}

bool InNetRTMPStream::SignalPlay(double &dts, double &length) {
	return true;
}

bool InNetRTMPStream::SignalPause() {
	return true;
}

bool InNetRTMPStream::SignalResume() {
	return true;
}

bool InNetRTMPStream::SignalSeek(double &dts) {
	return true;
}

bool InNetRTMPStream::SignalStop() {
	return true;
}

//#define RTMP_DUMP_PTSDTS
#ifdef RTMP_DUMP_PTSDTS
double __lastRtmpPts = 0;
double __lastRtmpDts = 0;
#endif /* RTMP_DUMP_PTSDTS */

bool InNetRTMPStream::FeedDataAggregate(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	if (processedLength != GETAVAILABLEBYTESCOUNT(_aggregate)) {
		_aggregate.IgnoreAll();
		return true;
	}

	if (dataLength + processedLength > totalLength) {
		_aggregate.IgnoreAll();
		return true;
	}

	_aggregate.ReadFromBuffer(pData, dataLength);

	uint8_t *pBuffer = GETIBPOINTER(_aggregate);
	uint32_t length = GETAVAILABLEBYTESCOUNT(_aggregate);

	if ((length != totalLength) || (length == 0)) {
		return true;
	}

	uint32_t frameLength = 0;
	uint32_t timestamp = 0;
	while (length >= 15) {
		frameLength = (ENTOHLP(pBuffer)) &0x00ffffff;
		timestamp = ENTOHAP(pBuffer + 4);
		if (length < frameLength + 15) {
			_aggregate.IgnoreAll();
			return true;
		}
		if ((pBuffer[0] != 8) && (pBuffer[0] != 9)) {
			length -= (frameLength + 15);
			pBuffer += (frameLength + 15);
			continue;
		}

		if (!FeedData(pBuffer + 11, frameLength, 0, frameLength, timestamp,
				timestamp, (pBuffer[0] == 8))) {
			FATAL("Unable to feed data");
			return false;
		}
		length -= (frameLength + 15);
		pBuffer += (frameLength + 15);
	}
	_aggregate.IgnoreAll();
	return true;
}

bool InNetRTMPStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	if (isAudio) {
		_audioPacketsCount++;
		_audioBytesCount += dataLength;
		if (
				(processedLength == 0)
				&&(dataLength >= 2)
				&&(
				(!_audioCapabilitiesInitialized)
				|| (_lastAudioCodec != (pData[0] >> 4))
				|| (((pData[0] >> 4) == 0x0a)&&(pData[1] == 0)))
				) {
			if (!InitializeAudioCapabilities(this, _streamCapabilities,
					_audioCapabilitiesInitialized, pData, dataLength)) {
				FATAL("Unable to initialize audio capabilities");
				return false;
			}
			_lastAudioCodec = (pData[0] >> 4);
		}
	} else {
		_videoPacketsCount++;
		_videoBytesCount += dataLength;
		if (
				(processedLength == 0)
				&&(dataLength >= 2)
				&&(
				(!_videoCapabilitiesInitialized)
				|| (_lastVideoCodec != (pData[0]&0x0f))
				|| (((pData[0]&0x0f) == 0x07)&&(pData[1] == 0)))
				) {
			if (!InitializeVideoCapabilities(this, _streamCapabilities,
					_videoCapabilitiesInitialized, pData, dataLength)) {
				FATAL("Unable to initialize video capabilities");
				return false;
			}
			_lastVideoCodec = (pData[0]&0x0f);
		}

		if ((processedLength == 0) && ((pData[0]&0x0f) == 7) && (dataLength >= 6)) {
			_videoCts = (ENTOHLP(pData + 2) >> 8);
		}
		pts = dts + _videoCts;
	}

#ifdef RTMP_DUMP_PTSDTS
	if (!isAudio) {
		FINEST("pts: %8.2f\tdts: %8.2f\tcts: %8.2f\tptsd: %+6.2f\tdtsd: %+6.2f\t%s",
				pts,
				dts,
				pts - dts,
				pts - __lastRtmpPts,
				dts - __lastRtmpDts,
				pts == dts ? "" : "DTS Present");
		if (dataLength + processedLength == totalLength) {
			__lastRtmpPts = pts;
			__lastRtmpDts = dts;
			FINEST("--------------");
		}
	}
#endif /* RTSP_DUMP_PTSDTS */

	LinkedListNode<BaseOutStream *> *pTemp = _pOutStreams;
	while (pTemp != NULL) {
		if (!pTemp->info->IsEnqueueForDelete()) {
			if (!pTemp->info->FeedData(pData, dataLength, processedLength, totalLength,
					pts, dts, isAudio)) {
				FINEST("Unable to feed OS: %p", pTemp->info);
				pTemp->info->EnqueueForDelete();
				if (GetProtocol() == pTemp->info->GetProtocol()) {
					return false;
				}
			}
		}
		pTemp = pTemp->pPrev;
	}
	return true;
}

uint32_t InNetRTMPStream::GetInputVideoTimescale() {
	return 1000;
}

uint32_t InNetRTMPStream::GetInputAudioTimescale() {
	return 1000;
}

BaseRTMPProtocol *InNetRTMPStream::GetRTMPProtocol() {
	return (BaseRTMPProtocol *) _pProtocol;
}

bool InNetRTMPStream::InitializeAudioCapabilities(
		BaseInStream *pStream, StreamCapabilities &streamCapabilities,
		bool &capabilitiesInitialized, uint8_t *pData, uint32_t length) {
	if (length == 0) {
		capabilitiesInitialized = false;
		return true;
	}
	CodecInfo *pCodecInfo = NULL;
	switch (pData[0] >> 4) {
		case 0: //Linear PCM, platform endian
		case 1: //ADPCM
		case 3: //Linear PCM, little endian
		case 7: //G.711 A-law logarithmic PCM
		case 8: //G.711 mu-law logarithmic PCM
		case 11: //Speex
		case 14: //MP3 8-Khz
		case 15: //Device-specific sound
		{
			WARN("RTMP input audio codec %"PRIu8" defaulted to pass through", (uint8_t) (pData[0] >> 4));
			pCodecInfo = streamCapabilities.AddTrackAudioPassThrough(pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse pass-through codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 2: //MP3
		{
			uint32_t ssRate = 0;
			switch ((pData[0] >> 2)&0x03) {
				case 0:
					ssRate = 5512;
					break;
				case 1:
					ssRate = 11025;
					break;
				case 2:
					ssRate = 22050;
					break;
				case 3:
					ssRate = 44100;
					break;
			}
			pCodecInfo = streamCapabilities.AddTrackAudioMP3(
					ssRate,
					(pData[0] & 0x01) == 0 ? 1 : 2,
					((pData[0] >> 1)&0x01) == 0 ? 8 : 16,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse MP3 codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 4: //Nellymoser 16-kHz mono
		{
			pCodecInfo = streamCapabilities.AddTrackAudioNellymoser(
					16000,
					1,
					((pData[0] >> 1)&0x01) == 0 ? 8 : 16,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse Nellymoser 16-kHz mono codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 5: //Nellymoser 8-kHz mono
		{
			pCodecInfo = streamCapabilities.AddTrackAudioNellymoser(
					8000,
					1,
					((pData[0] >> 1)&0x01) == 0 ? 8 : 16,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse Nellymoser 8-kHz mono codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 6: //Nellymoser
		{
			uint32_t ssRate = 0;
			switch ((pData[0] >> 2)&0x03) {
				case 0:
					ssRate = 5512;
					break;
				case 1:
					ssRate = 11025;
					break;
				case 2:
					ssRate = 22050;
					break;
				case 3:
					ssRate = 44100;
					break;
			}
			pCodecInfo = streamCapabilities.AddTrackAudioNellymoser(
					ssRate,
					(pData[0] & 0x01) == 0 ? 1 : 2,
					((pData[0] >> 1)&0x01) == 0 ? 8 : 16,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse Nellymoser codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 10: //AAC
		{
			if (length < 4) {
				FATAL("Invalid length");
				return false;
			}
			pCodecInfo = streamCapabilities.AddTrackAudioAAC(
					pData + 2, (uint8_t) (length - 2), true,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse AAC codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 9: //reserved
		default:
		{
			FATAL("Invalid audio codec ID %"PRIu32" detected on an input RTMP stream",
					pData[0] >> 4);
			return false;
		}
	}

	capabilitiesInitialized = true;

	return true;
}

bool InNetRTMPStream::InitializeVideoCapabilities(
		BaseInStream *pStream, StreamCapabilities &streamCapabilities,
		bool &capabilitiesInitialized, uint8_t *pData, uint32_t length) {
	if ((length == 0) || ((pData[0] >> 4) == 5)) {
		capabilitiesInitialized = false;
		return true;
	}
	CodecInfo *pCodecInfo = NULL;
	switch (pData[0]&0x0f) {
		case 1: //JPEG (currently unused)
		case 3: //Screen video
		case 5: //On2 VP6 with alpha channel
		case 6: //Screen video version 2
		{
			WARN("RTMP input video codec %"PRIu8" defaulted to pass through", (uint8_t) (pData[0]&0x0f));
			pCodecInfo = streamCapabilities.AddTrackVideoPassThrough(pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse pass-through codec setup bytes for input RTMP stream");
				return false;
			}
			break;
		}
		case 2: //Sorenson H.263
		{
			//17 - marker
			//5 - format1
			//8 - picture number/timestamp
			//3 - format2
			//2*8 or 2*16 - width/height depending on the format2
			if (length < 11) {
				FATAL("Not enough data to initialize Sorenson H.263 for an input RTMP stream. Wanted: %"PRIu32"; Got: %"PRIu32,
						(uint32_t) (11), length);
				return false;
			}
			pCodecInfo = streamCapabilities.AddTrackVideoSorensonH263(pData + 1,
					10,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse Sorenson H.263 headers for input RTMP stream");
				return false;
			}
			break;
		}
		case 4: //On2 VP6
		{
			if (length < 7) {
				FATAL("Not enough data to initialize On2 VP6 codec for an input RTMP stream. Wanted: %"PRIu32"; Got: %"PRIu32,
						(uint32_t) (7), length);
				return false;
			}
			pCodecInfo = streamCapabilities.AddTrackVideoVP6(pData + 1, 6,
					pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse On2 VP6 codec for input RTMP stream");
				return false;
			}
			break;
		}
		case 7: //AVC
		{
			/*
			 * map:
			 * iiiiiiiiiii ss <sps> i pp <pps>
			 *
			 * i - ignored
			 * ss - sps length
			 * <sps> - sps data
			 * pp - pps length
			 * <pps> - pps data
			 */
			if (length < 11 + 2) {
				FATAL("Not enough data to initialize AVC codec for an input RTMP stream. Wanted: %"PRIu32"; Got: %"PRIu32,
						(uint32_t) (11 + 2), length);
				return false;
			}
			if (((pData[0] >> 4) != 1) || (pData[1] != 0)) {
				FINEST("this is not a key frame or not a codec setup. Ignore it: %02"PRIu8"%02"PRIu8,
						pData[0], pData[1]);
				return true;
			}
			uint32_t spsLength = ENTOHSP(pData + 11);
			if (length < 11 + 2 + spsLength + 1 + 2) {
				FATAL("Not enough data to initialize AVC codec for an input RTMP stream. Wanted: %"PRIu32"; Got: %"PRIu32,
						11 + 2 + spsLength + 1 + 2, length);
				return false;
			}
			uint32_t ppsLength = ENTOHSP(pData + 11 + 2 + spsLength + 1);
			if (length != 11 + 2 + spsLength + 1 + 2 + ppsLength) {
				FATAL("Invalid AVC codec packet length for an input RTMP stream. Wanted: %"PRIu32"; Got: %"PRIu32,
						11 + 2 + spsLength + 1 + 2 + ppsLength, length);
				return false;
			}
			uint8_t *pSPS = pData + 11 + 2;
			uint8_t *pPPS = pData + 11 + 2 + spsLength + 1 + 2;
			pCodecInfo = streamCapabilities.AddTrackVideoH264(
					pSPS, spsLength, pPPS, ppsLength, 90000, pStream);
			if (pCodecInfo == NULL) {
				FATAL("Unable to parse SPS/PPS for input RTMP stream");
				return false;
			}
			break;
		}
		default:
		{
			FATAL("Invalid audio codec ID %"PRIu32" detected on an input RTMP stream:",
					pData[0]&0x0f);
			return false;
		}
	}

	capabilitiesInitialized = true;

	return true;
}
#endif /* HAS_PROTOCOL_RTMP */

