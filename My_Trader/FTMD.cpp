#include "stdafx.h"
using namespace std;
CFTMD::CFTMD()
{

}

CFTMD::~CFTMD()
{
	
}

void CFTMD::Init(AccountInfo account_info, CFTTD* pTdHandler)
{
	// 产生一个CThostFtdcMdApi实例
	memset(qh_BrokerID, 0, sizeof(qh_BrokerID));
	memset(qh_MDAddress, 0, sizeof(qh_MDAddress));
	memset(qh_UserID, 0, sizeof(qh_UserID));
	memset(qh_Password, 0, sizeof(qh_Password));
	strcpy_s(qh_BrokerID, account_info.BrokerID.c_str());
	strcpy_s(qh_UserID, account_info.UserID.c_str());
	strcpy_s(qh_Password, account_info.Password.c_str());
	g_pTdHandler = pTdHandler;

	m_pMdApi = CThostFtdcMdApi::CreateFtdcMdApi();
	m_pMdApi->RegisterSpi(this);
	for (list<string>::iterator it = account_info.MdAddress.begin(); it != account_info.MdAddress.end(); it++)
	{
		strcpy_s(qh_MDAddress, (*it).c_str());
		m_pMdApi->RegisterFront(qh_MDAddress);
	}
	hEvent = CreateEvent(NULL, false, false, NULL);
	m_pMdApi->Init();
	if (WaitForSingleObject(hEvent, 30000) == WAIT_TIMEOUT)
		printf("***%s*** 行情前端登录错误 该线程已退出\n", g_pTdHandler->g_AccountInfo.AccountName.c_str());
	else
		printf("***%s*** 行情前端登陆成功\n", g_pTdHandler->g_AccountInfo.AccountName.c_str());
	//等待行情订阅完毕
	int waitingtimes = 0;
	while (LastDepth.size() < g_pTdHandler->Instruments.size())
	{
	
		Sleep(500);
		waitingtimes++;
		if (waitingtimes > 4)
			break;
		cout << account_info.AccountName<<g_pTdHandler->Instruments.size()- LastDepth.size() << "  "<<waitingtimes<<endl;
	}

}

void CFTMD::OnFrontConnected()
{
	CThostFtdcReqUserLoginField reqUserLogin;
	memset(&reqUserLogin,0,sizeof(reqUserLogin));
	strcpy_s(reqUserLogin.BrokerID, qh_BrokerID);
	strcpy_s(reqUserLogin.UserID, qh_UserID);
	strcpy_s(reqUserLogin.Password, qh_Password);
	int login = m_pMdApi->ReqUserLogin(&reqUserLogin,1);
}
void CFTMD::OnFrontDisconnected(int nReason)
{
	printf("***%s*** 行情连接断开 原因代码:%d\n", g_pTdHandler->g_AccountInfo.AccountName.c_str(), nReason);
}
void CFTMD::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
	if (pRspInfo->ErrorID == 0)
		initSubMD();
	else
		printf("***%s*** 行情前端登陆错误 ErrorID=%d ErrorMsg=%s 当前日期=%s\n",g_pTdHandler->g_AccountInfo.AccountName.c_str(), pRspInfo->ErrorID, pRspInfo->ErrorMsg, pRspUserLogin->TradingDay);
}

