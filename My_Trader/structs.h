#pragma once
#include "stdafx.h"
struct OrderStruct
{
	TThostFtdcInstrumentIDType code;
	TThostFtdcVolumeType volume;
	TThostFtdcDirectionType direction;
	TThostFtdcCombOffsetFlagType kp;
};
struct AccountInfo
{
	std::list<std::string>  TdAddress;
	std::list<std::string>  MdAddress;
	std::string  UserID;
	std::string  Password;
	std::string  BrokerID;
	std::string  PositionFileHead;
	std::string  PositionFileTail;
	std::string  AccountName;
};