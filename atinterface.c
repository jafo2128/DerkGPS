/*
  atinterface.c

  Copyright (c) 2008-2009 Patrick Bellasi

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

*/

#include "atinterface.h"
#include "serials.h"
// #include "gps.h"

/// Golbal variables defined within derkgps.c
extern volatile unsigned long d_pcount;
/// Last computed odometer pulses frequency
extern unsigned long d_freq;
/// Current max [ppm/s] OVER_SPEED alarm
extern unsigned long d_minOverSpeed;
/// Current max [ppm/s] decelleration EMERGENCY_BREAK alarm
extern unsigned long d_minEmergencyBreak;
/// Delay ~[s] between dispaly monitor sentences, if 0 DISABLED (default 0);
extern unsigned d_displayTime;
/// Events pending to be ACKed
extern derkgps_event_t d_pendingEvents[EVENT_CLASS_TOT];

/// The buffer for output and result return
extern char d_displayBuff[OUTPUT_BUFFER_SIZE];

/// Readed and parsed value
unsigned long  newValueUL;
unsigned newValueU;

#define cmdLook()	look(UART1)
#define cmdRead()	read(UART1)
#define cmdAvailable()	available(UART1)

#define Serial_printValue(STR)	printStr(UART1, STR); printStr(UART1, " ")


#define ShowValue(VALUE)				\
	snprintf(d_displayBuff, OUTPUT_BUFFER_SIZE,	\
		 "%d", VALUE);				\
	Serial_printStr(d_displayBuff);			\
	Serial_printStr(" ")
	
#define ShowValueU(VALUE)				\
	snprintf(d_displayBuff, OUTPUT_BUFFER_SIZE,	\
		 "%u", VALUE);				\
	Serial_printStr(d_displayBuff);			\
	Serial_printStr(" ")

#define ShowValueUL(VALUE)				\
	snprintf(d_displayBuff, OUTPUT_BUFFER_SIZE,	\
		 "%lu", VALUE);				\
	Serial_printStr(d_displayBuff);			\
	Serial_printStr(" ")

#define ShowValueD(VALUE)					\
	formatDouble(VALUE, d_displayBuff, OUTPUT_BUFFER_SIZE);	\
	Serial_printStr(d_displayBuff);			\
	Serial_printStr(" ")

/// Copy a command value from UART buffer to local (d_displayBuff) buffer
void cmdReadValue() {
	short iValRead = 0;
	do {
		d_displayBuff[iValRead] = cmdLook();
		if (d_displayBuff[iValRead]!=-1 &&		// uart empty
		    d_displayBuff[iValRead]!=LINE_TERMINATOR &&	// line end
		    d_displayBuff[iValRead]!='+') {		// cmd following
			// Removing char from uart buffer
			cmdRead();
			iValRead++;
		} else {
			// Null terminating string and returning
			d_displayBuff[iValRead]=0;
			return;
		}
	} while(1);
}

#define ReadValueU(VALUE)				\
	cmdReadValue();					\
	sscanf(d_displayBuff, "%u", &newValueU);	\
	VALUE = newValueU;

#define ReadValueUL(VALUE)				\
	cmdReadValue();					\
	sscanf(d_displayBuff, "%lu", &newValueUL);	\
	VALUE = newValueUL;

inline int parseAlarmCmd(int type) {
	
	switch(cmdRead()) {
	case 'E':
		switch(cmdRead()) { 
		case 'B':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Emergency Break"
				ShowValueUL(d_minEmergencyBreak);
				goto pac_ok;
			case '=':
				// WRITE "Emergency Break"
				cmdRead();
				ReadValueUL(d_minEmergencyBreak);
				goto pac_ok;
			}
			goto pac_error;
		}
		goto pac_error;
	case 'S':
		switch(cmdRead()) { 
		case 'L':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Speed Limit"
				ShowValueUL(d_minOverSpeed);
				goto pac_ok;
			case '=': // WRITE "Speed Limit"
				cmdRead();
				ReadValueUL(d_minOverSpeed);
				goto pac_ok;
			}
			goto pac_error;
		}
		goto pac_error;
	}

pac_error:
	return ERROR;
pac_ok:
	return OK;

}

