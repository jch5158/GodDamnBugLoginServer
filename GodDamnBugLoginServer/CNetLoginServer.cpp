#include "stdafx.h"
#include "CNetLoginServer.h"

CNetLoginServer::CNetLoginServer(void)
	: mbDisconnectThreadFlag(FALSE)
	, mbWhiteModeFlag(FALSE)
	, mGameServerPort(0)
	, mChatServerPort(0)
	, mDisconnectThreadID(0)
	, mAuthenticTPS(0)
	, mGameAcceptTotal(0)
	, mChatAcceptTotal(0)
	, mDisconnectThread(INVALID_HANDLE_VALUE)
	, mDisconnectEvent(CreateEvent(NULL,FALSE,FALSE,nullptr))
	, mConnectionStateMapLock{ 0, }
	, mGameServerIP{ 0, }
	, mChatServerIP{ 0, }
	, mDBConnector()
	, mRedisConnector()
	, mConnectionStateMap()
{
	
	InitializeCriticalSection(&mConnectionStateMapLock);
}


CNetLoginServer::~CNetLoginServer(void)
{
	
	DeleteCriticalSection(&mConnectionStateMapLock);
}

BOOL CNetLoginServer::OnStart(void)
{
	mConnectionStateMap.reserve(GetMaxClientCount());

	setupDisconnectThread();

	return TRUE;
}

void CNetLoginServer::OnClientJoin(UINT64 sessionID)
{
	return;
}


void CNetLoginServer::OnClientLeave(UINT64 sessionID)
{
	EnterCriticalSection(&mConnectionStateMapLock);

	CConnectionState* pConnectionState = findConnectionState(sessionID);
	if (pConnectionState == nullptr)
	{
		LeaveCriticalSection(&mConnectionStateMapLock);

		return;
	}

	EnterCriticalSection(&pConnectionState->mConnectionStateLock);

	LeaveCriticalSection(&mConnectionStateMapLock);

	pConnectionState->mbConnectionFlag = FALSE;

	LeaveCriticalSection(&pConnectionState->mConnectionStateLock);

	return;
}

void CNetLoginServer::OnStartAcceptThread(void)
{
	accountDBConnect();

	return;
}

void CNetLoginServer::OnStartWorkerThread(void)
{	
	accountDBConnect();

	mRedisConnector.Connect();

	return;
}


void CNetLoginServer::OnRecv(UINT64 sessionID, CMessage* pMessage)
{
	WORD messageType;

	*pMessage >> messageType;

	if (recvProcedure(sessionID, messageType, pMessage) == FALSE)
	{
		Disconnect(sessionID);
	}

	return;
}

void CNetLoginServer::OnCloseAcceptThread(void)
{	
	mDBConnector.Disconnect();

	return;
}

void CNetLoginServer::OnCloseWorkerThread(void)
{
	mDBConnector.Disconnect();

	mRedisConnector.Disconnect();

	return;
}


BOOL CNetLoginServer::OnConnectionRequest(const WCHAR* pUserIP, WORD userPort)
{
	BOOL retval = TRUE;

	if (mbWhiteModeFlag == TRUE)
	{
		RECONNECT:

		if (mDBConnector.Query((WCHAR*)L"SELECT EXISTS (SELECT * FROM whiteip where ip = '%s') AS SUCCESS", pUserIP) == FALSE)
		{
			mDBConnector.Reconnect();

			goto RECONNECT;
		}

		mDBConnector.StoreResult();

		MYSQL_ROW mysqlRow = mDBConnector.FetchRow();
		if (mysqlRow != nullptr)
		{
			retval = FALSE;
		}

		mDBConnector.FreeResult();
	}

	return retval;
}


void CNetLoginServer::OnError(DWORD errorCode, const WCHAR* errorMessage)
{


	return;
}


void CNetLoginServer::OnStop(void)
{
	closeDisconnectThread();

	clearConnectionStateMap();

	return;
}


void CNetLoginServer::NotificationSessionConnect(BYTE serverType)
{
	if (serverType == dfSERVER_TYPE_GAME)
	{
		InterlockedIncrement64((INT64*)&mGameAcceptTotal);
	}
	else if (serverType == dfSERVER_TYPE_CHAT)
	{
		InterlockedIncrement64((INT64*)&mChatAcceptTotal);
	}
	else
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CNetLoginServer", L"[NotificationSessionConnect] ServerType : %d", serverType);

		CCrashDump::Crash();
	}

	return;
}

