/******************************************************************************\
 * Copyright (c) 2004-2016
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later 
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined ( CHANNEL_HOIH9345KJH98_3_4344_BB23945IUHF1912__INCLUDED_ )
#define CHANNEL_HOIH9345KJH98_3_4344_BB23945IUHF1912__INCLUDED_

#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include "global.h"
#include "buffer.h"
#include "util.h"
#include "protocol.h"
#include "socket.h"


/* Definitions ****************************************************************/
// Set the time-out for the input buffer until the state changes from
// connected to not connected (the actual time depends on the way the error
// correction is implemented)
#define CON_TIME_OUT_SEC_MAX                30 // seconds

enum EPutDataStat
{
    PS_GEN_ERROR,
    PS_AUDIO_OK,
    PS_AUDIO_ERR,
    PS_AUDIO_INVALID,
    PS_PROT_OK,
    PS_PROT_OK_MESS_NOT_EVALUATED,
    PS_PROT_ERR,
    PS_NEW_CONNECTION
};


/* Classes ********************************************************************/
class CChannel : public QObject
{
    Q_OBJECT

public:
    // we have to make "server" the default since I do not see a chance to
    // use constructor initialization in the server for a vector of channels
    CChannel ( const bool bNIsServer = true );

    void PutProtcolData ( const int               iRecCounter,
                          const int               iRecID,
                          const CVector<uint8_t>& vecbyMesBodyData,
                          const CHostAddress&     RecHostAddr );

    EPutDataStat PutAudioData ( const CVector<uint8_t>& vecbyData,
                                const int               iNumBytes,
                                CHostAddress            RecHostAddr );

    EGetDataStat GetData ( CVector<uint8_t>& vecbyData,
                           const int         iNumBytes );

    void PrepAndSendPacket ( CHighPrioSocket*        pSocket,
                             const CVector<uint8_t>& vecbyNPacket,
                             const int               iNPacketLen );

    void ResetTimeOutCounter() { iConTimeOut = iConTimeOutStartVal; }
    bool IsConnected() const { return iConTimeOut > 0; }
    void Disconnect();

    void SetEnable ( const bool bNEnStat );
    bool IsEnabled() { return bIsEnabled; }

    void SetAddress ( const CHostAddress NAddr ) { InetAddr = NAddr; }
    bool GetAddress ( CHostAddress& RetAddr );
    const CHostAddress& GetAddress() const { return InetAddr; }

    void ResetInfo() { ChannelInfo = CChannelCoreInfo(); } // reset does not emit a message
    void SetName ( const QString sNNa );
    QString GetName();
    void SetChanInfo ( const CChannelCoreInfo& NChanInf );
    CChannelCoreInfo& GetChanInfo() { return ChannelInfo; }

    void SetRemoteInfo ( const CChannelCoreInfo ChInfo )
    {
        // because of compatibility to old versions, we also have to send the
        // channel name message extra
// #### COMPATIBILITY OLD VERSION, TO BE REMOVED ####
Protocol.CreateChanNameMes ( ChInfo.strName );
        Protocol.CreateChanInfoMes ( ChInfo );
    }
    void CreateReqChanInfoMes() { Protocol.CreateReqChanInfoMes(); }

    void SetGain ( const int iChanID, const double dNewGain );
    double GetGain ( const int iChanID );

    void SetRemoteChanGain ( const int iId, const double dGain )
        { Protocol.CreateChanGainMes ( iId, dGain ); }

    bool SetSockBufNumFrames ( const int  iNewNumFrames,
                               const bool bPreserve = false );
    int GetSockBufNumFrames() const { return iCurSockBufNumFrames; }

    void UpdateSocketBufferSize();

    int GetUploadRateKbps();

    // set/get network out buffer size and size factor
    void SetAudioStreamProperties ( const EAudComprType eNewAudComprType,
                                    const int iNewNetwFrameSize,
                                    const int iNewNetwFrameSizeFact,
                                    const int iNewNumAudioChannels );

    void SetDoAutoSockBufSize ( const bool bValue )
        { bDoAutoSockBufSize = bValue; }

    bool GetDoAutoSockBufSize() const { return bDoAutoSockBufSize; }

    int GetNetwFrameSizeFact() const { return iNetwFrameSizeFact; }
    int GetNetwFrameSize() const { return iNetwFrameSize; }

    void GetBufErrorRates ( CVector<double>& vecErrRates, double& dLimit, double& dMaxUpLimit )
        { SockBuf.GetErrorRates ( vecErrRates, dLimit, dMaxUpLimit ); }

    EAudComprType GetAudioCompressionType() { return eAudioCompressionType; }
    int GetNumAudioChannels() const { return iNumAudioChannels; }

