#include "PSNSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const PSN::PSNSpecificSettings& n)
{
  m_stream << n.multicastAddress << n.port << n.numTrackers
           << n.enableVelocity << n.enableAcceleration
           << n.enableOrientation << n.enableTargetPosition;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(PSN::PSNSpecificSettings& n)
{
  m_stream >> n.multicastAddress >> n.port >> n.numTrackers
           >> n.enableVelocity >> n.enableAcceleration
           >> n.enableOrientation >> n.enableTargetPosition;
  checkDelimiter();
}

template <>
void JSONReader::read(const PSN::PSNSpecificSettings& n)
{
  obj["MulticastAddress"] = n.multicastAddress;
  obj["Port"] = (int)n.port;
  obj["NumTrackers"] = n.numTrackers;
  obj["EnableVelocity"] = n.enableVelocity;
  obj["EnableAcceleration"] = n.enableAcceleration;
  obj["EnableOrientation"] = n.enableOrientation;
  obj["EnableTargetPosition"] = n.enableTargetPosition;
}

template <>
void JSONWriter::write(PSN::PSNSpecificSettings& n)
{
  n.multicastAddress = obj["MulticastAddress"].toString();
  n.port = (uint16_t)obj["Port"].toInt();
  n.numTrackers = obj["NumTrackers"].toInt();
  n.enableVelocity = obj["EnableVelocity"].toBool();
  n.enableAcceleration = obj["EnableAcceleration"].toBool();
  n.enableOrientation = obj["EnableOrientation"].toBool();
  n.enableTargetPosition = obj["EnableTargetPosition"].toBool();
}
