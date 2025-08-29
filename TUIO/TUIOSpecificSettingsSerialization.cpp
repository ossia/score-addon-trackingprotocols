#include "TUIOSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const TUIO::TUIOSpecificSettings& n)
{
  m_stream << n.port << n.numObjects << n.numCursors << n.numBlobs;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(TUIO::TUIOSpecificSettings& n)
{
  m_stream >> n.port >> n.numObjects >> n.numCursors >> n.numBlobs;
  checkDelimiter();
}

template <>
void JSONReader::read(const TUIO::TUIOSpecificSettings& n)
{
  obj["Port"] = (int)n.port;
  obj["NumObjects"] = n.numObjects;
  obj["NumCursors"] = n.numCursors;
  obj["NumBlobs"] = n.numBlobs;
}

template <>
void JSONWriter::write(TUIO::TUIOSpecificSettings& n)
{
  n.port = (uint16_t)obj["Port"].toInt();
  n.numObjects = obj["NumObjects"].toInt();
  n.numCursors = obj["NumCursors"].toInt();
  n.numBlobs = obj["NumBlobs"].toInt();
}