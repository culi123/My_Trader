#include "stdafx.h"
using namespace std;
extern string GetExePath();
extern HANDLE hMutex;
CFTTD::CFTTD()
{ 
}

CFTTD::~CFTTD()
{
}

void CFTTD::Init(AccountInfo account_info, void * pMdHandler, logInfo * pLog)
{
    memset(qh_BrokerID, 0, sizeof(qh_BrokerID));
    memset(qh_TDAddress, 0, sizeof(qh_TDAddress));
    memset(qh_UserID, 0, sizeof(qh_UserID));
    memset(qh_Password, 0, sizeof(qh_Password));
	strcpy_s(qh_BrokerID, account_info.BrokerID.c_str());
    strcpy_s(qh_UserID, account_info.UserID.c_str());
    strcpy_s(qh_Password, account_info.Password.c_str());

    bIsgetInst = false;
	bWannaLogin = true;
	g_pMdHandler = pMdHandler;
	g_pLog = pLog;
	g_pLog->printLog("账号名称 %s\n", account_info.AccountName.c_str());
	g_AccountInfo = account_info;
	// 产生一个CThostFtdcTraderApi实例
	m_pTdApi = CThostFtdcTraderApi::CreateFtdcTraderApi();

	// 注册一事件处理的实例
	m_pTdApi->RegisterSpi(this);

    // 订阅公共流
	//        TERT_RESTART:从本交易日开始重传
	//        TERT_RESUME:从上次收到的续传
	//        TERT_QUICK:只传送登录后公共流的内容
	m_pTdApi->SubscribePublicTopic(THOST_TERT_QUICK);

    // 订阅私有流
	//        TERT_RESTART:从本交易日开始重传
	//        TERT_RESUME:从上次收到的续传
	//        TERT_QUICK:只传送登录后私有流的内容
	m_pTdApi->SubscribePrivateTopic(THOST_TERT_QUICK);
	
	// 设置交易托管系统服务的地址，可以注册多个地址备用
	for (list<string>::iterator it = account_info.TdAddress.begin(); it != account_info.TdAddress.end(); it++)
	{
		strcpy_s(qh_TDAddress, (*it).c_str());
		m_pTdApi->RegisterFront(qh_TDAddress);
	}

	// 使客户端开始与后台服务建立连接
	hEvent = CreateEvent(NULL, false, false, NULL);
	m_pTdApi->Init();
	if (WaitForSingleObject(hEvent, 30000) == WAIT_TIMEOUT)
		cout <<"***"+g_AccountInfo.AccountName+"***" << "交易前端登陆失败 该线程已退出" << endl;
	ConfirmSettleInfo();
}
void CFTTD::OnFrontConnected()
{
	if (bWannaLogin)
	{
		CThostFtdcReqUserLoginField reqUserLogin;
		memset(&reqUserLogin, 0, sizeof(reqUserLogin));
		strcpy_s(reqUserLogin.BrokerID, qh_BrokerID);//此处2011不要改
		strcpy_s(reqUserLogin.UserID, qh_UserID);//输入自己的帐号
		strcpy_s(reqUserLogin.Password, qh_Password);//输入密码
		int login = m_pTdApi->ReqUserLogin(&reqUserLogin, 1);//登录
	}
}
void CFTTD::OnFrontDisconnected (int nReason)
{
	WaitForSingleObject(hMutex, INFINITE);
    printf("***%s*** 交易连接断开 原因代码:%d\n",g_AccountInfo.AccountName.c_str(),nReason);
	ReleaseMutex(hMutex);
}

void CFTTD::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
	if (pRspInfo->ErrorID == 0)
		GetInstruments();
	else
		printf("***%s*** 交易前端登陆错误 ErrorID=%d ErrorMsg=%s 当前日期=%s\n",g_AccountInfo.AccountName.c_str(),pRspInfo->ErrorID,pRspInfo->ErrorMsg,pRspUserLogin->TradingDay);
}
void CFTTD::GetInstruments()
{
    //获得合约列表
	Instruments.clear();
	CThostFtdcQryInstrumentField qryField;
    memset(&qryField, 0, sizeof(qryField));
	Sleep(1000);
    int resCode = m_pTdApi-> ReqQryInstrument(&qryField, 0);
}

