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
#include "../json/json.h"
#include "../main/HTMLSanitizer.h"
#ifdef HAVE_LINUX_I2C
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#include <math.h>
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
	return true;
}

bool DomoCAN::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{

	const tRBUF *pCmd = reinterpret_cast<const tRBUF*>(pdata);
	if ((pCmd->LIGHTING2.packettype == pTypeLighting2)) {
		uint8_t pin_number = pCmd->LIGHTING2.unitcode; // in DB column "Unit" is used for identify pin number
		uint8_t  value = pCmd->LIGHTING2.cmnd;

	}
	return false;
}

void DomoCAN::Do_Work()
{
	int msec_counter = 0;
	int sec_counter = 0;
	_log.Log(LOG_STATUS, "DomoCAN: Worker started...");

	while (!IsStopRequested(100))
	{
		msec_counter++;
		if (msec_counter == 10)
		{
			msec_counter = 0;
			sec_counter++;
			if (sec_counter % 12 == 0) {
				m_LastHeartbeat = mytime(NULL);
			}
			try
			{
				if (sec_counter % DCAN_READ_INTERVAL == 0)
				{
					if (m_dev_type == DCANTYPE_RELAY_MODULE_5)
					{
					}
					else if (m_dev_type == DCANTYPE_ACTIVE_MON)
					{
					}
				}
			}
			catch (...)
			{
				_log.Log(LOG_ERROR, "DomoCAN: Error reading sensor data!...");
			}
		}
	}
	_log.Log(LOG_STATUS, "DomoCAN: Worker stopped...");
}


// Returns a file id for the port/bus
int DomoCAN::SocketCAN_Open(const char *SocketCANIfName)
{
	int fd;
	//Open port for reading and writing
	if ((fd = open(SocketCANIfName, O_RDWR)) < 0)
	{
		_log.Log(LOG_ERROR, "DomoCAN: Failed to open the i2c bus!...");
		_log.Log(LOG_ERROR, "DomoCAN: Check to see if you have a bus: %s", m_dcan_ifname.c_str());
		_log.Log(LOG_ERROR, "DomoCAN: We might only be able to access this as root user");
		return -1;
	}
	return fd;
}

void DomoCAN::AddDevice(const int ID, const std::string &Name, const int DomoCANDevType, const int DomoCANID)
{
	char szID[40];
	char szName[100];
	
	switch (DomoCANDevType)
	{
	case DCANTYPE_COIN_SENDER_05:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 1);
		m_sql.InsertDevice(m_HwdID, szID, 1, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_1:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 2);
		m_sql.InsertDevice(m_HwdID, szID, 2, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_2:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 4);
		m_sql.InsertDevice(m_HwdID, szID, 4, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_3:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 6);
		m_sql.InsertDevice(m_HwdID, szID, 6, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_4:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 8);
		m_sql.InsertDevice(m_HwdID, szID, 8, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_5:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 10);
		m_sql.InsertDevice(m_HwdID, szID, 10, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_SENDER_6:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 12);
		m_sql.InsertDevice(m_HwdID, szID, 12, pTypeLighting2, sTypeAC, STYPE_PushOn, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_COUNTER:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 1);
		m_sql.InsertDevice(m_HwdID, szID, 1, pTypeGeneral, sTypeCounterIncremental, MTYPE_COUNTER, 0, "0", Name, 12, 255, 1);
		break;
	case DCANTYPE_COIN_COUNTER_TOTAL:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 2);
		m_sql.InsertDevice(m_HwdID, szID, 2, pTypeGeneral, sTypeCounterIncremental, MTYPE_COUNTER, 0, "0", Name, 12, 255, 1);
		break;
	case DCANTYPE_ACTIVE_MON:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 0);
		m_sql.InsertDevice(m_HwdID, szID, 0, pTypeLighting2, sTypeAC, STYPE_Motion, 0, " ", Name, 12, 255, 1);
		break;
	case DCANTYPE_RELAY_MODULE_5:
		for (char relayid = 0; relayid < 5; relayid++) {
			sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, relayid);
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
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 1);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_1:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 2);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_2:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 4);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_3:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 6);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_4:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 8);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_5:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 10);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_SENDER_6:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 12);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_COUNTER:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 1);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_COIN_COUNTER_TOTAL:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 2);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_ACTIVE_MON:
		sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, 0);
		m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		break;
	case DCANTYPE_RELAY_MODULE_5:
		for (char relayid = 0; relayid < 5; relayid++) {
			sprintf(szID, "%02X%02X%02X%02X%02X", (ID & 0xFF00) >> 8, ID & 0xFF, DomoCANID  & 0xFF, DomoCANDevType & 0xFF, relayid);
			m_sql.safe_query("DELETE FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, szID);
		}
		break;
	}
}

void DomoCAN::AddNode(const std::string &Name, const int DomoCANDevType, const int DomoCANID)
{
	std::vector<std::vector<std::string> > result;

	_log.Log(LOG_STATUS, "SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)", m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);

	//Check if exists
	result = m_sql.safe_query("SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)",
		m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);
	if (!result.empty())
		return; //Already exists

	_log.Log(LOG_STATUS, "INSERT INTO DomoCANNodes (HardwareID, Name, DomoCANDevType, DomoCANID) VALUES (%d,'%q', %d, %d)", m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);

	m_sql.safe_query("INSERT INTO DomoCANNodes (HardwareID, Name, DomoCANDevType, DomoCANID) VALUES (%d,'%q', %d, %d)",
		m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);

	_log.Log(LOG_STATUS, "SELECT ID FROM DomoCANNodes WHERE (HardwareID==%d) AND (Name=='%q') AND (DomoCANDevType==%d) AND (DomoCANID==%d)", m_HwdID, Name.c_str(), DomoCANDevType, DomoCANID);
	
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


