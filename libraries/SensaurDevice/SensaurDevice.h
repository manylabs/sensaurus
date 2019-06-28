#ifndef _SENSAUR_DEVICE_H_
#define _SENSAUR_DEVICE_H_
#include "Arduino.h"


#define MAX_COMPONENT_COUNT 6


class Component {
public:

	Component();

	// the current value (for sensors)
	// we represent values as strings so as not to worry about gaining/losing decimal places
	const char *value() const { return m_value; }
	void setValue(const char *value);

	// info received from the device
	// e.g.: "i,CO2,K-30,PPM"
	void setInfo(const char *info);
	char dir() const { return m_dir; };

	// retrieve the component info as a JSON string
	String infoJson();

	// retrieve the type-based ID suffix for this component
	const char *idSuffix() const { return m_idSuffix; }

private:

	// current state
	char m_value[10];

	// info from the device
	char m_dir;
	char m_type[20];
	char m_model[20];
	char m_units[20];
	char m_idSuffix[6];
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
	inline uint32_t lastMessageTime() const { return m_lastMessageTime; }
	inline void setLastMessageTime(uint32_t lastMessageTime) { m_lastMessageTime = lastMessageTime; }
	inline bool connected() const { return m_connected; }
	inline void setConnected(bool connected) { m_connected = connected; }

	// component access
	inline int componentCount() const { return m_componentCount; }
	inline void setComponentCount(int componentCount) { m_componentCount = (componentCount < MAX_COMPONENT_COUNT) ? componentCount : MAX_COMPONENT_COUNT; }
	inline Component &component(int index) { return m_components[index]; }
	inline void resetComponents() { m_componentCount = 0; }

private:
	char m_version[10];
	char m_id[10];
	uint32_t m_lastMessageTime;
	bool m_connected;
	int m_componentCount;
	Component m_components[MAX_COMPONENT_COUNT];
};


#endif  // _SENSAUR_DEVICE_H_
