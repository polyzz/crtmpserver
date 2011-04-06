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

#ifndef _LUAAPI_HANDLER_RTMP_H
#define	_LUAAPI_HANDLER_RTMP_H

#include "common.h"

#define LUA_HANDLER_RTMP_GET_PROTOCOL(pProtocol,protocolId,L) \
BaseProtocol *pProtocol = ProtocolManager::GetProtocol(protocolId); \
if (pProtocol == NULL) { \
	FATAL("Protocol %d not available",protocolId); \
	lua_pushboolean(L, false); \
	return 1; \
} \
if ((pProtocol->GetType() != PT_INBOUND_RTMP) \
		&& (pProtocol->GetType() != PT_OUTBOUND_RTMP)) { \
	FATAL("Protocol %d has invalid type",protocolId); \
	lua_pushboolean(L, false); \
	return 1; \
} \
do{}while(0)

#define LUA_HANDLER_RTMP_GET_PROTOCOL_HANDLER_BY_PROTOCOL(pHandler,pApplication,pProtocol,L) \
BaseRTMPAppProtocolHandler *pHandler = \
	pApplication->GetProtocolHandler<BaseRTMPAppProtocolHandler > (pProtocol); \
if (pHandler == NULL) {\
	lua_pushboolean(L, false); \
	return 1; \
} \
do{}while(0)

#define LUA_HANDLER_RTMP_GET_PROTOCOL_HANDLER_BY_SCHEME(pHandler,pApplication,scheme,L) \
BaseRTMPAppProtocolHandler *pHandler = NULL; \
do{ \
	string ____tempString=scheme; \
	pHandler=pApplication->GetProtocolHandler<BaseRTMPAppProtocolHandler > (____tempString); \
	if (pHandler == NULL) {\
		lua_pushboolean(L, false); \
		return 1; \
	} \
} while(0)

namespace app_vmapp {
	int luaapi_handler_rtmp_pullExternalStream(lua_State *L);
	int luaapi_handler_rtmp_pushLocalStream(lua_State *L);
	int luaapi_handler_rtmp_outboundConnectionEstablished(lua_State *L);
	int luaapi_handler_rtmp_authenticateInbound(lua_State *L);
	int luaapi_handler_rtmp_inboundMessageAvailable(lua_State *L);
	int luaapi_handler_rtmp_processWinAckSize(lua_State *L);
	int luaapi_handler_rtmp_processPeerBW(lua_State *L);
	int luaapi_handler_rtmp_processAck(lua_State *L);
	int luaapi_handler_rtmp_processChunkSize(lua_State *L);
	int luaapi_handler_rtmp_processUsrCtrl(lua_State *L);
	int luaapi_handler_rtmp_processNotify(lua_State *L);
	int luaapi_handler_rtmp_processFlexStreamSend(lua_State *L);
	int luaapi_handler_rtmp_processSharedObject(lua_State *L);
	int luaapi_handler_rtmp_processInvoke(lua_State *L);
	int luaapi_handler_rtmp_processInvokeConnect(lua_State *L);
	int luaapi_handler_rtmp_processInvokeCreateStream(lua_State *L);
	int luaapi_handler_rtmp_processInvokePublish(lua_State *L);
	int luaapi_handler_rtmp_processInvokeSeek(lua_State *L);
	int luaapi_handler_rtmp_processInvokePlay(lua_State *L);
	int luaapi_handler_rtmp_processInvokePauseRaw(lua_State *L);
	int luaapi_handler_rtmp_processInvokePause(lua_State *L);
	int luaapi_handler_rtmp_processInvokeCloseStream(lua_State *L);
	int luaapi_handler_rtmp_processInvokeReleaseStream(lua_State *L);
	int luaapi_handler_rtmp_processInvokeDeleteStream(lua_State *L);
	int luaapi_handler_rtmp_processInvokeOnStatus(lua_State *L);
	int luaapi_handler_rtmp_processInvokeFCPublish(lua_State *L);
	int luaapi_handler_rtmp_processInvokeGetStreamLength(lua_State *L);
	int luaapi_handler_rtmp_processInvokeOnBWDone(lua_State *L);
	int luaapi_handler_rtmp_processInvokeGeneric(lua_State *L);
	int luaapi_handler_rtmp_processInvokeResultWithoutRequest(lua_State *L);
	int luaapi_handler_rtmp_processInvokeResultWithRequest(lua_State *L);
	int luaapi_handler_rtmp_processInvokeConnectResult(lua_State *L);
	int luaapi_handler_rtmp_processInvokeCreateStreamResult(lua_State *L);
	int luaapi_handler_rtmp_processInvokeFCSubscribeResult(lua_State *L);
	int luaapi_handler_rtmp_generateMetaFiles(lua_State *L);
	int luaapi_handler_rtmp_getMetaData(lua_State *L);
	int luaapi_handler_rtmp_sendRTMPMessage(lua_State *L);
}

#endif	/* _LUAAPI_HANDLER_RTMP_H */