void CFTTD::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if ((pInstrument != NULL)
		&& ((strcmp(pInstrument->ExchangeID, "CFFEX") == 0)
		|| (strcmp(pInstrument->ExchangeID, "CZCE") == 0)
		|| (strcmp(pInstrument->ExchangeID, "SHFE") == 0)
		|| (strcmp(pInstrument->ExchangeID, "DCE") == 0))
		&& ((pInstrument->ProductClass == '1')))
	{
		CThostFtdcInstrumentField InstFld;
		memset(&InstFld, 0, sizeof(InstFld));
		memcpy(&InstFld, pInstrument, sizeof(InstFld));
		CFTTD::Instruments.push_back(InstFld);
		//g_pLog->printLog("名称：%s,交易所代码：%s \n", InstFld.InstrumentID, InstFld.ExchangeID);
	}
	if (bIsLast)
	{
		bIsgetInst = true;
		cout << "***" + g_AccountInfo.AccountName + "***" << "交易前端登陆成功" << endl;
		SetEvent(hEvent);
	}
    return;
}
void CFTTD::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pInvestorPosition != NULL)
	{
		//忽略组合合约
		string code = pInvestorPosition->InstrumentID;
		if (code.find("SPC") == code.npos)
		{
			CThostFtdcInvestorPositionField position_temp;
			position_temp = *pInvestorPosition;
			CurrentPosition.push_back(position_temp);
		}
	}
	if (bIsLast)
		SetEvent(hEvent);
	return;
}
void CFTTD::QueryAcct()
{
   CThostFtdcQryTradingAccountField qryTradingAccount;
   memset(&qryTradingAccount,0,sizeof(qryTradingAccount));
   strcpy_s(qryTradingAccount.BrokerID, qh_BrokerID);
   strcpy_s(qryTradingAccount.InvestorID, qh_UserID);
   ResetEvent(hEvent);
   Sleep(1000);
   int resCode = m_pTdApi->ReqQryTradingAccount(&qryTradingAccount, 0);
   WaitForSingleObject(hEvent,1000);
   return;
}
void CFTTD::PlaceOrder()
{
	bWannaDealMsg = true;
	if (orders.size() > 0)
	{
		order_count = 1;
		int max_batch = 0;
		for (map<int, OrderStruct>::iterator it = orders.begin(); it != orders.end(); it++)
		{
			int batch_temp = floor(it->first / 1000.);
			if (max_batch < batch_temp)
				max_batch = batch_temp;
		}
		WaitForSingleObject(hMutex, INFINITE);
		printf("***%s*** 下单进行中...\n", g_AccountInfo.AccountName.c_str());
		ReleaseMutex(hMutex);
		int total_seconds = 180;
		DWORD sleep_MilSecs = ceil(total_seconds / max_batch * 1000);
		for (int cur_batch = max_batch; cur_batch >0 ; cur_batch--)
		{
			Sleep(sleep_MilSecs);
			for (map<int, OrderStruct>::iterator it = orders.begin(); it != orders.end(); it++)
			{
				int batch_temp = floor(it->first / 1000);
				if (batch_temp == cur_batch)
				{
					OrderStruct order = it->second;
					SubmitOrder(order);
				}
			}
			WaitForSingleObject(hMutex, INFINITE);
			printf("***%s*** 第%d批单已经下完\n", g_AccountInfo.AccountName.c_str(), cur_batch);
			ReleaseMutex(hMutex);
		}
		//将主线程挂起，等待支线程交易处理结果
		while (TradeResult.size() < orders.size())
			Sleep(100);
		g_pLog->printLog("交易完毕 未成交部分如下:\n");
		string code;
		string kp;
		string mm;
		int volume;
		for (map<int, OrderStruct>::iterator it = TradeResult.begin(); it != TradeResult.end(); it++)
		{
			if (it->second.volume == 0)
				continue;
			else
			{
				if (it->second.direction == THOST_FTDC_D_Buy)
					mm = "买";
				else
					mm = "卖";
				if (it->second.kp[0] == THOST_FTDC_OFEN_CloseYesterday)
					kp = "平昨";
				else if (it->second.kp[0] == THOST_FTDC_OFEN_CloseToday)
					kp = "平今";
				else if (it->second.kp[0] == THOST_FTDC_OFEN_Open)
					kp = "开";
				else
					kp = "不知道";
				code = it->second.code;
				volume = it->second.volume;
				g_pLog->printLog("%s %s %s %d\n", code.c_str(), mm.c_str(), kp.c_str(), volume);
			}
		}
		g_pLog->printLog("\n");
	}
	return;
}

void CFTTD::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	int index = pInputOrder->RequestID;
	if (find(RequestIdDealed.begin(), RequestIdDealed.end(), index)==RequestIdDealed.end() && bWannaDealMsg)
	{
		int order_sn = int((int(pInputOrder->RequestID / 10)) / 1000);
		g_pLog->printLog("onRspOrderInsert errorID=%d errorMsg=%s 报单编号:%d \n", pRspInfo->ErrorID, pRspInfo->ErrorMsg, order_sn);
		OrderStruct order;
		memset(&order, 0, sizeof(order));
		strcpy_s(order.code, pInputOrder->InstrumentID);
		order.volume = pInputOrder->VolumeTotalOriginal;
		order.direction = pInputOrder->Direction;
		strcpy_s(order.kp, pInputOrder->CombOffsetFlag);
		TradeResult.insert({ index,order });
		RequestIdDealed.push_back(index);
		return;
	}
}