void CNetLoginServer::SetWhiteModeFlag(BOOL bWhiteModeFlag)
{
	mbWhiteModeFlag = bWhiteModeFlag;

	return;
}


BOOL CNetLoginServer::GetWhiteModeFlag(void) const
{
	return mbWhiteModeFlag;
}


BOOL CNetLoginServer::SetGameServerIP(WCHAR* pGameServerIP)
{
	HRESULT retval = StringCbCopyW(mGameServerIP, netloginserver::MAX_IP_LENGTH, pGameServerIP);
	if (FAILED(retval) == TRUE)
	{
		return FALSE;
	}

	return TRUE;
}


BOOL CNetLoginServer::GetGameServerIP(WCHAR* pGameServerIP, DWORD cbLength) const
{
	HRESULT retval = StringCbCopyW(pGameServerIP, cbLength, mGameServerIP);
	if (FAILED(retval) == TRUE)
	{
		return FALSE;
	}

	return TRUE;
}


void CNetLoginServer::SetGameServerPort(WORD gameServerPort)
{
	mGameServerPort = gameServerPort;

	return;
}


WORD CNetLoginServer::GetGameServerPort(void) const
{
	return mGameServerPort;
}


BOOL CNetLoginServer::SetChatServerIP(WCHAR* pChatServerIP)
{
	HRESULT retval = StringCbCopyW(mChatServerIP, netloginserver::MAX_IP_LENGTH, pChatServerIP);
	if (FAILED(retval) == TRUE)
	{
		return FALSE;
	}

	return TRUE;
}


BOOL CNetLoginServer::GetChatServerIP(WCHAR* pChatServerIP, DWORD cbLength) const
{
	HRESULT retval = StringCbCopyW(pChatServerIP, cbLength, mChatServerIP);
	if (FAILED(retval) == TRUE)
	{
		return FALSE;
	}

	return TRUE;
}


void CNetLoginServer::SetChatServerPort(WORD chatServerPort)
{
	mChatServerPort = chatServerPort;

	return;
}

WORD CNetLoginServer::GetChatServerPort(void) const
{
	return mChatServerPort;
}

LONG CNetLoginServer::GetAuthenticTPS(void) const
{
	return mAuthenticTPS;
}


UINT64 CNetLoginServer::GetGameAcceptTotal(void) const
{
	return mGameAcceptTotal;
}

UINT64 CNetLoginServer::GetChatAcceptTotal(void) const
{
	return mChatAcceptTotal;
}

void CNetLoginServer::InitializeAuthenticTPS(void)
{
	InterlockedExchange(&mAuthenticTPS, 0);

	return;
}


DWORD WINAPI CNetLoginServer::ExecuteDisconnectThread(void* pParam)
{
	CNetLoginServer* pNetLoginServer = (CNetLoginServer*)pParam;

	pNetLoginServer->DisconnectThread();

	return 1;
}


void CNetLoginServer::DisconnectThread(void)
{
	// ���� Ŀ���͸� �̿��ؼ� token�� ����� ���� Connect�� �Ѵ�.
	mRedisConnector.Connect();

	while (mbDisconnectThreadFlag == TRUE)
	{
		// mConnectionStateMap Lock
		EnterCriticalSection(&mConnectionStateMapLock);

		// ����� ������ ���ٸ� Block 
		if (mConnectionStateMap.size() == 0)
		{
			// mConnectionStateMap unLock
			LeaveCriticalSection(&mConnectionStateMapLock);

			if (WaitForSingleObject(mDisconnectEvent, INFINITE) != WAIT_OBJECT_0)
			{
				CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CNetLoginServer", L"[DisconnectThread] WaitForSingObject Error Code : %d", GetLastError());

				CCrashDump::Crash();
			}

			continue;
		}


		auto iterE = mConnectionStateMap.end();
		for (auto iter = mConnectionStateMap.begin(); iter != iterE; ++iter)
		{
			CConnectionState* pConnectionState = iter->second;

			{
				CCriticalSection criticalSection(&pConnectionState->mConnectionStateLock);
			}

			if (pConnectionState->mbConnectionFlag == FALSE || timeGetTime() - pConnectionState->mLoginTime > 60000)
			{
				// ���� Ŀ�ؼ� �ʿ��� �����Ѵ�.
				if (eraseConnectionStateMap(pConnectionState->mSessionID) == FALSE)
				{
					CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CNetLoginServer", L"[DisconnectThread] erase Failed");

					CCrashDump::Crash();
				}
				
				Disconnect(pConnectionState->mSessionID);

				// CConnection ��ü�� �ݳ��Ѵ�.
				pConnectionState->Free();
			}
		}


		// mConnectionStateMap unLock
		LeaveCriticalSection(&mConnectionStateMapLock);

		// 1�� �ֱ�� Ȯ���ϱ� ����
		Sleep(1000);
	}

	mRedisConnector.Disconnect();

	return;
}