inline int parseGpsCmd(int type) {
	
	switch(cmdRead()) {
	case 'F':
		switch(cmdRead()) {
		case 'V':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Fix value"
				ShowValueU(gpsFix());
				goto pgc_ok;
			}
			goto pgc_error;
		}
		goto pgc_error;
	case 'G':
		switch(cmdRead()) {
		case 'S':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Ground speed"
				ShowValueD(gpsSpeed());
				goto pgc_ok;
			}
			goto pgc_error;
		}
		goto pgc_error;
	case 'L':
		switch(cmdRead()) {
		case 'A':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Lat"
				if (gpsIsPosValid()) {
					ShowValueD(gpsLat());
// 					formatDouble(gpsLat(), d_displayBuff, OUTPUT_BUFFER_SIZE);
// 					Serial_printLine(d_displayBuff);
// 					Serial_printLine(gpsLatStr());
				} else
					Serial_printStr("NA ");
				goto pgc_ok;
			}
			goto pgc_error;
		case 'O':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Lon"
				if (gpsIsPosValid()) {
					ShowValueD(gpsLon());
// 					formatDouble(gpsLon(), d_displayBuff, OUTPUT_BUFFER_SIZE);
// 					Serial_printLine(d_displayBuff);
// 					Serial_printLine(gpsLonStr());
				} else
					Serial_printStr("NA ");
				goto pgc_ok;
			}
			goto pgc_error;
		}
		goto pgc_error;
	}

pgc_error:
	return ERROR;
pgc_ok:
	return OK;

}

inline int parseOdoCmd(int type) {

	switch(cmdRead()) {
	case 'C':
		switch(cmdRead()) { 
		case 'P':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Pulse conunt"
				ShowValueUL(d_pcount);
				goto poc_ok;
			case '=':
				// WRITE "Pulse conunt"
				cmdRead();
				ReadValueUL(d_pcount);
				goto poc_ok;
			}
			goto poc_error;
		}
		goto poc_error;
	case 'F':
		switch(cmdRead()) { 
		case 'P':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Pulse frequency"
				ShowValueUL(d_freq);
				goto poc_ok;
			}
			goto poc_error;
		}
		goto poc_error;
	}
	
poc_error:
	return ERROR;
poc_ok:
	return OK;

}

inline int parseQueryCmd(int type) {
	unsigned long events;
	
	switch(cmdRead()) {
	case 'E':
		switch(cmdRead()) {
		case 'R':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Event register"
				events = d_pendingEvents[EVENT_CLASS_GPS];
				events <<= 8;
				events |= d_pendingEvents[EVENT_CLASS_ODO];
				snprintf(d_displayBuff, OUTPUT_BUFFER_SIZE, "%lu 0x%02X%02X",
						events,
						d_pendingEvents[EVENT_CLASS_GPS],
						d_pendingEvents[EVENT_CLASS_ODO]);
// 				Serial_printLine(d_displayBuff);
				Serial_printValue(d_displayBuff);
				
				// Resetting pending event
				d_pendingEvents[EVENT_CLASS_GPS] = EVENT_NONE;
				d_pendingEvents[EVENT_CLASS_ODO] = EVENT_NONE;
				
				goto pqc_ok;
			}
			goto pqc_error;
		}
		goto pqc_error;
	case 'C':
		switch(cmdRead()) {
		case 'M':
			switch(cmdLook()) {
			case LINE_TERMINATOR:
				cmdRead();
			case '+':
				// READ  "Continuous monitor"
				ShowValueU(d_displayTime);
				goto pqc_ok;
			case '=':
				// WRITE "Continuous monitor"
				cmdRead();
				ReadValueU(d_displayTime);
				goto pqc_ok;
			}
			goto pqc_error;
		}
		goto pqc_error;
	}

pqc_error:
	return ERROR;
pqc_ok:
	return OK;

}

inline int parseCmdClass(int type) {
	switch(cmdRead()) {
	case 'A':
		// Alarms
		return parseAlarmCmd(type);
		break;
	case 'G':
		// GPS
		return parseGpsCmd(type);
		break;
	case 'O':
		// Odometer
		return parseOdoCmd(type);
		break;
	case 'Q':
		// Query registry
		return parseQueryCmd(type);
		break;
	}
	return ERROR;
}


int parseCommand() {
	int result = OK;
	
	do {
		switch(cmdRead()) {
		case LINE_TERMINATOR:
			// Be fair: return after a command line has been processed
			break;
		case '+':
			result = parseCmdClass(ASCII);
			break;
		case '$':
			result = parseCmdClass(BINARY);
			break;
		}
		
		if (result!=OK) {
			// Error on cmds parsing... aborting next commands
			break;
		}
		
	} while ( cmdAvailable() );
	
	Serial_printFlush();
	Serial_printLine("");
	if (result==OK)
		Serial_printLine("OK");
	else
		Serial_printLine("ERROR");
	
	return result;
	
}