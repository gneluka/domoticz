/*
This code supports any socketCAN device.

Authors:
- Guillermo Estevez (gestevezluka@gmail.com)
*/

#include "stdafx.h"
#include "DomoCAN.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>

#include "../json/json.h"
#include "../main/HTMLSanitizer.h"

#ifdef HAVE_LINUX_I2C
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include "../main/Helper.h"
#include "../main/Logger.h"
#include "hardwaretypes.h"
#include "../main/RFXtrx.h"
#include "../main/localtime_r.h"
#include "../main/mainworker.h"
#include "../main/SQLHelper.h"
#include "../main/WebServer.h"

#define round(a) ( int ) ( a + .5 )

#define DCAN_READ_INTERVAL 1



const char* szDCANTypeNames[] = {
	"DomoCAN_Unknown",
	"DomoCAN_CoinSender_05",
	"DomoCAN_CoinSender_1",
	"DomoCAN_CoinSender_2",
	"DomoCAN_CoinSender_3",
	"DomoCAN_CoinSender_4",
	"DomoCAN_CoinSender_5",
	"DomoCAN_CoinSender_6",
	"DomoCAN_CoinCounter",
	"DomoCAN_CoinCounter_Totals",
	"DomoCAN_ActiveMonitor",
	"DomoCAN_RelayModule5Channels",
};

DomoCAN::DomoCAN(const int ID, const std::string &Address, const std::string &SerialPort):
	m_dcan_id((uint8_t)atoi(Address.c_str())),
	m_dcan_ifname(SerialPort)
{
	_log.Log(LOG_STATUS, "DomoCAN Start HW with ID: %d, DomoCAN ID: %d ,Interface Name: %s", ID, m_dcan_id, m_dcan_ifname.c_str());
	m_HwdID = ID;
}

DomoCAN::~DomoCAN()
{
}

bool DomoCAN::StartHardware()
{
	RequestStart();

	//Start worker thread
	m_thread = std::make_shared<std::thread>(&DomoCAN::Do_Work, this);
	SetThreadNameInt(m_thread->native_handle());
	sOnConnected(this);
	//StartHeartbeatThread();
	m_bIsStarted = true;
	return (m_thread != nullptr);
}

bool DomoCAN::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	//StopHeartbeatThread();
	return true;
}

bool DomoCAN::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	uint8_t DomoCANDevType, DomoCANID, unitcode, nvalue;
	
	const tRBUF *pCmd = reinterpret_cast<const tRBUF*>(pdata);
	if ((pCmd->LIGHTING2.packettype == pTypeLighting2)) {
		DomoCANDevType=pCmd->LIGHTING2.id4;
		DomoCANID=pCmd->LIGHTING2.id3;
		uint8_t unitcode = pCmd->LIGHTING2.unitcode;
		uint8_t nvalue = pCmd->LIGHTING2.cmnd;
		_log.Log(LOG_NORM, "DomoCAN::WriteToHardware DomoCANDevType: %d, DomoCANID: %d, Coin: %d", DomoCANDevType, DomoCANID, unitcode);
		switch(DomoCANDevType)
		{
			case DCANTYPE_COIN_SENDER_05:
			case DCANTYPE_COIN_SENDER_1:
			case DCANTYPE_COIN_SENDER_2:
			case DCANTYPE_COIN_SENDER_3:
			case DCANTYPE_COIN_SENDER_4:
			case DCANTYPE_COIN_SENDER_5:
			case DCANTYPE_COIN_SENDER_6:
				if (nvalue=1)
					return (SendCoin(unitcode, DomoCANID));
				break;
				
		}

	}
	return true;
}