BOOL CNetLoginServer::setupDisconnectThread(void)
{
	mbDisconnectThreadFlag = TRUE;

	mDisconnectThread = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)ExecuteDisconnectThread, this, 0, (UINT*)&mDisconnectThreadID);
	if (mDisconnectThread == INVALID_HANDLE_VALUE)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CNetLoginServer", L"[setupDisconnectThread] _beginthreadex Error Code : %d", GetLastError());

		return FALSE;
	}

	return TRUE;
}

void CNetLoginServer::closeDisconnectThread(void)
{
	mbDisconnectThreadFlag = FALSE;

	if (WaitForSingleObject(mDisconnectThread, INFINITE) != WAIT_OBJECT_0)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CNetLoginServer", L"[closeDisconnectThread] WaitForSingleObject Error Code : %d", GetLastError());

		CCrashDump::Crash();
	}

	CloseHandle(mDisconnectThread);

	return;
}




BOOL CNetLoginServer::recvProcedure(UINT64 sessionID, WORD messageType, CMessage* pMessage)
{
	switch (messageType)
	{
	case en_PACKET_CS_LOGIN_REQ_LOGIN:

		return recvProcedureLoginRequest(sessionID, pMessage);

	default:

		return FALSE;
	}

	return TRUE;
}


BOOL CNetLoginServer::recvProcedureLoginRequest(UINT64 sessionID, CMessage* pMessage)
{
	InterlockedIncrement(&mAuthenticTPS);
	
	UINT64 accountNo;

	WCHAR userID[netloginserver::PLAYER_STRING];

	WCHAR userNick[netloginserver::PLAYER_STRING];

	CHAR status;

	CHAR sessionKey[65];

	sessionKey[64] = '\0';

	*pMessage >> accountNo;

	// ���̴� ��ū ������ ���� �ʴ´�.
	if (accountNo > 999999)
	{
		pMessage->GetPayload(sessionKey, 64);

		pMessage->MoveReadPos(64);

		if (mRedisConnector.SetEx(accountNo, 60, sessionKey) == FALSE)
		{
			CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[recvProcedureLoginRequest] Redis SetEx Faild");

			CCrashDump::Crash();
		}
	}

	// ID�� NickName �� ��ȸ�ϰ� �ƿ� �Ķ���ͷ� ����.
	if (getAccountInfo(accountNo, userID, userNick, &status) == FALSE)
	{
		return FALSE;
	}

	WCHAR clientIP[30];

	DWORD clientPort;

	if (GetClientIP(sessionID, clientIP, 30) == FALSE)
	{
		clientIP[0] = L'\0';
	}

	if (GetClientPort(sessionID, &clientPort) == FALSE)
	{
		clientPort = 0;
	}
	
	setAccountStatus(accountNo, status, clientIP, clientPort);	

	CConnectionState* pConnectionState = CConnectionState::Alloc();

	pConnectionState->mAccountNo = accountNo;

	pConnectionState->mSessionID = sessionID;
	
	pConnectionState->mLoginTime = timeGetTime();


	{
		CCriticalSection criticalSection(&mConnectionStateMapLock);	

		insertConnectionStateMap(sessionID, pConnectionState);
	}


	SetEvent(mDisconnectEvent);

	sendLoginResponse(sessionID, accountNo, dfLOGIN_STATUS_OK, userID, userNick);

	return TRUE;
}


