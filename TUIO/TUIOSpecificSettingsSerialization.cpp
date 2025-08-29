#include "TUIOSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const TUIO::TUIOSpecificSettings& n)
{
  m_stream << n.port;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(TUIO::TUIOSpecificSettings& n)
{
  m_stream >> n.port;
  checkDelimiter();
}

template <>
void JSONReader::read(const TUIO::TUIOSpecificSettings& n)
{
  obj["Port"] = (int)n.port;
}

template <>
void JSONWriter::write(TUIO::TUIOSpecificSettings& n)
{
  n.port = (uint16_t)obj["Port"].toInt();
}