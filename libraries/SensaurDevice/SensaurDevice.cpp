#include "SensaurDevice.h"
#include "ArduinoJson.h"


// ======== COMPONENT CLASS ========


Component::Component() {
	m_dir = 0;
	m_type[0] = 0;
	m_model[0] = 0;
	m_units[0] = 0;
	m_value[0] = 0;
}


void Component::setValue(const char *value) {
	strncpy(m_value, value, 10);
}


void Component::setInfo(const char *info) {
	m_dir = info[0];
	int part = 0;
	int pos = 0;
	int typeStart = 0, unitsStart = 0, modelStart = 0;
	while (info[pos]) {
		if (info[pos] == ',') {
			part++;
			switch (part) {
			case 1: typeStart = pos + 1; break;
			case 2: modelStart = pos + 1; break;
			case 3: unitsStart = pos + 1; break;
			}
		}
		pos++;
	}
	if (modelStart) {
		int typeLen = modelStart - typeStart - 1;
		if (typeLen > 20) typeLen = 20;
		int modelLen = unitsStart - modelStart - 1;
		if (modelLen > 20) modelLen = 20;
		strncpy(m_type, info + typeStart, typeLen);
		strncpy(m_model, info + modelStart, modelLen);
		strncpy(m_units, info + unitsStart, 20);
	}
	strncpy(m_idSuffix, m_type, 6);  // use first 5 characters of type
}


String Component::infoJson() {
	char dirStr[2] = {m_dir, 0};
	DynamicJsonDocument doc(256);
	doc["dir"] = dirStr;
	doc["type"] = m_type;
	doc["model"] = m_model;
	doc["units"] = m_units;
	String message;
	serializeJson(doc, message);
	return message;
}


// ======== DEVICE CLASS ========



Device::Device() {
	m_id[0] = 0;
	m_version[0] = 0;
	m_connected = false;
	m_componentCount = 0;
}


void Device::setId(const char *id) {
	strncpy(m_id, id, 10);
}


void Device::setVersion(const char *version) {
	strncpy(m_version, version, 10);
}