void CNetLoginServer::sendLoginResponse(UINT64 sessionID, UINT64 accountNo, BYTE status, WCHAR* pUserID, WCHAR* pUserNickName)
{
	CMessage* pMessage = CMessage::Alloc();

	packingLoginResponse(accountNo, status, pUserID, pUserNickName, pMessage);
	
	SendPacket(sessionID, pMessage);

	pMessage->Free();

	return;
}

void CNetLoginServer::packingLoginResponse(UINT64 accountNo, BYTE status, WCHAR* pUserID, WCHAR* pUserNickName, CMessage* pMessage)
{
	*pMessage << (WORD)en_PACKET_CS_LOGIN_RES_LOGIN << accountNo << status;

	pMessage->PutPayload((CHAR*)pUserID, netloginserver::PLAYER_STRING * sizeof(WCHAR));
	pMessage->MoveWritePos(netloginserver::PLAYER_STRING * sizeof(WCHAR));

	pMessage->PutPayload((CHAR*)pUserNickName, netloginserver::PLAYER_STRING * sizeof(WCHAR));
	pMessage->MoveWritePos(netloginserver::PLAYER_STRING * sizeof(WCHAR));

	if(accountNo < 1000000)
	{ 
		pMessage->PutPayload((CHAR*)mGameServerIP, netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));
		pMessage->MoveWritePos(netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));

		*pMessage << mGameServerPort;

		pMessage->PutPayload((CHAR*)mChatServerIP, netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));
		pMessage->MoveWritePos(netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));

		*pMessage << mChatServerPort;
	}
	else
	{
		pMessage->PutPayload((CHAR*)L"106.245.38.107", netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));
		pMessage->MoveWritePos(netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));

		*pMessage << mGameServerPort;

		pMessage->PutPayload((CHAR*)L"106.245.38.107", netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));
		pMessage->MoveWritePos(netloginserver::PACKET_IP_LENGTH * sizeof(WCHAR));

		*pMessage << mChatServerPort;
	}
	
	return;
}

void CNetLoginServer::accountDBConnect(void)
{
RECONNECT:

	if (mDBConnector.Connect((WCHAR*)L"127.0.0.1", 10950, (WCHAR*)L"accountdb", (WCHAR*)L"chanhun", (WCHAR*)L"Cksgns123$") == FALSE)
	{
		if (mDBConnector.CheckReconnectErrorCode() == TRUE)
		{
			goto RECONNECT;

			Sleep(100);
		}

		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[accountDBConnect] Error Code : %d, Error Message : %s", mDBConnector.GetLastError(), mDBConnector.GetLastErrorMessage());

		CCrashDump::Crash();
	}

	return;
}


void CNetLoginServer::accountDBReConnect(void)
{
RECONNECT:

	if (mDBConnector.Reconnect() == FALSE)
	{
		if (mDBConnector.CheckReconnectErrorCode() == TRUE)
		{
			goto RECONNECT;

			Sleep(100);
		}

		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[accountDBReConnect] Error Code : %d, Error Message : %s", mDBConnector.GetLastError(), mDBConnector.GetLastErrorMessage());

		CCrashDump::Crash();
	}

	return;
}

// dummyAccount ���� Ȯ���Ѵ�.
BOOL CNetLoginServer::checkDummyAccount(UINT64 accountNo)
{
	if (accountNo <= netloginserver::MAX_DUMMY_ACOUNT_NO)
	{
		return TRUE;
	}

	return FALSE;
}



BOOL CNetLoginServer::getAccountInfo(UINT64 accountNo, WCHAR* pUserIP, WCHAR* pUserNick, CHAR* pStatus)
{
REQUERY:

	if (mDBConnector.Query((WCHAR*)L"SELECT userid, usernick, status FROM `v_account` WHERE accountno = %lld;", accountNo) == FALSE)
	{
		accountDBReConnect();

		goto REQUERY;
	}

	mDBConnector.StoreResult();

	MYSQL_ROW mysqlRow = mDBConnector.FetchRow();
	if (mysqlRow == nullptr)
	{
		return FALSE;
	}

	if (MultiByteToWideChar(CP_ACP, 0, mysqlRow[0], strlen(mysqlRow[0]) + 1, pUserIP, netloginserver::PLAYER_STRING) == FALSE)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[recvProcedureLoginRequest] MultiByteToWideChar Error Code : %d", GetLastError());

		CCrashDump::Crash();
	}

	if (MultiByteToWideChar(CP_ACP, 0, mysqlRow[1], strlen(mysqlRow[1]) + 1, pUserNick, netloginserver::PLAYER_STRING) == FALSE)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[recvProcedureLoginRequest] MultiByteToWideChar Error Code : %d", GetLastError());

		CCrashDump::Crash();
	}

	*pStatus = atoi(mysqlRow[2]);

	mDBConnector.FreeResult();

	return TRUE;
}