void DomoCAN::Do_Work()
{
	int msec_counter = 0;
	int sec_counter = 0;
	_log.Log(LOG_STATUS, "DomoCAN: Worker started...");

	fd_set rdfs;
	int s, nbytes, rc;
	struct can_frame frame;
	struct timeval timeout;

	CanEidTypeDef eid;

	memset(&frame, 0x0, sizeof(frame));

	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;

	// socketCAN stuff

	s = SocketCAN_Open();

	while (!IsStopRequested(0))
	{
		// Make sure CAN socket is opened successfully
		if (s < 0)
		{
			_log.Log(LOG_ERROR, "DomoCAN::Do_Work(): Failed to open socketCAN. Retrying in 5 seconds");
			if (!IsStopRequested(5000))
			{
				s = SocketCAN_Open();
				continue;
			}
			else
				break;
		}
		
		FD_ZERO(&rdfs);
		FD_SET(s, &rdfs);
		if ((rc = select(s+1, &rdfs, NULL, NULL, &timeout)) < 0) {
			_log.Log(LOG_ERROR, "DomoCAN::Do_Work error fd select");
			continue;
		}
		if (FD_ISSET(s, &rdfs))
		{
			nbytes = read(s, &frame, sizeof(frame));
			//printf("nbytes= %d\n", nbytes);
			if (nbytes > 0) 
			{
				if ((size_t)nbytes != CAN_MTU){
					_log.Log(LOG_ERROR, "DomoCAN::Do_Work read: incomplete CAN frame");
					continue;
				}
				eid.eid = frame.can_id;
				if (eid.eid_bf.function == FCN_COIN_EVENT || eid.eid_bf.function == FCN_ACTIVE_EVENT)
				{
					Process_RX_EVENT(&eid, (uint8_t*)&frame.data[0]);
				}
			}
			else if (nbytes < 0)
			{
				_log.Log(LOG_ERROR, "DomoCAN::Do_Work Error reading from socketCAN: %d, %s", errno, strerror(errno));
				continue;
			}
		}

		
		msec_counter++;
		if (msec_counter == 1000)
		{
			msec_counter = 0;
			sec_counter++;
			if (sec_counter % 12 == 0) {
				//_log.Log(LOG_STATUS, "DomoCAN::Do_Work() m_LastHeartbeat = mytime(NULL);");
				m_LastHeartbeat = mytime(NULL);
			}
			/*
			try
			{
				if (sec_counter % DCAN_READ_INTERVAL == 0)
				{
				}
			}
			catch (...)
			{
				_log.Log(LOG_ERROR, "DomoCAN: Error reading sensor data!...");
			}
			*/
		}
	}
	close(s);
	_log.Log(LOG_STATUS, "DomoCAN: Worker stopped...");
}


// Returns a file id for the port/bus
int DomoCAN::SocketCAN_Open()
{
	int s;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct can_filter rfilter[2];
	struct timeval timeout;
	memset(&ifr, 0, sizeof(ifr.ifr_name));

	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (s < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin Failed to open SocketCAN!...");
		return s;
	}

	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
	
    rfilter[0].can_id   = m_dcan_id | EFF_FLAG_MASK;
    rfilter[0].can_mask = DST_NODE_ID_MASK | EFF_FLAG_MASK;
	rfilter[1].can_id   = 0x7F | EFF_FLAG_MASK;
    rfilter[1].can_mask = DST_NODE_ID_MASK | EFF_FLAG_MASK;

	if(setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0){
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin setsockopt CAN_RAW_JOIN_FILTERS not supported by your Linux Kernel");
		close(s);
		return -1;
	}
	
	strncpy(ifr.ifr_name, m_dcan_ifname.c_str(), m_dcan_ifname.length());
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin SIOCGIFINDEX failure");
		close(s);
		return -1;
	}
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_family = AF_CAN;

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin bind failure");
		close(s);
		return -1;
	}
	
	return (s);

}

