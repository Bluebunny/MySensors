/*
 The MySensors library adds a new layer on top of the RF24 library.
 It handles radio network routing, relaying and ids.

 Created by Henrik Ekblad <henrik.ekblad@gmail.com>
	
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
*/

#ifndef MySensor_h
#define MySensor_h

#include "Version.h"   // Auto generated by bot
#include "MyConfig.h"
#include "MyMessage.h"
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include <Arduino.h>
#include <stddef.h>
#include <SPI.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdarg.h>

#ifdef DEBUG
#define debug(x,...) debugPrint(x, ##__VA_ARGS__)
#else
#define debug(x,...)
#endif

#define BAUD_RATE 115200

#define AUTO 0xFF // 0-254. Id 255 is reserved for auto initialization of nodeId.
#define NODE_SENSOR_ID 0xFF // Node child id is always created for when a node

// EEPROM start address for mysensors library data
#define EEPROM_START 0
// EEPROM location of node id
#define EEPROM_NODE_ID_ADDRESS EEPROM_START
// EEPROM location of parent id
#define EEPROM_PARENT_NODE_ID_ADDRESS (EEPROM_START+1)
// EEPROM location of distance to gateway
#define EEPROM_DISTANCE_ADDRESS (EEPROM_PARENT_NODE_ID_ADDRESS+1)
#define EEPROM_ROUTES_ADDRESS (EEPROM_DISTANCE_ADDRESS+1) // Where to start storing routing information in EEPROM. Will allocate 256 bytes.
#define EEPROM_CONTROLLER_CONFIG_ADDRESS (EEPROM_ROUTES_ADDRESS+256) // Location of controller sent configuration (we allow one payload of config data from controller)
#define EEPROM_LOCAL_CONFIG_ADDRESS (EEPROM_CONTROLLER_CONFIG_ADDRESS+24) // First free address for sketch static configuration

// This is the nodeId for sensor net gateway receiver sketch (where all sensors should send their data).
// This is also act as base value for sensor nodeId
#define BASE_RADIO_ID ((uint64_t)0xA8A8E1FC00LL)
#define GATEWAY_ADDRESS ((uint8_t)0)
#define BROADCAST_ADDRESS ((uint8_t)0xFF)
#define TO_ADDR(x) (BASE_RADIO_ID + x)

#define WRITE_PIPE ((uint8_t)0)
#define CURRENT_NODE_PIPE ((uint8_t)1)
#define BROADCAST_PIPE ((uint8_t)2)

// Search for a new parent node after this many transmission failures
#define SEARCH_FAILURES  5

struct NodeConfig
{
	uint8_t nodeId; // Current node id
	uint8_t parentNodeId; // Where this node sends its messages
	uint8_t distance; // This nodes distance to sensor net gateway (number of hops)
};

struct ControllerConfig {
	uint8_t isMetric;
};



class MySensor : public RF24
{
  public:
	/**
	* Constructor
	*
	* Creates a new instance of Sensor class.
	*
	* @param _cepin The pin attached to RF24 Chip Enable on the RF module (defualt 9)
	* @param _cspin The pin attached to RF24 Chip Select (default 10)
	*/
	MySensor(uint8_t _cepin=9, uint8_t _cspin=10);

	/**
	* Begin operation of the MySensors library
	*
	* Call this in setup(), before calling any other sensor net library methods.
	* @param incomingMessageCallback Callback function for incoming messages from other nodes or controller and request responses. Default is NULL.
	* @param relayMode Activate relay mode. This node will forward messages to other nodes in the radio network. Make sure to call process() regularly. Default in false
	* @param nodeId The unique id (1-254) for this sensor. Default is AUTO(255) which means sensor tries to fetch an id from controller.
	* @param paLevel Radio PA Level for this sensor. Default RF24_PA_MAX
	* @param channel Radio channel. Default is channel 76
	* @param dataRate Radio transmission speed. Default RF24_1MBPS
	*/

	void begin(void (* msgCallback)(MyMessage)=NULL, boolean relayMode=false, uint8_t nodeId=AUTO, rf24_pa_dbm_e paLevel=RF24_PA_LEVEL, uint8_t channel=RF24_CHANNEL, rf24_datarate_e dataRate=RF24_DATARATE);

