// Copyright (c) 2019 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef RDM_CMDC_HEADER
#define RDM_CMDC_HEADER

#include	<string.h>
#include	<stdint.h>

// Generic RDM Commands
class RDM_CmdC
{
public:
	RDM_CmdC(void)
	{
		m_Buffer = 0;
		setCommand(0); setParameter(0); setSubdevice(0); setLength(0); setManufacturerId(0); setTransactionNum(0);
		setMessageCount(0); setDeviceId(0); setResponseType(0);
	}
	RDM_CmdC(uint8_t cmd, uint16_t parameter, uint16_t subdevice = 0, uint8_t len = 0,
		const void *buffer = 0, uint16_t manu = 0, uint32_t dev = 0)
	{
		m_Buffer = 0;
		setCommand(cmd); setParameter(parameter); setSubdevice(subdevice); setLength(len); setBuffer(buffer);
		setTransactionNum(0); setMessageCount(0); setResponseType(0); setManufacturerId(manu); setDeviceId(dev);
	}
	RDM_CmdC(const RDM_CmdC &src)
	{
		m_Buffer = 0;
		operator=(src);
	};
	virtual ~RDM_CmdC(void)				{ setBuffer(0); };
	virtual RDM_CmdC *copyThis(void) const {return new RDM_CmdC(*this);};
	void operator=(const RDM_CmdC &src)
	{
		setCommand(src.getCommand()); setParameter(src.getParameter()); setSubdevice(src.getSubdevice());
		setLength(src.getLength()); setBuffer(src.getBuffer()); setTransactionNum(src.getTransactionNum());
		setMessageCount(src.getMessageCount()); setResponseType(src.getResponseType());
		setManufacturerId(src.getManufacturerId()); setDeviceId(src.getDeviceId());
	}

	bool operator==(const RDM_CmdC &cmd) const
	{
		if((getCommand() != cmd.getCommand()) || (getParameter() != cmd.getParameter()) ||
				(getSubdevice() != cmd.getSubdevice()) || (getLength() != cmd.getLength()) ||
				(getTransactionNum() != cmd.getTransactionNum()) || (getMessageCount() != cmd.getMessageCount()) ||
				(getResponseType() != cmd.getResponseType()) || (getDeviceId() != cmd.getDeviceId()) ||
				(getManufacturerId() != cmd.getManufacturerId()))
		{
			// Basic data doesn't match
			return false;
		}

		if(getLength())
		{
			if(memcmp(getBuffer(), cmd.getBuffer(), getLength()) != 0)
			{
				// Buffer contents doesn't match
				return false;
			}
		}

		return true;
	}
	bool operator!=(const RDM_CmdC &cmd) const { return !(operator ==(cmd)); }

	friend bool operator<(const RDM_CmdC& a, const RDM_CmdC& b);
	friend bool operator>(const RDM_CmdC & lhs, const RDM_CmdC & rhs) { return rhs < lhs; }
	friend bool operator<=(const RDM_CmdC & lhs, const RDM_CmdC & rhs) { return !(lhs > rhs); }
	friend bool operator>=(const RDM_CmdC & lhs, const RDM_CmdC & rhs) { return !(lhs < rhs); }

	uint8_t getCommand(void) const			{ return m_Command; };
	uint16_t getParameter(void) const		{ return m_Parameter; };
	uint16_t getSubdevice(void) const		{ return m_Subdevice; };
	uint8_t getLength(void) const			{ return m_Length; };
	const void *getBuffer(void) const		{ return m_Buffer; };
	uint8_t getTransactionNum(void) const { return m_TransactionNum; }
	uint8_t getResponseType(void) const		{ return m_ResponseType; };
	uint8_t getMessageCount(void) const  { return m_MessageCount; }
	uint16_t getManufacturerId(void) const	{ return manufacturer_id; };
	uint32_t getDeviceId(void) const		{ return device_id; };

	void setCommand(uint8_t newVal)			{ m_Command = newVal; };
	void setParameter(uint16_t newVal)		{ m_Parameter = newVal; };
	void setSubdevice(uint16_t newVal)		{ m_Subdevice = newVal; };
	void setLength(uint8_t newVal)			{ m_Length = newVal; };
	void setBuffer(const void *newVal)		{ if(m_Buffer) {delete [] m_Buffer; m_Buffer = 0;}
											  if(newVal)   {m_Buffer = new uint8_t[m_Length]; memcpy(m_Buffer, newVal, m_Length);}
											};
	void setTransactionNum(uint8_t newVal)	{ m_TransactionNum = newVal; }
	void setResponseType(uint8_t newVal)	{ m_ResponseType = newVal; };
	void setMessageCount(uint8_t newVal)	{ m_MessageCount = newVal; }
	void setManufacturerId(uint16_t id)		{ manufacturer_id = id; };
	void setDeviceId(uint32_t id)			{ device_id = id; };

private:
	uint8_t		m_Command;			// Which RDM command (E120_GET_COMMAND or E120_SET_COMMAND)
	uint16_t	m_Parameter;		// Which PID (e.g. E120_DEVICE_INFO or E120_DEVICE_LABEL)
	uint16_t	m_Subdevice;		// Which subdevice command is for (0 = root)
	uint8_t		m_Length;			// Length of data in "buffer" for this command
	uint8_t		*m_Buffer;			// Data to include with this command
	uint8_t		m_TransactionNum;	// RDM transaction number
	uint8_t		m_ResponseType;		// RDM response type (e.g. E120_RESPONSE_TYPE_ACK, E120_RESPONSE_TYPE_ACK_TIMER)
	uint8_t		m_MessageCount;		// RDM message count, valid only in a response
	uint16_t	manufacturer_id;	// ESTA-assigned manufacturer id
	uint32_t	device_id;			// Unique to the manufacturer
};

#endif
