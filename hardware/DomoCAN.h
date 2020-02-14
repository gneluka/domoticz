#pragma once

#include "DomoticzHardware.h"

class DomoCAN : public CDomoticzHardwareBase
{
public:
	enum _eDCANType
	{
		DCANTYPE_UNKNOWN = 0,
		DCANTYPE_COIN_SENDER_05,
		DCANTYPE_COIN_SENDER_1,
		DCANTYPE_COIN_SENDER_2,
		DCANTYPE_COIN_SENDER_3,
		DCANTYPE_COIN_SENDER_4,
		DCANTYPE_COIN_SENDER_5,
		DCANTYPE_COIN_SENDER_6,
		DCANTYPE_COIN_COUNTER,
		DCANTYPE_COIN_COUNTER_TOTAL,
		DCANTYPE_ACTIVE_MON,
		DCANTYPE_RELAY_MODULE_5
	};
	explicit DomoCAN(const int ID, const std::string &Address, const std::string &SerialPort);
	~DomoCAN();
	bool WriteToHardware(const char *pdata, const unsigned char length) override;

	void AddDevice(const int ID, const std::string &Name, const int DomoCANDevType, const int DomoCANID);
	void RemoveDevice(const int ID, const int DomoCANDevType, const int DomoCANID);
	
	void AddNode(const std::string &Name, const int DomoCANDevType, const int DomoCANID);
	bool UpdateNode(const int ID, const std::string &Name, const int DomoCANDevType, const int NodeID);
	void RemoveNode(const int ID);
	void RemoveAllNodes();

private:
	bool StartHardware() override;
	bool StopHardware() override;

	void Do_Work();

	int SocketCAN_Open(const char *SocketCANIfName);

private:
	std::shared_ptr<std::thread> m_thread;

	_eDCANType m_dev_type;
	uint8_t m_dcan_id;
	std::string m_dcan_ifname;


};

