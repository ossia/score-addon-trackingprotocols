#include "RTTrPSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const RTTrP::RTTrPSpecificSettings& n)
{
  m_stream << n.port << n.numTrackables << n.maxLEDsPerTrackable << n.maxZones
           << n.enableQuaternion << n.enableEuler << n.enableVelocity
           << n.enableAcceleration << n.enableZones;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(RTTrP::RTTrPSpecificSettings& n)
{
  m_stream >> n.port >> n.numTrackables >> n.maxLEDsPerTrackable >> n.maxZones
           >> n.enableQuaternion >> n.enableEuler >> n.enableVelocity
           >> n.enableAcceleration >> n.enableZones;
  checkDelimiter();
}

template <>
void JSONReader::read(const RTTrP::RTTrPSpecificSettings& n)
{
  obj["Port"] = (int)n.port;
  obj["NumTrackables"] = n.numTrackables;
  obj["MaxLEDsPerTrackable"] = n.maxLEDsPerTrackable;
  obj["MaxZones"] = n.maxZones;
  obj["EnableQuaternion"] = n.enableQuaternion;
  obj["EnableEuler"] = n.enableEuler;
  obj["EnableVelocity"] = n.enableVelocity;
  obj["EnableAcceleration"] = n.enableAcceleration;
  obj["EnableZones"] = n.enableZones;
}

template <>
void JSONWriter::write(RTTrP::RTTrPSpecificSettings& n)
{
  n.port = (uint16_t)obj["Port"].toInt();
  n.numTrackables = obj["NumTrackables"].toInt();
  n.maxLEDsPerTrackable = obj["MaxLEDsPerTrackable"].toInt();
  n.maxZones = obj["MaxZones"].toInt();
  n.enableQuaternion = obj["EnableQuaternion"].toBool();
  n.enableEuler = obj["EnableEuler"].toBool();
  n.enableVelocity = obj["EnableVelocity"].toBool();
  n.enableAcceleration = obj["EnableAcceleration"].toBool();
  n.enableZones = obj["EnableZones"].toBool();
}
