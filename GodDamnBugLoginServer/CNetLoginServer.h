#pragma once

namespace netloginserver
{
	constexpr DWORD PLAYER_STRING = 20;

	constexpr DWORD MAX_IP_LENGTH = 50;

	constexpr DWORD PACKET_IP_LENGTH = 16;

	constexpr DWORD MAX_DUMMY_ACOUNT_NO = 999999;
}

class CNetLoginServer : public CNetServer
{
public:

	CNetLoginServer(void);

	~CNetLoginServer(void);

	virtual BOOL OnStart(void) final;

	virtual void OnClientJoin(UINT64 sessionID) final;

	virtual void OnClientLeave(UINT64 sessionID) final;

	virtual void OnStartAcceptThread(void) final;

	virtual void OnStartWorkerThread(void) final;

	virtual void OnRecv(UINT64 sessionID, CMessage* pMessage) final;

	virtual void OnCloseAcceptThread(void) final;

	virtual void OnCloseWorkerThread(void) final;

	// accept 직후 바로 호출
	virtual BOOL OnConnectionRequest(const WCHAR* pUserIP, WORD userPort) final;

	virtual void OnError(DWORD errorCode, const WCHAR* errorMessage) final;

	virtual void OnStop(void) final;

	void NotificationSessionConnect(BYTE serverType);

	void SetWhiteModeFlag(BOOL bWhiteModeFlag);

	BOOL GetWhiteModeFlag(void) const;

	BOOL SetGameServerIP(WCHAR* pGameServerIP);

	BOOL GetGameServerIP(WCHAR* pGameServerIP, DWORD cbLength) const;

	void SetGameServerPort(WORD gameServerPort);

	WORD GetGameServerPort(void) const;

	BOOL SetChatServerIP(WCHAR* pChatServerIP);

	BOOL GetChatServerIP(WCHAR* pChatServerIP, DWORD cbLength) const;

	void SetChatServerPort(WORD chatServerPort);

	WORD GetChatServerPort(void) const;

	LONG GetAuthenticTPS(void) const;

	UINT64 GetGameAcceptTotal(void) const;

	UINT64 GetChatAcceptTotal(void) const;

	void InitializeAuthenticTPS(void);


	struct CConnectionState
	{
	public:

		template <class DATA>
		friend class CTLSLockFreeObjectFreeList;

		static CConnectionState* Alloc(void)
		{
			CConnectionState* pConnectionState = mFreeList.Alloc();

			pConnectionState->Clear();

			return pConnectionState;
		}

		void Free(void)
		{
			if (mFreeList.Free(this) == FALSE)
			{
				CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"CNetLoginServer", L"[Free] CConnectionState Free Error");

				CCrashDump::Crash();
			}

			return;
		}

		void Clear(void)
		{
			mbConnectionFlag = TRUE;
			mAccountNo = 0;
			mSessionID = 0;
			mLoginTime = 0;

			return;
		}

	private:

		CConnectionState(void)
			: mbConnectionFlag(FALSE)
			, mAccountNo(0)
			, mSessionID(0)
			, mLoginTime(0)
			, mConnectionStateLock{ 0, }
		{
			InitializeCriticalSection(&mConnectionStateLock);
		}

		~CConnectionState(void)
		{
			DeleteCriticalSection(&mConnectionStateLock);
		}

		inline static CTLSLockFreeObjectFreeList<CConnectionState> mFreeList = { 0,FALSE };

	public:

		BOOL mbConnectionFlag;
		UINT64 mAccountNo;
		UINT64 mSessionID;
		//BOOL mbGameConnectionFlag;
		//BOOL mbChatConnectionFlag;
	
		// 로그인 타임
		DWORD mLoginTime;	

		// CConnectionState 객체 동기화 객체
		CRITICAL_SECTION mConnectionStateLock;
	};

private:

	static DWORD WINAPI ExecuteDisconnectThread(void* pParam);

	void DisconnectThread(void);

	BOOL setupDisconnectThread(void);

	void closeDisconnectThread(void);

	BOOL recvProcedure(UINT64 sessionID, WORD messageType, CMessage* pMessage);

	BOOL recvProcedureLoginRequest(UINT64 sessionID, CMessage* pMessage);

	void sendLoginResponse(UINT64 sessionID, UINT64 accountNo, BYTE status, WCHAR *pUserID, WCHAR *pUserNickName);

	void packingLoginResponse(UINT64 accountNo, BYTE status, WCHAR *pUserID, WCHAR* pUserNickName, CMessage* pMessage);

	void accountDBConnect(void);

	void accountDBReConnect(void);

	BOOL checkDummyAccount(UINT64 accountNo);

	BOOL getAccountInfo(UINT64 accountNo, WCHAR* pUserIP, WCHAR* pUserNick, CHAR* pStatus);
	
	void setAccountStatus(UINT64 accountNo, CHAR status, WCHAR *pClientIP, DWORD clientPort);

	BOOL insertConnectionStateMap(UINT64 sessionID, CConnectionState* pConnectionState);
	BOOL eraseConnectionStateMap(UINT64 sessionID);
	CConnectionState* findConnectionState(UINT64 sessionID);


	void clearConnectionStateMap(void);

	BOOL mbDisconnectThreadFlag;

	BOOL mbWhiteModeFlag;

	WORD mGameServerPort;

	WORD mChatServerPort;

	DWORD mDisconnectThreadID;

	LONG mAuthenticTPS;

	UINT64 mGameAcceptTotal;

	UINT64 mChatAcceptTotal;

	HANDLE mDisconnectThread;

	HANDLE mDisconnectEvent;


	// mConnectionStateMap을 위한 동기화 객체
	CRITICAL_SECTION mConnectionStateMapLock;

	WCHAR mGameServerIP[netloginserver::MAX_IP_LENGTH];

	WCHAR mChatServerIP[netloginserver::MAX_IP_LENGTH];

	CTLSDBConnector mDBConnector;

	CTLSRedisConnector mRedisConnector;


	// key : sessionID, value : CConectionState
	std::unordered_map<UINT64, CConnectionState*> mConnectionStateMap;

};