void DomoCAN::Process_RX_EVENT(CanEidTypeDef* eid, uint8_t* data)
{

	std::vector<std::vector<std::string> > result;
	char szID[40];
	float fcoin;
	int ID, iActiveState;
	
	if (eid->eid_bf.function == FCN_COIN_EVENT)
	{
		switch (eid->eid_bf.parameter)
		{
			case EVENT_COIN_50_CENTS:
			case EVENT_COIN_1_EURO:
			case EVENT_COIN_2_EURO:
				fcoin = eid->eid_bf.parameter;
				std::string strCoin = std::to_string(fcoin/2);
				
				//_log.Log(LOG_NORM, "DomoCAN::Process_RX_EVENT FCN_COIN_EVENT: %d, time: %02x:%02x:%02x - %02x/%02x/20%02x", eid->eid_bf.parameter, data[3], data[4], data[5], data[2], data[1], data[0] );
					
				result = m_sql.safe_query("SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (DomoCANDevType==%d) AND (DomoCANID==%d)", m_HwdID, DCANTYPE_COIN_COUNTER, eid->eid_bf.SRC_node_id);
				if (!result.empty()) // device found
				{
					ID = atoi(result[0][0].c_str());
					sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, eid->eid_bf.SRC_node_id, DCANTYPE_COIN_COUNTER);
					//_log.Log(LOG_NORM, "ID: %d, strCoin: %s, szID: %s", ID, strCoin.c_str(), szID);
					m_mainworker.UpdateDevice(m_HwdID, szID, 1, pTypeGeneral, sTypeCounterIncremental, 0, strCoin, 12, 255);
				}
				
				
				result = m_sql.safe_query("SELECT ID, DomoCANID FROM DomoCANNodes WHERE (HardwareID==%d) AND (DomoCANDevType==%d)", m_HwdID, DCANTYPE_COIN_COUNTER_TOTAL);
				if (!result.empty()) // device found
				{
					ID = atoi(result[0][0].c_str());
					sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, eid->eid_bf.SRC_node_id, DCANTYPE_COIN_COUNTER_TOTAL);
					//_log.Log(LOG_NORM, "ID: %d, strCoin: %s, szID: %s", ID, strCoin.c_str(), szID);
					m_mainworker.UpdateDevice(m_HwdID, szID, 2, pTypeGeneral, sTypeCounterIncremental, 0, strCoin, 12, 255);
				}
				//insert_db_coin_reg((void*)db, can_node->DbData->LocID, can_node->DbData->DevID, (uint8_t)eid->eid_bf.parameter, data);
				
				break;
		}
	}
	else if (eid->eid_bf.function == FCN_ACTIVE_EVENT)
	{
		
		if (eid->eid_bf.parameter == EVENT_INACTIVE)
			iActiveState = 0;
		else if (eid->eid_bf.parameter == EVENT_ACTIVE)
			iActiveState = 1;
			
		std::string strActiveState = std::to_string(iActiveState);

		result = m_sql.safe_query("SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (DomoCANDevType==%d) AND (DomoCANID==%d)", m_HwdID, DCANTYPE_ACTIVE_MON, eid->eid_bf.SRC_node_id);
		if (!result.empty()) // device found
		{
			ID = atoi(result[0][0].c_str());
			sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, eid->eid_bf.SRC_node_id, DCANTYPE_ACTIVE_MON);
			//_log.Log(LOG_NORM, "ID: %d, strActiveState: %s, szID: %s", ID, strActiveState.c_str(), szID);
			m_mainworker.UpdateDevice(m_HwdID, szID, 0, pTypeLighting2, sTypeAC, iActiveState, strActiveState, 12, 255);
		}

		//printf("EVENT_ACTIVE: %d, time: %02x:%02x:%02x - %02x/%02x/20%02x, IDx: %d\n", eid->eid_bf.parameter, data[3], data[4], data[5], data[2], data[1], data[0], can_node->IDxData->ActiveIDx );
		//insert_db_active_reg((void*)db, can_node->DbData->LocID, can_node->DbData->DevID, (uint8_t)eid->eid_bf.parameter, data);
		
	}
	
}

