// KC_Trader_001.cpp : Defines the entry point for the console application.
//

#include "StdAfx.h"
using namespace std;
HANDLE hMutex = CreateMutex(NULL, false, NULL);
string GetExePath()
{
	char path[MAX_PATH];
	wchar_t path_w[MAX_PATH];
	GetModuleFileName(NULL, path_w, sizeof(path_w));
	(_tcsrchr(path_w, _T('\\')))[1] = 0;
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, path_w, -1, path, MAX_PATH, NULL, NULL);
	string path_str = path;
	return path_str;
}

AccountInfo GetAccountInfo(string account_name)
{
	string path = GetExePath();
	string fn_plus = path + account_name + "/账号信息.txt";
	ifstream in_file(fn_plus);
	string s;
	AccountInfo account_info;
	while (!in_file.is_open())
	{
		cout << "读取***" + account_name + "***账号信息文件失败 请确保该文件存在并且文件名正确" << endl;
		Sleep(10000);
		in_file.open(fn_plus);
	}
	int item_count = 0;
	bool bIsGetTdAddress = false;
	bool bIsGetMdAddress = false;
	while (getline(in_file, s))
	{
		stringstream line(s);
		string name;
		line >> name;
		string info;
		line >> info;
		if (strcmp(name.c_str(), "交易前端") == 0)
		{
			account_info.TdAddress.push_back(info);
			bIsGetTdAddress = true;
		}
		else if (strcmp(name.c_str(), "行情前端") == 0)
		{
			account_info.MdAddress.push_back(info);
			bIsGetMdAddress = true;
		}
		else if (strcmp(name.c_str(), "经纪商代码") == 0)
		{
			account_info.BrokerID = info;
			item_count++;
		}
		else if (strcmp(name.c_str(), "账号") == 0)
		{
			account_info.UserID = info;
			item_count++;
		}
		else if (strcmp(name.c_str(), "密码") == 0)
		{
			account_info.Password = info;
			item_count++;
		}
		else if (strcmp(name.c_str(), "order文档名头部") == 0)
		{
			account_info.PositionFileHead = info;
			item_count++;
		}
		else if (strcmp(name.c_str(), "order文档名尾部") == 0)
		{
			account_info.PositionFileTail = info;
			item_count++;
		}
		else if (strcmp(name.c_str(), "账号名称") == 0)
		{
			account_info.AccountName = info;
			item_count++;
		}
		else
		{
			;
		}
	}
	if (bIsGetTdAddress && bIsGetMdAddress)
	{
		item_count++;
		item_count++;
	}
	if (item_count < 8)
	{
		cout << "***" + account_name + "*** 账号信息文件格式错误 该线程已退出" << endl;
		ExitThread(0);
	}
	return account_info;
}

DWORD WINAPI TradeAccount(LPVOID lpParameter)
{
	string account_name = (*(string *)lpParameter);
	AccountInfo account_info = GetAccountInfo(account_name);
	CFTTD * pTdHandler = new CFTTD();
	CFTMD * pMdHandler = new CFTMD();
	logInfo log;
	logInfo * pLog = &log;
	string path = GetExePath();
	string log_path = path + account_info.AccountName + "\\logs";
	pLog->SetLogPath(log_path.c_str());
	pTdHandler->Init(account_info, pMdHandler, pLog);
	pMdHandler->Init(account_info, pTdHandler);
	pTdHandler->GetTargetPosition();
	//pMdHandler->test();
	clock_t start = clock();
	pTdHandler->GetCurrentPosition();
	pTdHandler->GetOrders();
	pTdHandler->PlaceOrder();
	pTdHandler->GetCurrentPosition();
	clock_t end = clock();
	WaitForSingleObject(hMutex, INFINITE);
	pTdHandler->g_pLog->printLog("交易完成 耗时%d秒\n", (end - start) / CLOCKS_PER_SEC);
	printf("***%s*** 交易完成 耗时%d秒\n", account_info.AccountName.c_str(), (end - start) / CLOCKS_PER_SEC);
	ReleaseMutex(hMutex);
	return 0;
}
int main(int argc, char* argv[], char *envp[])
{
	//获取账号列表
	list<string> accounts;
	string path = GetExePath();
	string accounts_file_fn = path + "账号列表.txt";
	ifstream in_file(accounts_file_fn);
	if (in_file.good() && in_file.is_open())
	{
		string s;
		while (getline(in_file, s))
		{
			stringstream line(s);
			string account_s;
			line >> account_s;
			accounts.push_back(account_s);
		}
		cout << "账号列表如下" << endl;
		for (list<string>::iterator it = accounts.begin(); it != accounts.end(); it++)
			cout << *it << endl;
		cout << endl;
	}
	else
	{
		cout << "获取账号列表失败" << endl;
		exit(-1);
	}
	//输入管理员口令 登录CTP 做交易
	cout << "请输入本公司交易软件管理员口令" << endl;
	string s;
	cin >> s;
	while (strcmp(s.c_str(), "232461") != 0)
	{
		cout << "口令错误 请重新输入" << endl;
		cin >> s;
	}
	cout << "口令正确 开始登陆CTP" << endl;
	list<HANDLE> hThreads;
	for (list<string>::iterator it = accounts.begin(); it != accounts.end(); it++)
	{
		HANDLE hThread = CreateThread(NULL, 0, TradeAccount, &(*it), 0, NULL);
		hThreads.push_back(hThread);
	}
	//等待所有交易线程结束
	for (list<HANDLE>::iterator it = hThreads.begin(); it != hThreads.end(); it++)
		WaitForSingleObject(*it, INFINITE);

	return 0;
}