void CNetLoginServer::setAccountStatus(UINT64 accountNo, CHAR status, WCHAR* pClientIP, DWORD clientPort)
{

REQUERY:

	if (mDBConnector.Query((WCHAR*)L"begin;") == FALSE)
	{
		if (mDBConnector.CheckReconnectErrorCode() == TRUE)
		{
			accountDBReConnect();

			goto REQUERY;
		}

		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[setAccountStatus] Error Code : %d, Error Message : %s", mDBConnector.GetLastError(), mDBConnector.GetLastErrorMessage());

		CCrashDump::Crash();
	}

	// status ���̺� Ȯ�� �� ����
	if (status != dfLOGIN_STATUS_GAME)
	{
		if (mDBConnector.Query((WCHAR*)L"UPDATE `status` SET `status` = '%d' WHERE (`accountno` = '%lld');", dfLOGIN_STATUS_OK, accountNo) == FALSE)
		{
			if (mDBConnector.CheckReconnectErrorCode() == TRUE)
			{
				accountDBReConnect();

				goto REQUERY;
			}

			CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[setAccountStatus] Error Code : %d, Error Message : %s", mDBConnector.GetLastError(), mDBConnector.GetLastErrorMessage());

			CCrashDump::Crash();
		}
	}

	if (mDBConnector.Query((WCHAR*)L"INSERT INTO `logdb`.`gamelog` (`logtime`, `accountno`,`servername`, `type`, `code`, `param1`,`message`) VALUES(NOW(), '%lld', 'Login', '100', '101', '%d','%s:%d');",
		accountNo, dfLOGIN_STATUS_OK, pClientIP, clientPort) == FALSE)
	{
		if (mDBConnector.CheckReconnectErrorCode() == TRUE)
		{
			mDBConnector.Reconnect();

			goto REQUERY;
		}

		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CGodDamnBug", L"[setAccountStatus] MySQL Error Code : %d, Error Message : %s", mDBConnector.GetLastError(), mDBConnector.GetLastErrorMessage());

		CCrashDump::Crash();
	}


	if (mDBConnector.Query((WCHAR*)L"commit;") == FALSE)
	{
		if (mDBConnector.CheckReconnectErrorCode() == TRUE)
		{
			accountDBReConnect();

			goto REQUERY;
		}

		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"NetLoginServer", L"[setAccountStatus] Error Code : %d, Error Message : %s", mDBConnector.GetLastError(), mDBConnector.GetLastErrorMessage());

		CCrashDump::Crash();
	}

	return;
}



BOOL CNetLoginServer::insertConnectionStateMap(UINT64 sessionID, CConnectionState* pConnectionState)
{
	return mConnectionStateMap.insert(std::make_pair(sessionID, pConnectionState)).second;
}

BOOL CNetLoginServer::eraseConnectionStateMap(UINT64 sessionID)
{
	return mConnectionStateMap.erase(sessionID);
}


CNetLoginServer::CConnectionState* CNetLoginServer::findConnectionState(UINT64 sessionID)
{
	auto iter = mConnectionStateMap.find(sessionID);
	if (iter == mConnectionStateMap.end())
	{
		return nullptr;
	}

	return iter->second;
}


void CNetLoginServer::clearConnectionStateMap(void)
{
	CCriticalSection criticalSection(&mConnectionStateMapLock);

	for (auto &iter : mConnectionStateMap)
	{	
		CConnectionState* pConnectionState = iter.second;

		EnterCriticalSection(&pConnectionState->mConnectionStateLock);

		LeaveCriticalSection(&pConnectionState->mConnectionStateLock);

		mConnectionStateMap.erase(iter.first);

		pConnectionState->Free();
	}

	return;
}