    // network protocol interface
    void CreateJitBufMes ( const int iJitBufSize )
    { 
        if ( ProtocolIsEnabled() )
        {
            Protocol.CreateJitBufMes ( iJitBufSize );
        }
    }
    void CreateReqNetwTranspPropsMes()                       { Protocol.CreateReqNetwTranspPropsMes(); }
    void CreateReqJitBufMes()                                { Protocol.CreateReqJitBufMes(); }
    void CreateReqConnClientsList()                          { Protocol.CreateReqConnClientsList(); }
    void CreateChatTextMes ( const QString& strChatText )    { Protocol.CreateChatTextMes ( strChatText ); }
    void CreateLicReqMes ( const ELicenceType eLicenceType ) { Protocol.CreateLicenceRequiredMes ( eLicenceType ); }

// #### COMPATIBILITY OLD VERSION, TO BE REMOVED ####
void CreateConClientListNameMes ( const CVector<CChannelInfo>& vecChanInfo )
    { Protocol.CreateConClientListNameMes ( vecChanInfo ); }

    void CreateConClientListMes ( const CVector<CChannelInfo>& vecChanInfo )
        { Protocol.CreateConClientListMes ( vecChanInfo ); }

    CNetworkTransportProps GetNetworkTransportPropsFromCurrentSettings();

protected:
    bool ProtocolIsEnabled();

    void ResetNetworkTransportProperties()
    {
        // set it to a state were no decoding is ever possible (since we want
        // only to decode data when a network transport property message is
        // received with the correct values)
        eAudioCompressionType = CT_NONE;
        iNetwFrameSizeFact    = FRAME_SIZE_FACTOR_PREFERRED;
        iNetwFrameSize        = CELT_MINIMUM_NUM_BYTES;
        iNumAudioChannels     = 1; // mono
    }

    // connection parameters
    CHostAddress      InetAddr;

    // channel info
    CChannelCoreInfo  ChannelInfo;

    // mixer and effect settings
    CVector<double>   vecdGains;

    // network jitter-buffer
    CNetBufWithStats  SockBuf;
    int               iCurSockBufNumFrames;
    bool              bDoAutoSockBufSize;

    // network output conversion buffer
    CConvBuf<uint8_t> ConvBuf;

    // network protocol
    CProtocol         Protocol;

    int               iConTimeOut;
    int               iConTimeOutStartVal;

    bool              bIsEnabled;
    bool              bIsServer;

    int               iNetwFrameSizeFact;
    int               iNetwFrameSize;

    EAudComprType     eAudioCompressionType;
    int               iNumAudioChannels;

    QMutex            Mutex;
    QMutex            MutexSocketBuf;
    QMutex            MutexConvBuf;

public slots:
    void OnSendProtMessage ( CVector<uint8_t> vecMessage );
    void OnJittBufSizeChange ( int iNewJitBufSize );
    void OnChangeChanGain ( int iChanID, double dNewGain );
    void OnChangeChanName ( QString strName );
    void OnChangeChanInfo ( CChannelCoreInfo ChanInfo );
    void OnNetTranspPropsReceived ( CNetworkTransportProps NetworkTransportProps );
    void OnReqNetTranspProps();

    void OnParseMessageBody ( CVector<uint8_t> vecbyMesBodyData,
                              int              iRecCounter,
                              int              iRecID )
    {
        // note that the return value is ignored here
        Protocol.ParseMessageBody ( vecbyMesBodyData, iRecCounter, iRecID );
    }

    void OnProtcolMessageReceived ( int              iRecCounter,
                                    int              iRecID,
                                    CVector<uint8_t> vecbyMesBodyData,
                                    CHostAddress     RecHostAddr )
    {
        PutProtcolData ( iRecCounter, iRecID, vecbyMesBodyData, RecHostAddr );
    }

    void OnProtcolCLMessageReceived ( int              iRecID,
                                      CVector<uint8_t> vecbyMesBodyData,
                                      CHostAddress     RecHostAddr )
    {
        emit DetectedCLMessage ( vecbyMesBodyData, iRecID, RecHostAddr );
    }

    void OnNewConnection() { emit NewConnection(); }

signals:
    void MessReadyForSending ( CVector<uint8_t> vecMessage );
    void NewConnection();
    void ReqJittBufSize();
    void JittBufSizeChanged ( int iNewJitBufSize );
    void ServerAutoSockBufSizeChange ( int iNNumFra );
    void ReqConnClientsList();
    void ConClientListNameMesReceived ( CVector<CChannelInfo> vecChanInfo );
    void ConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo );
    void ChanInfoHasChanged();
    void ReqChanInfo();
    void OpusSupported();
    void ChatTextReceived ( QString strChatText );
    void ReqNetTranspProps();
    void LicenceRequired ( ELicenceType eLicenceType );
    void Disconnected();

    void DetectedCLMessage ( CVector<uint8_t> vecbyMesBodyData,
                             int              iRecID,
                             CHostAddress     RecHostAddr );

    void ParseMessageBody ( CVector<uint8_t> vecbyMesBodyData,
                            int              iRecCounter,
                            int              iRecID );
};

#endif /* !defined ( CHANNEL_HOIH9345KJH98_3_4344_BB23945IUHF1912__INCLUDED_ ) */