bool DomoCAN::SendCoin(const char coin, const char DomoCANID)
{

	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct can_frame frame;
	struct timeval timeout;
	struct can_filter rfilter;

	memset(&frame, 0x0, sizeof(frame));
	memset(&ifr, 0, sizeof(ifr.ifr_name));
	
	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (s < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin Failed to open SocketCAN!...");
		return false;
	}

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);

    rfilter.can_id   = (m_dcan_id | (DomoCANID << 7) | (FCN_NODE_ACK << 16));
    rfilter.can_mask = DST_NODE_ID_MASK | SRC_NODE_ID_MASK | FUNCTION_MASK;
	
	if(setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0){
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin setsockopt CAN_RAW_JOIN_FILTERS not supported by your Linux Kernel");
		close(s);
		return false;
	}
	
	strncpy(ifr.ifr_name, m_dcan_ifname.c_str(), m_dcan_ifname.length());
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin SIOCGIFINDEX failure");
		close(s);
		return false;
	}
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_family = AF_CAN;



	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin bind failure");
		close(s);
		return false;
	}

	CanEidTypeDef eid;
	eid.eid_bf.SRC_node_id = m_dcan_id;
	eid.eid_bf.DST_node_id = DomoCANID;
	eid.eid_bf.function = FCN_COIN_EVENT;
	eid.eid_bf.parameter = coin;
	eid.eid_bf.eff_flag = 1;


	frame.can_id = eid.eid;
	frame.can_dlc = 0;
	
	nbytes = write(s, &frame, sizeof(frame));
	if (nbytes < 0) {
		_log.Log(LOG_ERROR, "DomoCAN::SendCoin error writing CAN frame");
		close(s);
		return false;
	}
	nbytes = read(s, &frame, sizeof(frame));
	if (nbytes > 0) 
	{
		//printf("nbytes: %d\n", nbytes);
		if ((size_t)nbytes == CAN_MTU)
		{			
			CanEidTypeDef eidrx;
			eidrx.eid = frame.can_id;
			if (eidrx.eid_bf.rtr_flag == 0)
				_log.Log(LOG_NORM, "DomoCAN::SendCoin ACK received from Node: %d, Coin sent successfully.", eidrx.eid_bf.SRC_node_id);
			else
			{
				_log.Log(LOG_ERROR, "DomoCAN::SendCoin Error - incorrect response");
				close(s);
				return false;
			}
		}
		else
			{
				_log.Log(LOG_ERROR, "DomoCAN::SendCoin read: incomplete CAN frame");
				close(s);
				return false;
			}
	}
	else
		{
			_log.Log(LOG_ERROR, "DomoCAN::SendCoin Timeout. No response from node.");
			close(s);
			return false;
		}
	
	
	close(s);
	return true;
	
}


void DomoCAN::AddDevice(const int ID, const std::string &Name, const int DomoCANDevType, const int DomoCANID)
{
	char szID[40];
	char szName[100];
	
	switch (DomoCANDevType)
	{
	case DCANTYPE_COIN_SENDER_05:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 1, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_1:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 2, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_2:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 4, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_3:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 6, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_4:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 8, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_5:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 10, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_6:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 12, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_COUNTER:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 1, pTypeGeneral, sTypeCounterIncremental, MTYPE_COUNTER, 0, "0", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_COUNTER_TOTAL:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 2, pTypeGeneral, sTypeCounterIncremental, MTYPE_COUNTER, 0, "0", Name, 12, 255, 1);
		break;
	case DCANTYPE_ACTIVE_MON:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.InsertDevice(m_HwdID, szID, 0, pTypeLighting2, sTypeAC, STYPE_Motion, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_RELAY_MODULE_5:
		for (char relayid = 0; relayid < 5; relayid++) {
			sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
			sprintf(szName, "%s_%d", Name.c_str(), relayid);
			m_sql.InsertDevice(m_HwdID, szID, relayid, pTypeLighting2, sTypeAC, STYPE_OnOff, 0, " ", szName, 12, 255, 0);
		}
		break;
	}
}