void CFTTD::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(pTradingAccount!=NULL)
    {
		g_pLog->printLog("账户信息:\n");
		g_pLog->printLog("入金金额:%.2f\n", pTradingAccount->Deposit);
		g_pLog->printLog("出金金额:%.2f\n", pTradingAccount->Withdraw);
		g_pLog->printLog("冻结的保证金:%.2f\n", pTradingAccount->FrozenMargin);
		g_pLog->printLog("冻结的资金:%.2f\n", pTradingAccount->FrozenCash);
		g_pLog->printLog("冻结的手续费:%.2f\n", pTradingAccount->FrozenCommission);
		g_pLog->printLog("当前保证金总额:%.2f\n", pTradingAccount->CurrMargin);
		g_pLog->printLog("资金差额:%.2f\n", pTradingAccount->CashIn);
		g_pLog->printLog("手续费:%.2f\n", pTradingAccount->Commission);
		g_pLog->printLog("平仓盈亏:%.2f\n", pTradingAccount->CloseProfit);
		g_pLog->printLog("持仓盈亏:%.2f\n", pTradingAccount->PositionProfit);
		g_pLog->printLog("期货结算准备金:%.2f\n", pTradingAccount->Balance);
		g_pLog->printLog("可用资金:%.2f\n", pTradingAccount->Available);
		g_pLog->printLog("可取资金:%.2f\n", pTradingAccount->WithdrawQuota);
		g_pLog->printLog("基本准备金:%.2f\n", pTradingAccount->Reserve);
		g_pLog->printLog("交易日:%s\n", pTradingAccount->TradingDay);
		g_pLog->printLog("质押金额:%.2f\n", pTradingAccount->Mortgage);
		g_pLog->printLog("交易所保证金:%.2f\n", pTradingAccount->ExchangeMargin);
    }
	SetEvent(hEvent);
    return;
}

//ErrRSP&Rtn/////////////////////////////////////////////////////////////////////
void CFTTD::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	g_pLog->printLog("onRspError errorID=%d errorMsg=%s\n", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
	return;
}
void CFTTD::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	return;
}
void CFTTD::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
	return;
}
//Rtn/////////////////////////////////////////////////////////////////////
void CFTTD::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    if(pOrder!=NULL && bWannaDealMsg)
    {
		int index = pOrder->RequestID;
		int retry_count = index % 10;
		int time_stamp_old = (int(index / 10)) % 1000;
		int order_sn = int((int(index / 10)) / 1000);
		int volumeRemained = pOrder->VolumeTotalOriginal - pOrder->VolumeTraded;
		//g_pLog->printLog("OnRtnOrder, 合约,%s; limit price%f，volume traded,%d; volume total,%d; 状态信息,%s; 报单状态,%c; 报单编号,%d; 尝试次数%d\n", pOrder->InstrumentID, pOrder->LimitPrice,pOrder->VolumeTraded, pOrder->VolumeTotalOriginal, pOrder->StatusMsg, pOrder->OrderStatus,order_sn,retry_count);
		g_pLog->printLog("OnRtnOrder, 合约,%s; volume traded,%d; volume total,%d; 状态信息,%s; 报单状态,%c; 报单编号,%d; 尝试次数%d\n", pOrder->InstrumentID, pOrder->VolumeTraded, pOrder->VolumeTotalOriginal, pOrder->StatusMsg, pOrder->OrderStatus, order_sn, retry_count);

		if (pOrder->OrderStatus == THOST_FTDC_OST_AllTraded)
		{
			if (find(RequestIdDealed.begin(), RequestIdDealed.end(), index) == RequestIdDealed.end())
			{
				OrderStruct order;
				memset(&order, 0, sizeof(order));
				strcpy_s(order.code, pOrder->InstrumentID);
				order.volume = 0;
				TradeResult.insert({ index,order });
				RequestIdDealed.push_back(index);
			}
		}
		else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled && retry_count == RETRY_TIMES)
		{
			if (find(RequestIdDealed.begin(), RequestIdDealed.end(), index) == RequestIdDealed.end())
			{
				OrderStruct order;
				memset(&order, 0, sizeof(order));
				strcpy_s(order.code, pOrder->InstrumentID);
				order.volume = volumeRemained;
				order.direction = pOrder->Direction;
				strcpy_s(order.kp, pOrder->CombOffsetFlag);
				TradeResult.insert({ index,order });
				RequestIdDealed.push_back(index);
			}
		}
		if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled && volumeRemained > 0 && retry_count < RETRY_TIMES)
		{
			if (find(RequestIdDealed.begin(), RequestIdDealed.end(), index) == RequestIdDealed.end())
			{
				CThostFtdcInputOrderField newInputOrder;
				CThostFtdcInputOrderField* pNewInputOrder = &newInputOrder;
				memset(pNewInputOrder, 0, sizeof(newInputOrder));
				strcpy_s(pNewInputOrder->BrokerID, qh_BrokerID);
				strcpy_s(pNewInputOrder->InvestorID, qh_UserID);
				strcpy_s(pNewInputOrder->InstrumentID, pOrder->InstrumentID);
				pNewInputOrder->VolumeTotalOriginal = volumeRemained;
				pNewInputOrder->OrderPriceType = THOST_FTDC_OPT_LimitPrice;
				OrderStruct order;
				memset(&order, 0, sizeof(order));
				strcpy_s(order.code, pOrder->InstrumentID);
				order.direction = pOrder->Direction;
				double price = ((CFTMD*)g_pMdHandler)->GetTradePrice(order);
				pNewInputOrder->LimitPrice = price;
				pNewInputOrder->TimeCondition = THOST_FTDC_TC_IOC;
				pNewInputOrder->Direction = pOrder->Direction;
				strcpy_s(pNewInputOrder->CombOffsetFlag, pOrder->CombOffsetFlag);
				pNewInputOrder->ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
				strcpy_s(pNewInputOrder->CombHedgeFlag, "1");
				pNewInputOrder->VolumeCondition = THOST_FTDC_VC_AV; //成交量类型
				pNewInputOrder->ContingentCondition = THOST_FTDC_CC_Immediately; //触发条件
				if (order_count % 6 == 0 && order_count > 0)
					Sleep(1000);
				clock_t timer;
				timer = clock();
				int time_stamp_new = int(timer / 1000);
				if (time_stamp_new >= time_stamp_old + 3)
				{
					pNewInputOrder->RequestID = (order_count*1000 + time_stamp_new) * 10 + retry_count + 1;
					m_pTdApi->ReqOrderInsert(pNewInputOrder, 1);
					RequestIdDealed.push_back(index);
				}
				else
				{
					Sleep(3000);
					timer = clock();
					time_stamp_new = int(timer / 1000);
					pNewInputOrder->RequestID = (order_count * 1000 + time_stamp_new) * 10 + retry_count + 1;
					m_pTdApi->ReqOrderInsert(pNewInputOrder, 1);
					RequestIdDealed.push_back(index);
				}
				order_count++;
			}
		}
    }
	return;
}
void CFTTD::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
}