void CFTMD::initSubMD()
{
    //根据合约列表订阅行情
	int n_count = g_pTdHandler->Instruments.size();
	char ** codes = new char*[n_count];
	TThostFtdcInstrumentIDType* InstrumentIDs = new TThostFtdcInstrumentIDType[n_count];
	int i = 0;
	for (list<CThostFtdcInstrumentField>::iterator it = g_pTdHandler->Instruments.begin(); it!=g_pTdHandler->Instruments.end(); it++)
	{
		strcpy_s(InstrumentIDs[i], it->InstrumentID);
		codes[i] = InstrumentIDs[i];
		i++;
		
	}
	m_pMdApi->SubscribeMarketData(codes, n_count);

	delete[] codes;
	delete[] InstrumentIDs;
}
//void CFTMD::test()
//{
//	cout <<"LastDepth['jjssd'].LastPrice " <<LastDepth["jjssd"].LastPrice << endl;
//}
void CFTMD::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (bIsLast)
		SetEvent(hEvent);
}
void CFTMD::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) 
{
	CThostFtdcDepthMarketDataField *pMD;
	LastDepth[pDepthMarketData->InstrumentID] = *pDepthMarketData;
	//g_pTdHandler->g_pLog->printLog("ID：%s \n", pDepthMarketData->InstrumentID);
	pMD = &LastDepth[pDepthMarketData->InstrumentID];
	pMD->LastPrice = (pMD->LastPrice > 10000000.0) ? 0 : pMD->LastPrice;                          ///最新价
	pMD->OpenPrice = (pMD->OpenPrice > 10000000.0) ? pMD->LastPrice : pMD->OpenPrice;             ///今开盘
	pMD->HighestPrice = (pMD->HighestPrice > 10000000.0) ? pMD->LastPrice : pMD->HighestPrice;    ///最高价
	pMD->LowestPrice = (pMD->LowestPrice > 10000000.0) ? pMD->LastPrice : pMD->LowestPrice;       ///最低价
	pMD->BidPrice1 = (pMD->BidPrice1 > 10000000.0) ? pMD->LastPrice : pMD->BidPrice1;             ///申买价一
	pMD->AskPrice1 = (pMD->AskPrice1 > 10000000.0) ? pMD->LastPrice : pMD->AskPrice1;             ///申卖价一
	pMD->AveragePrice = (pMD->AveragePrice > 10000000.0) ? pMD->LastPrice : pMD->AveragePrice;    ///当日均
}
double CFTMD::GetTradePrice(OrderStruct order)
{
	double price = 0.001;
	if (LastDepth.size() == 0)
	{
		printf("***%s*** CFTTD::GetTradePrice 提取行情数据错误 该线程已退出\n",g_pTdHandler->g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
	else if (order.direction == THOST_FTDC_DEN_Buy)
		price = LastDepth[order.code].AskPrice1;
	else if (order.direction == THOST_FTDC_DEN_Sell)
		price = LastDepth[order.code].BidPrice1;
	else
		;
	//对于未定义到的合约，设定价格为0.001，小于目前所有合约的最小变动单位，报单不会有效。
	if (price > 10000 * 1000)
		price = 0.001;
	else if (price < -10000 * 1000)
		price = 0.001;
	return price;
}

double CFTMD::GetVolume(string code)
{
	if (LastDepth.size() == 0)
	{
		printf("***%s*** CFTTD::GetVolume 提取行情数据错误 该线程已退出\n",g_pTdHandler->g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
	return LastDepth[code].Volume;
}

void CFTMD::AccountLogout()
{
	UnsubscribeMD();
	CThostFtdcUserLogoutField UserLogoutField;
	memset(&UserLogoutField, 0, sizeof(UserLogoutField));
	CThostFtdcUserLogoutField * pUserLogoutField = &UserLogoutField;
	strcpy_s(pUserLogoutField->BrokerID, g_pTdHandler->g_AccountInfo.BrokerID.c_str());
	strcpy_s(pUserLogoutField->UserID, g_pTdHandler->g_AccountInfo.UserID.c_str());
	ResetEvent(hEvent);
	m_pMdApi->ReqUserLogout(pUserLogoutField, 1);
	if (WaitForSingleObject(hEvent, 10000) == WAIT_TIMEOUT)
	{
		g_pTdHandler->g_pLog->printLog("行情前端登出失败\n");
		printf("***%s*** 行情前端登出失败 该线程已退出\n", g_pTdHandler->g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
	else
	{
		g_pTdHandler->g_pLog->printLog("行情前端登出\n");
		printf("***%s*** 行情前端登出\n", g_pTdHandler->g_AccountInfo.AccountName.c_str());
	}

}

void CFTMD::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo->ErrorID == 0)
		SetEvent(hEvent);
}

void CFTMD::UnsubscribeMD()
{
	//取消所订阅的行情列表
	int n_count = g_pTdHandler->Instruments.size();
	char ** codes = new char*[n_count];
	TThostFtdcInstrumentIDType* InstrumentIDs = new TThostFtdcInstrumentIDType[n_count];
	int i = 0;
	for (list<CThostFtdcInstrumentField>::iterator it = g_pTdHandler->Instruments.begin(); it != g_pTdHandler->Instruments.end(); it++)
	{
		strcpy_s(InstrumentIDs[i], it->InstrumentID);
		codes[i] = InstrumentIDs[i];
		i++;
	}
	ResetEvent(hEvent);
	m_pMdApi->UnSubscribeMarketData(codes, n_count);
	if (WaitForSingleObject(hEvent, 10000) == WAIT_TIMEOUT)
	{
		g_pTdHandler->g_pLog->printLog("退订行情失败\n");
		printf("***%s*** 退订行情失败 该线程已退出\n", g_pTdHandler->g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
	else
	{
		g_pTdHandler->g_pLog->printLog("退订行情成功\n");
		printf("***%s*** 退订行情成功\n", g_pTdHandler->g_AccountInfo.AccountName.c_str());
	}
	delete[] codes;
	delete[] InstrumentIDs;
}

void CFTMD::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (bIsLast)
		SetEvent(hEvent);
}