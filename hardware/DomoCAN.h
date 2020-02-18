#pragma once

#include "DomoticzHardware.h"

#include <stdint.h>

#define DST_NODE_ID_MASK 0x7F
#define SRC_NODE_ID_MASK (0x7F << 7)
#define FUNCTION_MASK (0x3F << 16)
#define PARAMETER_MASK (0x7F << 22)
#define ERROR_FLAG_MASK (0x1 << 29)
#define RTR_FLAG_MASK (0x1 << 30)
#define EFF_FLAG_MASK (0x1 << 31)

enum FUNCTION_CODE
{
  FCN_RTC = 0,
  FCN_SET_RTC = 1,
  FCN_COIN_EVENT = 2,
  FCN_ACTIVE_EVENT = 3,
  FCN_ACTIVE_STATUS = 4,
  FCN_NODE_PING = 5,
  FCN_NODE_ACK = 6,
  FCN_FW_VER = 7,
  FCN_NODE_UDID = 8,
  FCN_SET_NODE_ID =9,
  FCN_FWT_START =10,
  FCN_FWT_END =11,
  FCN_FWT_DWORD =12,
  FCN_RESET =13
};


enum COIN_EVENT_CODE
{
  EVENT_COIN_50_CENTS = 1,
  EVENT_COIN_1_EURO = 2,
  EVENT_COIN_2_EURO = 4,
  EVENT_COIN_3_EURO = 6,
  EVENT_COIN_4_EURO = 8,
  EVENT_COIN_5_EURO = 10,
  EVENT_COIN_6_EURO = 12,
};

enum ACTIVE_EVENT_CODE
{
  EVENT_INACTIVE = 0,
  EVENT_ACTIVE = 1,
};


typedef struct 
{
  uint32_t DST_node_id       : 7;
  uint32_t SRC_node_id       : 7;
  uint32_t first_packet_flag : 1;
  uint32_t last_packet_flag  : 1;
  uint32_t function          : 6;
  uint32_t parameter         : 7;
  uint32_t error_flag 	     : 1;
  uint32_t rtr_flag	         : 1;
  uint32_t eff_flag          : 1;
}CANeidBFTypeDef;

typedef union
{
  uint32_t eid;
  CANeidBFTypeDef eid_bf;
}CanEidTypeDef;

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

	int SocketCAN_Open();
	bool SendCoin(const char coin, const char DomoCANID);
	void Process_RX_EVENT(CanEidTypeDef* eid, uint8_t* data);

private:
	std::shared_ptr<std::thread> m_thread;

	_eDCANType m_dev_type;
	uint8_t m_dcan_id;
	std::string m_dcan_ifname;


};