void CFTTD::GetTargetPosition()
{
	//打开目标仓位文档
	string path = GetExePath();
	string head = path + "目标持仓\\" + g_AccountInfo.PositionFileHead;
	string tail = g_AccountInfo.PositionFileTail;
	time_t now;
	time(&now);
	tm  now_tm;
	localtime_s(&now_tm, &now);
	char  s_date[10];
	strftime(s_date, sizeof(s_date), "%Y%m%d", &now_tm);
	string str_date = s_date;
	string fn = head + str_date + tail;
	ifstream in_file(fn);
	printf("***%s*** 准备读入目标仓位 请将目标仓位文档放入指定的文件夹 并确保其名称格式正确\n",g_AccountInfo.AccountName.c_str());
	clock_t start = clock();
	while (!(in_file.good() && in_file.is_open()))
	{
		clock_t end = clock();

		if((((end - start)/CLOCKS_PER_SEC)%10) == 0 && ((end - start) / CLOCKS_PER_SEC)>0)
			printf("***%s*** 准备读入目标仓位 请将目标仓位文档放入指定的文件夹 并确保其名称格式正确\n", g_AccountInfo.AccountName.c_str());
		Sleep(1000);
		in_file.open(fn);
	}
	g_pLog->printLog("成功打开目标仓位文档:%s\n",fn.c_str());
	//读取目标仓位
	string s;
	string code_raw;
	int volume = 0;
	while (getline(in_file, s))
	{
		std::replace(s.begin(), s.end(), ',', ' ');
		stringstream line(s);
		line >> code_raw;
		line >> volume;
		int volume_temp = volume;
		if ((volume_temp)==0)
			continue;
		string code = GetTradeCode(code_raw);
		TargetPosition.insert({ code,volume_temp });
		s.clear();
		volume = 0;
	}
	g_pLog->printLog("目标仓位:\n");
	WaitForSingleObject(hMutex, INFINITE);
	printf("***%s&*** 获取目标仓位成功 目标仓位如下:\n",g_AccountInfo.AccountName.c_str());
	for (map<string, int>::iterator it = TargetPosition.begin(); it != TargetPosition.end(); it++)
	{
		g_pLog->printLog("%s %d\n", it->first.c_str(), it->second);
		printf("%s %d\n", it->first.c_str(), it->second);
	}
	g_pLog->printLog("\n");
	printf("\n");
	ReleaseMutex(hMutex);
}
void CFTTD::GetOrders()
{
	set<string> codes;
	for (list<CThostFtdcInvestorPositionField>::iterator it = CurrentPosition.begin(); it != CurrentPosition.end(); it++)
	{
		string code = it->InstrumentID;
		codes.insert(code);
	}
	for (map<string,int>::iterator it = TargetPosition.begin(); it != TargetPosition.end(); it++)
		codes.insert(it->first);

	int index = 0;
	int batch_count = 1;
	int order_count = 1;
	for (set<string>::iterator it_1 = codes.begin(); it_1 != codes.end(); it_1++)
	{
		string code = *it_1;
		int holding_long_yd = 0, holding_long_td = 0, holding_short_yd = 0, holding_short_td = 0;
		//获取合约代码code的当前持仓结构，包括昨多仓，今多仓，昨空仓，今空仓
		for (list<CThostFtdcInvestorPositionField>::iterator it_1 = CurrentPosition.begin(); it_1 != CurrentPosition.end(); it_1++)
		{
			if (strcmp(code.c_str(), it_1->InstrumentID) != 0)
				continue;
			else
			{
				if (it_1->PosiDirection == THOST_FTDC_PD_Long)
				{
					if (it_1->Position >= it_1->YdPosition)
					{
						holding_long_yd = holding_long_yd + it_1->YdPosition;
						holding_long_td = holding_long_td + (it_1->Position - it_1->YdPosition);
					}
					else
					{
						holding_long_yd = holding_long_yd + it_1->Position;
						holding_long_td = holding_long_td + 0;
					}
				}
				else if (it_1->PosiDirection == THOST_FTDC_PD_Short)
				{
					if (it_1->Position > it_1->YdPosition)
					{
						holding_short_yd = holding_short_yd + it_1->YdPosition;
						holding_short_td = holding_short_td + (it_1->Position - it_1->YdPosition);
					}
					else
					{
						holding_short_yd = holding_short_yd + it_1->Position;
						holding_short_td = holding_short_td + 0;
					}
				}
				else
				{
					printf("***%s*** CFTTD::GetOrders 当前持仓中%s持仓方向的值错误 该线程已退出\n",g_AccountInfo.AccountName.c_str(),code.c_str());
					ExitThread(0);
				}
			}
		}
		//根据目标仓位和现仓位计算交易指令
		int holding_net = holding_long_td + holding_long_yd - holding_short_td - holding_short_yd;
		int holding_tar = 0;
		int volume_remained = 0;
		int vol_bench = max(floor(((CFTMD*)g_pMdHandler)->GetVolume(code)*BIG_ORDER_FACTOR), 1);
		if (TargetPosition.find(code) != TargetPosition.end())
			holding_tar = TargetPosition[code];
		int delta = holding_tar - holding_net;
		if (delta == 0)
			continue;
		else if (delta > 0)
		{
			if (delta <= holding_short_yd)
			{
				//拆单，全部都是平昨仓
				batch_count = 1;
				volume_remained = delta;
				while (volume_remained > 0)
				{
					if (volume_remained <= vol_bench)
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = volume_remained;
						order_temp.direction = THOST_FTDC_DEN_Buy;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = 0;
					}
					else
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = vol_bench;
						order_temp.direction = THOST_FTDC_DEN_Buy;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = volume_remained - vol_bench;
					}
					order_count++;
					batch_count++;
				}
			}
			else if (delta > holding_short_yd && delta <= (holding_short_yd + holding_short_td))
			{
				batch_count = 1;
				if (holding_short_yd > 0)
				{
					//对平昨仓部分拆单
					volume_remained = holding_short_yd;
					while (volume_remained > 0)
					{
						if (volume_remained <= vol_bench)
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = volume_remained;
							order_temp.direction = THOST_FTDC_DEN_Buy;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = 0;
						}
						else
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = vol_bench;
							order_temp.direction = THOST_FTDC_DEN_Buy;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = volume_remained - vol_bench;
						}
						order_count++;
						batch_count++;
					}
				}
				//对平今仓部分拆单
				volume_remained = delta - holding_short_yd;
				while (volume_remained > 0)
				{
					if (volume_remained <= vol_bench)
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = volume_remained;
						order_temp.direction = THOST_FTDC_DEN_Buy;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = 0;
					}
					else
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = vol_bench;
						order_temp.direction = THOST_FTDC_DEN_Buy;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = volume_remained - vol_bench;
					}
					order_count++;
					batch_count++;
				}
			}
			else
			{
				batch_count = 1;
				//对平昨仓部分拆单
				if (holding_short_yd > 0)
				{
					volume_remained = holding_short_yd;
					while (volume_remained > 0)
					{
						if (volume_remained <= vol_bench)
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = volume_remained;
							order_temp.direction = THOST_FTDC_DEN_Buy;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = 0;
						}
						else
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = vol_bench;
							order_temp.direction = THOST_FTDC_DEN_Buy;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = volume_remained - vol_bench;
						}
						order_count++;
						batch_count++;
					}
				}
				//对平今仓部分拆单
				if (holding_short_td > 0)
				{
					volume_remained = holding_short_td;
					while (volume_remained > 0)
					{
						if (volume_remained <= vol_bench)
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = volume_remained;
							order_temp.direction = THOST_FTDC_DEN_Buy;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = 0;
						}
						else
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = vol_bench;
							order_temp.direction = THOST_FTDC_DEN_Buy;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = volume_remained - vol_bench;
						}
						order_count++;
						batch_count++;
					}
				}
				//对开仓部分拆单
				volume_remained = delta - holding_short_yd - holding_short_td;
				while (volume_remained > 0)
				{
					if (volume_remained <= vol_bench)
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = volume_remained;
						order_temp.direction = THOST_FTDC_DEN_Buy;
						order_temp.kp[0] = THOST_FTDC_OFEN_Open;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = 0;
					}
					else
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = vol_bench;
						order_temp.direction = THOST_FTDC_DEN_Buy;
						order_temp.kp[0] = THOST_FTDC_OFEN_Open;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = volume_remained - vol_bench;
					}
					order_count++;
					batch_count++;
				}
			}
		}
		else
		{
			if (abs(delta) <= holding_long_yd)
			{
				//对于平昨仓部分进行拆单
				batch_count = 1;
				volume_remained = abs(delta);
				while (volume_remained > 0)
				{
					if (volume_remained <= vol_bench)
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = volume_remained;
						order_temp.direction = THOST_FTDC_DEN_Sell;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = 0;
					}
					else
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = vol_bench;
						order_temp.direction = THOST_FTDC_DEN_Sell;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = volume_remained - vol_bench;
					}
					order_count++;
					batch_count++;
				}
			}
			else if (abs(delta) > holding_long_yd && abs(delta) <= (holding_long_yd + holding_long_td))
			{
				//对平昨仓部分进行拆单
				batch_count = 1;
				if (holding_long_yd > 0)
				{
					volume_remained = holding_long_yd;
					while (volume_remained > 0)
					{
						if (volume_remained <= vol_bench)
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = volume_remained;
							order_temp.direction = THOST_FTDC_DEN_Sell;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = 0;
						}
						else
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = vol_bench;
							order_temp.direction = THOST_FTDC_DEN_Sell;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = volume_remained - vol_bench;
						}
						order_count++;
						batch_count++;
					}
				}
				//对平今仓部分进行拆单
				volume_remained = abs(delta)-holding_long_yd;
				while (volume_remained > 0)
				{
					if (volume_remained <= vol_bench)
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = volume_remained;
						order_temp.direction = THOST_FTDC_DEN_Sell;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = 0;
					}
					else
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = vol_bench;
						order_temp.direction = THOST_FTDC_DEN_Sell;
						order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = volume_remained - vol_bench;
					}
					order_count++;
					batch_count++;
				}
			}
			else
			{
				//对平昨仓部分进行拆单
				batch_count = 1;
				if (holding_long_yd > 0)
				{
					volume_remained = holding_long_yd;
					while (volume_remained > 0)
					{
						if (volume_remained <= vol_bench)
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = volume_remained;
							order_temp.direction = THOST_FTDC_DEN_Sell;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = 0;
						}
						else
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = vol_bench;
							order_temp.direction = THOST_FTDC_DEN_Sell;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseYesterday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = volume_remained - vol_bench;
						}
						order_count++;
						batch_count++;
					}
				}
				//对平今仓部分进行拆单
				if (holding_long_td > 0)
				{
					volume_remained = holding_long_td;
					while (volume_remained > 0)
					{
						if (volume_remained <= vol_bench)
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = volume_remained;
							order_temp.direction = THOST_FTDC_DEN_Sell;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = 0;
						}
						else
						{
							OrderStruct order_temp;
							memset(&order_temp, 0, sizeof(order_temp));
							strcpy_s(order_temp.code, code.c_str());
							order_temp.volume = vol_bench;
							order_temp.direction = THOST_FTDC_DEN_Sell;
							order_temp.kp[0] = THOST_FTDC_OFEN_CloseToday;
							index = batch_count * 1000 + order_count;
							orders.insert({ index,order_temp });
							volume_remained = volume_remained - vol_bench;
						}
						order_count++;
						batch_count++;
					}
				}
				//对开仓部分进行拆单
				volume_remained = abs(delta) - holding_long_yd - holding_long_td;
				while (volume_remained > 0)
				{
					if (volume_remained <= vol_bench)
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = volume_remained;
						order_temp.direction = THOST_FTDC_DEN_Sell;
						order_temp.kp[0] = THOST_FTDC_OFEN_Open;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = 0;
					}
					else
					{
						OrderStruct order_temp;
						memset(&order_temp, 0, sizeof(order_temp));
						strcpy_s(order_temp.code, code.c_str());
						order_temp.volume = vol_bench;
						order_temp.direction = THOST_FTDC_DEN_Sell;
						order_temp.kp[0] = THOST_FTDC_OFEN_Open;
						index = batch_count * 1000 + order_count;
						orders.insert({ index,order_temp });
						volume_remained = volume_remained - vol_bench;
					}
					order_count++;
					batch_count++;
				}
			}
		}
	}	
	g_pLog->printLog("orders列表如下:\n");
	WaitForSingleObject(hMutex, INFINITE);
	printf("***%s*** orders列表如下:\n",g_AccountInfo.AccountName.c_str());
	string code;
	string kp;
	string mm;
	int volume;
	for (map<int, OrderStruct>::iterator it = orders.begin(); it != orders.end(); it++)
	{
		if (it->second.volume == 0)
			continue;
		else
		{
			if (it->second.direction == THOST_FTDC_D_Buy)
				mm = "买";
			else
				mm = "卖";
			if (it->second.kp[0] == THOST_FTDC_OFEN_CloseYesterday)
				kp = "平昨";
			else if (it->second.kp[0] == THOST_FTDC_OFEN_CloseToday)
				kp = "平今";
			else if (it->second.kp[0] == THOST_FTDC_OFEN_Open)
				kp = "开";
			else
				kp = "不知道";
			code = it->second.code;
			volume = it->second.volume;
			g_pLog->printLog("%s %s %s %d order编号:%d\n", code.c_str(), mm.c_str(), kp.c_str(), volume,it->first);
			printf("%s %s %s %d order编号:%d\n", code.c_str(), mm.c_str(), kp.c_str(), volume, it->first);
		}
	}
	g_pLog->printLog("\n");
	printf("\n");
	ReleaseMutex(hMutex);
}
void CFTTD::GetCurrentPosition()
{
	CThostFtdcQryInvestorPositionField QryPosFd;
	memset(&QryPosFd, 0,sizeof(QryPosFd));
	CThostFtdcQryInvestorPositionField * pQryPos = &QryPosFd;
	strcpy_s(pQryPos->BrokerID, qh_BrokerID);
	strcpy_s(pQryPos->InvestorID, qh_UserID);
	strcpy_s(pQryPos->InstrumentID, "");
	CurrentPosition.clear();
	ResetEvent(hEvent);
	Sleep(1000);
	m_pTdApi->ReqQryInvestorPosition(pQryPos, 0);
	if (WaitForSingleObject(hEvent, 10000) == WAIT_OBJECT_0)
	{
		map<string, int> current_position;
		for (list<CThostFtdcInvestorPositionField>::iterator it = CurrentPosition.begin(); it != CurrentPosition.end(); it++)
		{
			
			string code = (*it).InstrumentID;
			int position = 0;
			if ((*it).PosiDirection == THOST_FTDC_PD_Long)
				position = (*it).Position;
			else
				position = (*it).Position*(-1);
			if (current_position.find(code) != current_position.end())
				current_position[code] = current_position[code] + position;
			else
				current_position[code] = position;
		}
		g_pLog->printLog("获得当前仓位成功 当前持仓如下:\n");
		WaitForSingleObject(hMutex, INFINITE);
		printf("***%s*** 获取当前仓位成功 当前仓位如下:\n",g_AccountInfo.AccountName.c_str());
		for (map<string,int>::iterator it = current_position.begin(); it != current_position.end(); it++)
		{
			if (it->second == 0)
				continue;
			else
			{
				g_pLog->printLog("%s %d\n", it->first.c_str(), it->second);
				printf("%s %d\n", it->first.c_str(), it->second);
			}
		}
		g_pLog->printLog("\n");
		printf("\n");
		ReleaseMutex(hMutex);
	}
	else
	{
		printf("***%s*** CFTTD::GetCurrentPosition 获取当前仓位失败\n",g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
}

string CFTTD::GetTradeCode(string code_raw)
{
	std::transform(code_raw.begin(), code_raw.end(),code_raw.begin(), ::toupper);
	size_t index = code_raw.find_last_not_of("0123456789");
	string head_u = code_raw.substr(0, index + 1);
	string head_l = head_u;
	std::transform(head_l.begin(), head_l.end(), head_l.begin(), ::tolower);
	string tail_4;
	if ((code_raw.substr(index + 1)).length() == 4)
		tail_4 = code_raw.substr(index + 1);
	else
		tail_4 = '1' + code_raw.substr(index + 1);
	string tail_3 = tail_4.substr(1);

	list<string> InstrumentCodes;
	for (list<CThostFtdcInstrumentField>::iterator it = Instruments.begin(); it != Instruments.end(); it++)
	{
		string code = it->InstrumentID;
		InstrumentCodes.push_back(code);
	}
	if (std::find(InstrumentCodes.begin(), InstrumentCodes.end(), head_u + tail_3) != InstrumentCodes.end())
		return head_u + tail_3;
	else if (std::find(InstrumentCodes.begin(), InstrumentCodes.end(), head_u + tail_4) != InstrumentCodes.end())
		return head_u + tail_4;
	else if (std::find(InstrumentCodes.begin(), InstrumentCodes.end(), head_l + tail_3) != InstrumentCodes.end())
		return head_l + tail_3;
	else if (std::find(InstrumentCodes.begin(), InstrumentCodes.end(), head_l + tail_4) != InstrumentCodes.end())
		return head_l + tail_4;
	else
	{
		printf("****%s*** CFTTD::GetTradeCode 找不到合约%s 该线程已退出\n",g_AccountInfo.AccountName.c_str(),code_raw.c_str());
		ExitThread(0);
	}
}

void CFTTD::ConfirmSettleInfo()
{
	CThostFtdcSettlementInfoConfirmField SettleInfoConfirm;
	memset(&SettleInfoConfirm, 0, sizeof(SettleInfoConfirm));
	CThostFtdcSettlementInfoConfirmField * pSettleInfoConfirm = &SettleInfoConfirm;
	strcpy_s(pSettleInfoConfirm->BrokerID, qh_BrokerID);
	strcpy_s(pSettleInfoConfirm->InvestorID, qh_UserID);
	time_t now;
	time(&now);
	struct tm now_tm;
	struct tm *today = &now_tm;
	localtime_s(today,&now);
	strftime(SettleInfoConfirm.ConfirmDate, 9, "%Y%m%d", today);
	strftime(SettleInfoConfirm.ConfirmTime, 9, "%H%M%S", today);
	ResetEvent(hEvent);
	m_pTdApi->ReqSettlementInfoConfirm(pSettleInfoConfirm, 1);
	if (WaitForSingleObject(hEvent, 10000) == WAIT_OBJECT_0)
	{
		printf("***%s*** 确认结算结果完毕\n",g_AccountInfo.AccountName.c_str());
	}
	else
	{
		printf("***%s*** 确认结算结果失败 该线程已退出\n",g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
}

void CFTTD::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	SetEvent(hEvent);
}
void CFTTD::SubmitOrder(OrderStruct order)
{
	CThostFtdcInputOrderField InputOrder;
	memset(&InputOrder,0, sizeof(InputOrder));
	CThostFtdcInputOrderField* pInputOrder = &InputOrder;
	strcpy_s(pInputOrder->BrokerID, qh_BrokerID);
	strcpy_s(pInputOrder->InvestorID, qh_UserID);
	strcpy_s(pInputOrder->InstrumentID, order.code);
	pInputOrder->Direction = order.direction;
	pInputOrder->VolumeTotalOriginal = order.volume;
	strcpy_s(pInputOrder->CombOffsetFlag, order.kp);
	double price = ((CFTMD*)g_pMdHandler)->GetTradePrice(order);
	pInputOrder->LimitPrice = price;
	pInputOrder->OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	pInputOrder->TimeCondition = THOST_FTDC_TC_IOC;
	pInputOrder->VolumeTotalOriginal = order.volume;
	pInputOrder->ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	pInputOrder->VolumeCondition = THOST_FTDC_VC_AV;
	pInputOrder->MinVolume = 1;
	pInputOrder->IsAutoSuspend = 0;
	pInputOrder->UserForceClose = 0;
	strcpy_s(pInputOrder->CombHedgeFlag, "1");
	pInputOrder->ContingentCondition = THOST_FTDC_CC_Immediately;
	if (order_count % 6 == 0 && order_count>0)
		Sleep(1000);
	clock_t timer = clock();
	int time_stamp = timer / 1000;
	pInputOrder->RequestID =(order_count*1000+time_stamp) * 10 + 1;
	m_pTdApi->ReqOrderInsert(pInputOrder, 1);
	order_count++;
}
void CFTTD::AccountLogout()
{
	CThostFtdcUserLogoutField UserLogoutField;
	memset(&UserLogoutField, 0, sizeof(UserLogoutField));
	CThostFtdcUserLogoutField * pUserLogoutField = &UserLogoutField;
	strcpy_s(pUserLogoutField->BrokerID, g_AccountInfo.BrokerID.c_str());
	strcpy_s(pUserLogoutField->UserID, g_AccountInfo.UserID.c_str());
	ResetEvent(hEvent);
	bWannaLogin = false;
	m_pTdApi->ReqUserLogout(pUserLogoutField, 1);
	if (WaitForSingleObject(hEvent, 10000) == WAIT_TIMEOUT)
	{
		g_pLog->printLog("交易前端登出失败\n");
		printf("***%s*** 交易前端登出失败 该线程已退出\n", g_AccountInfo.AccountName.c_str());
		ExitThread(0);
	}
	else
	{
		g_pLog->printLog("交易前端登出\n");
		printf("***%s*** 交易前端登出\n", g_AccountInfo.AccountName.c_str());
	}
}

void CFTTD::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo->ErrorID == 0)
		SetEvent(hEvent);
}