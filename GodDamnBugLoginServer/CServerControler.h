#pragma once

#include "stdafx.h"

class CServerControler
{
public:

	CServerControler(void)
		:mbControlModeFlag(FALSE)
		,mbShutdownFlag(FALSE)
		,mpLanMonitoringClient(nullptr)
		,mpLanLoginServer(nullptr)
		,mpNetLoginServer(nullptr)
	{
	}

	~CServerControler(void)
	{
	}

	BOOL GetShutdownFlag(void) const
	{
		return mbShutdownFlag;
	}

	void SetLanMonitoringClient(CLanMonitoringClient* pLanMonitoringClient)
	{
		mpLanMonitoringClient = pLanMonitoringClient;

		return;
	}

	void SetLanLoginServer(CLanLoginServer* pLanLoginServer)
	{
		mpLanLoginServer = pLanLoginServer;

		return;
	}

	void SetNetLoginServer(CNetLoginServer* pNetLoginServer)
	{
		mpNetLoginServer = pNetLoginServer;

		return;
	}

	void ServerControling(void)
	{

		if (_kbhit() == TRUE)
		{
			WCHAR controlKey = _getwch();

			if (controlKey == L'u' || controlKey == L'U')
			{
				mbControlModeFlag = TRUE;
			}

			if ((controlKey == L'd' || controlKey == L'D') && mbControlModeFlag)
			{
				CCrashDump::Crash();
			}

			if ((controlKey == L'q' || controlKey == L'Q') && mbControlModeFlag)
			{
				mbShutdownFlag = TRUE;
			}

			if (controlKey == L'l' || controlKey == L'L')
			{
				mbControlModeFlag = FALSE;
			}
		}

		wprintf_s(L"\n\n\n\n"
			L"	                                                       [ Login Server ] \n"
			L" 旨收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收旬\n\n"
			L"    Net Login Bind IP : %s | NetLogin Bind Port : %d | NetLogin Accept Total : %lld | Current Client : %4d / %4d \n\n"
			L"    Running Thread : %d | Worker Thread : %d | Net Nagle : %d | GameAcceptTotal : %lld | ChatAcceptTotal : %lld \n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Monitoring Bind IP : %s | Monitoring Bind Port : %d | Connect State : %d\n\n"
			L"    Monitoring Running Thread : %d | Monitoring Worker Thread : %d | Monitoring Nagle : %d    \n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    LanLogin Bind IP : %s | LanLogin Bind Port : %d | LanLogin Current Client : %d\n\n"
			L"    LanLogin Running Thread : %d | LanLogin Worker Thread : %d | LanLogin Nagle : %d\n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Control Mode : %d | [ L ] : Control Lock | [ U ] : Control Unlock | [ D ] : Crash | [ Q ] : Exit | LogLevel : %d      \n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Chunk Message Alloc Count : %d | Chunk ConnectionState Alloc Count : %d\n\n"
			L" 曲收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收旭\n\n"
			, mpNetLoginServer->GetServerBindIP(), mpNetLoginServer->GetServerBindPort(), mpNetLoginServer->GetAcceptTotal()
			, mpNetLoginServer->GetCurrentClientCount(), mpNetLoginServer->GetMaxClientCount(), mpNetLoginServer->GetRunningThreadCount(), mpNetLoginServer->GetWorkerThreadCount(), mpNetLoginServer->GetNagleFlag()
			, mpNetLoginServer->GetGameAcceptTotal(),mpNetLoginServer->GetChatAcceptTotal(), mpLanMonitoringClient->GetConnectIP(), mpLanMonitoringClient->GetConnectPort(), mpLanMonitoringClient->GetConnectStateFlag()
			, mpLanMonitoringClient->GetRunningThreadCount(), mpLanMonitoringClient->GetWorkerThreadCount(), mpLanMonitoringClient->GetNagleFlag()
			, mpLanLoginServer->GetServerBindIP(), mpLanLoginServer->GetServerBindPort(), mpLanLoginServer->GetCurrentClientCount()
			, mpLanLoginServer->GetRunningThreadCount(), mpLanLoginServer->GetWorkerThreadCount(), mpLanLoginServer->GetNagleFlag()
			, mbControlModeFlag, CSystemLog::GetInstance()->GetLogLevel()
			, CLockFreeObjectFreeList<CTLSLockFreeObjectFreeList<CMessage>::CChunk>::GetAllocNodeCount()
			, CLockFreeObjectFreeList<CTLSLockFreeObjectFreeList<CNetLoginServer::CConnectionState>::CChunk>::GetAllocNodeCount()
		);

		return;
	}

private:

	BOOL mbControlModeFlag;

	BOOL mbShutdownFlag;

	CLanMonitoringClient* mpLanMonitoringClient;

	CLanLoginServer* mpLanLoginServer;

	CNetLoginServer* mpNetLoginServer;

};

