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

	// find positions of component info elements
	// note: type, model, and units are all optional, but they must appear in this order
	int part = 0;
	int pos = 0;
	int typeStart = 0, modelStart = 0, unitsStart = 0;
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

	// compute lengths
	int typeLen = 0, modelLen = 0, unitsLen = 0;
	if (typeStart) {
		typeLen = modelStart ? (modelStart - typeStart - 1) : pos - typeStart;
	}
	if (modelStart) {
		modelLen = unitsStart ? (unitsStart - modelStart - 1) : pos - modelStart;
	}
	if (unitsStart) {
		unitsLen = pos - unitsStart;
	}
	if (typeLen >= MAX_TYPE_LEN) typeLen = MAX_TYPE_LEN - 1;  // leave room for null-terminator
	if (modelLen >= MAX_MODEL_LEN) modelLen = MAX_MODEL_LEN - 1;
	if (unitsLen >= MAX_UNITS_LEN) unitsLen = MAX_UNITS_LEN - 1;

	// copy component info (doesn't null-terminate in this case)
	strncpy(m_type, info + typeStart, typeLen);
	strncpy(m_model, info + modelStart, modelLen);
	strncpy(m_units, info + unitsStart, unitsLen);

	// null terminate component info
	m_type[typeLen] = 0;
	m_model[modelLen] = 0;
	m_units[unitsLen] = 0;

	// create a component ID suffix from the first 5 characters of type (will be combined with device ID)
	strncpy(m_idSuffix, m_type, 5);
	m_idSuffix[5] = 0;
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
	m_noResponseCount = 0;
}


void Device::setId(const char *id) {
	strncpy(m_id, id, MAX_DEVICE_ID_LEN);
}


void Device::setVersion(const char *version) {
	strncpy(m_version, version, 10);
}
