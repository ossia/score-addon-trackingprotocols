#include "OpenTrackIOSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const OpenTrackIO::OpenTrackIOSpecificSettings& n)
{
  m_stream << n.multicastBaseAddress << n.port
           << n.minSourceNumber << n.maxSourceNumber
           << n.enableCamera << n.enableLens << n.enableTiming << n.enableGlobalStage
           << n.acceptJSON << n.acceptCBOR;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(OpenTrackIO::OpenTrackIOSpecificSettings& n)
{
  m_stream >> n.multicastBaseAddress >> n.port
           >> n.minSourceNumber >> n.maxSourceNumber
           >> n.enableCamera >> n.enableLens >> n.enableTiming >> n.enableGlobalStage
           >> n.acceptJSON >> n.acceptCBOR;
  checkDelimiter();
}

template <>
void JSONReader::read(const OpenTrackIO::OpenTrackIOSpecificSettings& n)
{
  obj["MulticastBaseAddress"] = n.multicastBaseAddress;
  obj["Port"] = (int)n.port;
  obj["MinSourceNumber"] = n.minSourceNumber;
  obj["MaxSourceNumber"] = n.maxSourceNumber;
  obj["EnableCamera"] = n.enableCamera;
  obj["EnableLens"] = n.enableLens;
  obj["EnableTiming"] = n.enableTiming;
  obj["EnableGlobalStage"] = n.enableGlobalStage;
  obj["AcceptJSON"] = n.acceptJSON;
  obj["AcceptCBOR"] = n.acceptCBOR;
}

template <>
void JSONWriter::write(OpenTrackIO::OpenTrackIOSpecificSettings& n)
{
  n.multicastBaseAddress = obj["MulticastBaseAddress"].toString();
  n.port = (uint16_t)obj["Port"].toInt();
  n.minSourceNumber = obj["MinSourceNumber"].toInt();
  n.maxSourceNumber = obj["MaxSourceNumber"].toInt();
  n.enableCamera = obj["EnableCamera"].toBool();
  n.enableLens = obj["EnableLens"].toBool();
  n.enableTiming = obj["EnableTiming"].toBool();
  n.enableGlobalStage = obj["EnableGlobalStage"].toBool();
  n.acceptJSON = obj["AcceptJSON"].toBool();
  n.acceptCBOR = obj["AcceptCBOR"].toBool();
}
