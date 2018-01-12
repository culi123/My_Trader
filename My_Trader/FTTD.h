#pragma once
#include "stdafx.h"
class CFTTD : public CThostFtdcTraderSpi  
{
public:
	std::map<int,OrderStruct> orders;
	std::map<int,OrderStruct> TradeResult;
	CFTTD();
	virtual ~CFTTD();

	bool bWannaLogin;
	bool bIsgetInst;
	bool bWannaDealMsg = false;
	HANDLE hEvent;
	std::list<CThostFtdcInstrumentField> Instruments;
	int order_count;
	CONST int RETRY_TIMES = 9;
	CONST double BIG_ORDER_FACTOR = 0.0001;
	std::map<std::string,int> TargetPosition;
	std::list<CThostFtdcInvestorPositionField> CurrentPosition;
	std::list<int> RequestIdDealed;
    TThostFtdcBrokerIDType qh_BrokerID;
	TThostFtdcAddressType qh_TDAddress;
	TThostFtdcUserIDType qh_UserID;
    TThostFtdcPasswordType qh_Password;
	logInfo * g_pLog;
	AccountInfo g_AccountInfo;

	void GetOrders();
	void GetTargetPosition();
	void GetCurrentPosition();
    void Init(AccountInfo account_info,void * pMdHandler, logInfo * pLogInfo);
    void OnFrontConnected();
	void OnFrontDisconnected(int nReason);
	void GetInstruments();
    void QueryAcct();
	void ConfirmSettleInfo();
	std::string GetTradeCode(std::string code_raw);
    void PlaceOrder();
	void SubmitOrder(OrderStruct order);
	void AccountLogout();

	void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);


	///请求查询投资者持仓响应
	void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询资金账户响应
	void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	
	///错误应答
	void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	
	///报单录入错误回报
	void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);

	///报单操作错误回报
	void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);

	///报单通知
	void OnRtnOrder(CThostFtdcOrderField *pOrder);

	///成交通知
	void OnRtnTrade(CThostFtdcTradeField *pTrade);

	//账号登出通知
	void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

private:
	CThostFtdcTraderApi* m_pTdApi;
	void * g_pMdHandler;
};
