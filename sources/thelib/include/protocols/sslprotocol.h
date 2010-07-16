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


#ifndef _SSLPROTOCOL_H
#define	_SSLPROTOCOL_H

#include "protocols/baseprotocol.h"
#include "buffering/iobuffer.h"

class SSLProtocol
: public BaseProtocol {
private:
    static map<string, SSL_CTX *> _pGlobalContexts;
    static bool _libraryInitialized;
    SSL_CTX *_pGlobalSSLContext1;
    SSL *_pSSL;
    IOBuffer _inputBuffer;
    IOBuffer _outputBuffer;
    uint8_t *_pReadBuffer;
public:
    SSLProtocol();
    virtual ~SSLProtocol();
    virtual bool Initialize(Variant &parameters);
    virtual bool AllowFarProtocol(uint64_t type);
    virtual bool AllowNearProtocol(uint64_t type);
    virtual bool EnqueueForOutbound();
    virtual IOBuffer * GetOutputBuffer();
    virtual IOBuffer * GetInputBuffer();
    virtual bool SignalInputData(int32_t recvAmount);
    virtual bool SignalInputData(IOBuffer &buffer);
private:
    string DumpBIO(BIO *pBIO);
    string GetSSLErrors();
    void InitRandGenerator();
    bool PerformIO();
};


#endif	/* _SSLPROTOCOL_H */