void DomoCAN::RemoveDevice(const int ID, const int DomoCANDevType, const int DomoCANID)
{
	char szID[40];
	
	switch (DomoCANDevType)
	{
	case DCANTYPE_COIN_SENDER_05:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_1:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_2:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_3:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_4:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_5:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_6:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_COUNTER:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_COUNTER_TOTAL:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_ACTIVE_MON:
		sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_RELAY_MODULE_5:
		for (char relayid = 0; relayid < 5; relayid++) {
			sprintf(szID, "%X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF);
			m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		}
		break;
	}
}

void DomoCAN::AddNode(const std::string &Name, const int DomoCANDevType, const int DomoCANID)
{
	std::vector<std::vector<std::string> > result;

	//_log.Log(LOG_STATUS, "SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)", m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);

	//Check if exists
	result = m_sql.safe_query("SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)",
		m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);
	if (!result.empty())
		return; //Already exists

	//_log.Log(LOG_STATUS, "INSERT INTO DomoCANNodes (HardwareID, Name, DomoCANDevType, DomoCANID) VALUES (%d,'%q', %d, %d)", m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);

	m_sql.safe_query("INSERT INTO DomoCANNodes (HardwareID, Name, DomoCANDevType, DomoCANID) VALUES (%d,'%q', %d, %d)",
		m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);

	//_log.Log(LOG_STATUS, "SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)", m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);
	
	result = m_sql.safe_query("SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)",
		m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);
	if (result.empty())
		return;
	int ID = atoi(result[0][0].c_str());
	AddDevice(ID, Name, DomoCANDevType, DomoCANID);

}

bool DomoCAN::UpdateNode(const int ID, const std::string &Name, const int DomoCANDevType, const int DomoCANID)
{
	std::vector<std::vector<std::string> > result;

	result = m_sql.safe_query("SELECT DomoCANDevType,DomoCANID FROM DomoCANNodes WHERE (HardwareID==%d) AND (ID==%d)", m_HwdID, ID);
	if (result.empty())
		return false; //Not Found!?

	int CurrDeviceType = atoi(result[0][0].c_str());
	int CurrDomoCANID = atoi(result[0][1].c_str());

	//Delete device/s
	RemoveDevice(ID, CurrDeviceType, CurrDomoCANID);

	m_sql.safe_query("UPDATE DomoCANNodes SET Name='%q', DomoCANDevType=%d, DomoCANID=%d WHERE (HardwareID==%d) AND (ID==%d)",
		Name.c_str(), DomoCANDevType, DomoCANID, m_HwdID, ID);

	AddDevice(ID, Name, DomoCANDevType, DomoCANID);

	return true;
}

void DomoCAN::RemoveNode(const int ID)
{
	std::vector<std::vector<std::string> > result;

	//Check if exists
	result = m_sql.safe_query("SELECT DomoCANDevType,DomoCANID FROM DomoCANNodes WHERE (HardwareID==%d) AND (ID==%d)", m_HwdID, ID);
	if (result.empty())
		return ; //Not Found!?

	int DeviceType = atoi(result[0][0].c_str());
	int DomoCANID = atoi(result[0][1].c_str());

	//Delete device/s
	RemoveDevice(ID, DeviceType, DomoCANID);

	//Delete node
	m_sql.safe_query("DELETE FROM DomoCANNodes WHERE (HardwareID==%d) AND (ID==%d)",
		m_HwdID, ID);

}

void DomoCAN::RemoveAllNodes()
{
	m_sql.safe_query("DELETE FROM DomoCANNodes WHERE (HardwareID==%d)", m_HwdID);

	//Also delete the all switches
	m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d)",
		m_HwdID);
}

//Webserver helpers
namespace http {
	namespace server {
		void CWebServer::Cmd_DomoCANGetNodes(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}
			std::string hwid = request::findValue(&req, "hid");
			if (hwid == "")
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pHardware = m_mainworker.GetHardware(iHardwareID);
			if (pHardware == NULL)
				return;
			if (pHardware->HwdType != HTYPE_DomoCANGW)
				return;

			root["status"] = "OK";
			root["title"] = "DomoCANGetNodes";

			std::vector<std::vector<std::string> > result;
			result = m_sql.safe_query("SELECT ID,Name,DomoCANDevType,DomoCANID FROM DomoCANNodes WHERE (HardwareID==%d)",
				iHardwareID);
			if (!result.empty())
			{
				std::vector<std::vector<std::string> >::const_iterator itt;
				int ii = 0;
				for (itt = result.begin(); itt != result.end(); ++itt)
				{
					std::vector<std::string> sd = *itt;

					root["result"][ii]["idx"] = sd[0];
					root["result"][ii]["Name"] = sd[1];
					//root["result"][ii]["DevID"] = sd[2];
					root["result"][ii]["DomoCANDevType"] = sd[2];
					root["result"][ii]["DomoCANID"] = sd[3];
					ii++;
				}
			}
		}