	/**
	 * Return the nodes nodeId.
	 */
	uint8_t getNodeId();

	/**
	* Each node must present all attached sensors before any values can be handled correctly by the controller.
    * It is usually good to present all attached sensors after power-up in setup().
	*
	* @param sensorId Select a unique sensor id for this sensor. Choose a number between 0-254.
	* @param sensorType The sensor type. See sensor typedef in Message.h.
	*/
	void present(uint8_t sensorId, uint8_t sensorType);

	/**
	 * Sends sketch meta information to the gateway. Not mandatory but a nice thing to do.
	 * @param name String containing a short Sketch name or NULL  if not applicable
	 * @param version String containing a short Sketch version or NULL if not applicable
	 */
	void sendSketchInfo(const char *name, const char *version);

	/**
	* Sends a message to gateway or one of the other nodes in the radio network
	*
	* @param msg Message to send
	* @param ack Set this to true if you want destination node to send ack back to this node. Default is not to request any ack.
	* @return true Returns true if message reached the first stop on its way to destination.
	*/
	bool send(MyMessage msg, bool ack=false);

	/**
	 * Send this nodes battery level to gateway.
	 * @param level Level between 0-100(%)
	 */
	void sendBatteryLevel(uint8_t level);

	/**
	* Requests a value from gateway or some other sensor in the radio network.
	* Make sure to add callback-method in begin-method to handle request responses.
	*
	* @param childSensorId  The unique child id for the different sensors connected to this arduino. 0-254.
	* @param variableType The variableType to fetch
	* @param destination The nodeId of other node in radio network. Default is gateway
	*/
	void request(uint8_t childSensorId, uint8_t variableType, uint8_t destination=GATEWAY_ADDRESS);

	/**
	 * Requests time from controller. Answer will be delivered to callback.
	 *
	 * @param callback for time request. Incoming argument is seconds since 1970.
	 */
	void requestTime(void (* timeCallback)(unsigned long));


	/**
	 * Processes incoming messages to this node. If this is a relaying node it will
	* Returns true if there is a message addressed for this node just was received.
	* Use callback to handle incoming messages.
	*/
	boolean process();

	/**
	 * Returns the most recent node configuration received from controller
	 */
	ControllerConfig getConfig();

	/**
	 * Save a state (in local EEPROM). Good for actuators to "remember" state between
	 * power cycles.
	 *
	 * You have 256 bytes to play with. Note that there is a limitation on the number
	 * of writes the EEPROM can handle (~100 000 cycles).
	 *
	 * @param pos The position to store value in (0-255)
	 * @param Value to store in position
	 */
	void saveState(uint8_t pos, uint8_t value);

	/**
	 * Load a state (from local EEPROM).
	 *
	 * @param pos The position to fetch value from  (0-255)
	 * @return Value to store in position
	 */
	uint8_t loadState(uint8_t pos);

	/**
	* Returns the last received message
	*/
	MyMessage getLastMessage(void);



#ifdef DEBUG
	void debugPrint(const char *fmt, ... );
	int freeRam();
#endif

  protected:
	NodeConfig nc; // Essential settings for node to work
	ControllerConfig cc; // Configuration coming from controller
	boolean relayMode;
	MyMessage msg;  // Buffer for incoming messages.
	MyMessage ack;  // Buffer for ack messages.

	void setupRelayMode();
	void setupRadio(rf24_pa_dbm_e paLevel, uint8_t channel, rf24_datarate_e dataRate);
	boolean sendRoute(MyMessage message);
	boolean sendWrite(uint8_t dest, MyMessage message, bool broadcast=false);
	uint8_t validate(MyMessage message);

  private:
	uint8_t failedTransmissions;
	uint8_t *childNodeTable; // In memory buffer for routing information to other nodes. also stored in EEPROM
    void (*timeCallback)(unsigned long); // Callback for requested time messages
    void (*msgCallback)(MyMessage); // Callback for incoming messages from other nodes and gateway.

	void requestNodeId();
	void findParentNode();
	uint8_t crc8Message(MyMessage);
	uint8_t getChildRoute(uint8_t childId);
	void addChildRoute(uint8_t childId, uint8_t route);
	void removeChildRoute(uint8_t childId);
};

#endif
