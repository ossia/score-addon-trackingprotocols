#include "OpenXRSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const OpenXR::OpenXRSpecificSettings& n)
{
  m_stream << n.enableHandTracking << n.enableControllers << n.pollRateMs;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(OpenXR::OpenXRSpecificSettings& n)
{
  m_stream >> n.enableHandTracking >> n.enableControllers >> n.pollRateMs;
  checkDelimiter();
}

template <>
void JSONReader::read(const OpenXR::OpenXRSpecificSettings& n)
{
  obj["EnableHandTracking"] = n.enableHandTracking;
  obj["EnableControllers"] = n.enableControllers;
  obj["PollRateMs"] = n.pollRateMs;
}

template <>
void JSONWriter::write(OpenXR::OpenXRSpecificSettings& n)
{
  n.enableHandTracking = obj["EnableHandTracking"].toBool();
  n.enableControllers = obj["EnableControllers"].toBool();
  n.pollRateMs = obj["PollRateMs"].toInt();
}