		void CWebServer::Cmd_DomoCANAddNode(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}

			std::string hwid = request::findValue(&req, "hid");
			std::string name = HTMLSanitizer::Sanitize(request::findValue(&req, "name"));
			std::string dcanid = request::findValue(&req, "dcanid"); 
			std::string devtype = request::findValue(&req, "devtype");
			if (
				(hwid == "") ||
				(name == "") ||
				(dcanid == "") ||
				(devtype == "")
				)
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pBaseHardware = m_mainworker.GetHardware(iHardwareID);
			if (pBaseHardware == NULL)
				return;
			if (pBaseHardware->HwdType != HTYPE_DomoCANGW)
				return;
			DomoCAN *pHardware = reinterpret_cast<DomoCAN*>(pBaseHardware);

			int DeviceType = atoi(devtype.c_str());
			int DomoCANID = atoi(dcanid.c_str());
			root["status"] = "OK";
			root["title"] = "DomoCANAddNode";
			_log.Log(LOG_STATUS, "Cmd_DomoCANAddNode: hid: %d, Name: %s, Devtype: %d, dcanid: %d", iHardwareID, name.c_str(), DeviceType, DomoCANID );
			pHardware->AddNode(name, DeviceType, DomoCANID);
		}

		void CWebServer::Cmd_DomoCANUpdateNode(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}

			std::string hwid = request::findValue(&req, "hid");
			std::string devidx = request::findValue(&req, "idx");
			std::string name = HTMLSanitizer::Sanitize(request::findValue(&req, "name"));
			std::string dcanid = request::findValue(&req, "dcanid"); 
			std::string devtype = request::findValue(&req, "devtype");
			if (
				(hwid == "") ||
				(devidx == "") ||
				(name == "") ||
				(dcanid == "") ||
				(devtype == "")
				)
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pBaseHardware = m_mainworker.GetHardware(iHardwareID);
			if (pBaseHardware == NULL)
				return;
			if (pBaseHardware->HwdType != HTYPE_DomoCANGW)
				return;
			DomoCAN *pHardware = reinterpret_cast<DomoCAN*>(pBaseHardware);

			int DeviceIDx = atoi(devidx.c_str());
			int DeviceType = atoi(devtype.c_str());
			int DomoCANID = atoi(dcanid.c_str());
			root["status"] = "OK";
			root["title"] = "DomoCANUpdateNode";
			pHardware->UpdateNode(DeviceIDx, name, DeviceType, DomoCANID);
		}

		void CWebServer::Cmd_DomoCANRemoveNode(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}

			std::string hwid = request::findValue(&req, "hid");
			std::string devidx = request::findValue(&req, "idx");
			if (
				(hwid == "") ||
				(devidx == "")
				)
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pBaseHardware = m_mainworker.GetHardware(iHardwareID);
			if (pBaseHardware == NULL)
				return;
			if (pBaseHardware->HwdType != HTYPE_DomoCANGW)
				return;
			DomoCAN *pHardware = reinterpret_cast<DomoCAN*>(pBaseHardware);

			int DeviceIDx = atoi(devidx.c_str());
			root["status"] = "OK";
			root["title"] = "DomoCANRemoveNode";
			pHardware->RemoveNode(DeviceIDx);
		}

		void CWebServer::Cmd_DomoCANClearNodes(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}

			std::string hwid = request::findValue(&req, "hid");
			if (hwid == "")
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pBaseHardware = m_mainworker.GetHardware(iHardwareID);
			if (pBaseHardware == NULL)
				return;
			if (pBaseHardware->HwdType != HTYPE_DomoCANGW)
				return;
			DomoCAN *pHardware = reinterpret_cast<DomoCAN*>(pBaseHardware);

			root["status"] = "OK";
			root["title"] = "DomoCANClearNodes";
			pHardware->RemoveAllNodes();
		}
	}
}


