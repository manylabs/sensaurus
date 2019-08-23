#ifndef _SENSAUR_DEVICE_H_
#define _SENSAUR_DEVICE_H_
#include "Arduino.h"


#define MAX_COMPONENT_COUNT 6
#define MAX_TYPE_LEN 20
#define MAX_MODEL_LEN 20
#define MAX_UNITS_LEN 20
#define MAX_DEVICE_ID_LEN 38


class Component {
public:

	Component();

	// the current value (for sensors)
	// we represent values as strings so as not to worry about gaining/losing decimal places
	inline const char *value() const { return m_value; }
	void setValue(const char *value);

	// info received from the device
	// e.g.: "i,CO2,K-30,PPM"
	void setInfo(const char *info);
	inline char dir() const { return m_dir; };

	// retrieve the component info as a JSON string
	String infoJson();

	// retrieve the type-based ID suffix for this component
	inline const char *idSuffix() const { return m_idSuffix; }

	// last value sent to actuator; stored here so we can update one actuator on a hub without changing the others
	inline void setActuatorValue(float v) { m_actuatorValue = v; }
	inline float actuatorValue() { return m_actuatorValue; }

private:

	// current state
	char m_value[10];

	// info from the device
	char m_dir;
	char m_type[MAX_TYPE_LEN];
	char m_model[MAX_MODEL_LEN];
	char m_units[MAX_UNITS_LEN];
	char m_idSuffix[6];

	// last value sent to actuator; stored here so we can update one actuator on a hub without changing the others
	float m_actuatorValue;
};



class Device {
public:
	Device();

	// basic device attributes
	inline const char *id() const { return m_id; }
	void setId(const char *id);
	inline const char *version() const { return m_version; }
	void setVersion(const char *version);

	// current status
	inline bool connected() const { return m_connected; }
	inline void setConnected(bool connected) { m_connected = connected; }
	inline int noResponseCount() const { return m_noResponseCount; }
	inline void noResponse() { m_noResponseCount++; }
	inline void responded() { m_noResponseCount = 0; }
	inline void resetErrorCount() { m_errorCount = 0;}
	// returns true if switched to holdoff state
	bool incErrorCount();
	inline int getErrorCount() const { return m_errorCount; }

	// component access
	inline int componentCount() const { return m_componentCount; }
	inline void setComponentCount(int componentCount) { m_componentCount = (componentCount < MAX_COMPONENT_COUNT) ? componentCount : MAX_COMPONENT_COUNT; }
	inline Component &component(int index) { return m_components[index]; }
	inline void resetComponents() { m_componentCount = 0; }

private:
	char m_version[10];
	char m_id[MAX_DEVICE_ID_LEN];
	bool m_connected;
	int m_componentCount;
	int m_noResponseCount;
	// checksum error count
	int m_errorCount;
	Component m_components[MAX_COMPONENT_COUNT];
};


#endif  // _SENSAUR_DEVICE_H_
