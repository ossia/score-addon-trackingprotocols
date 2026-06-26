#pragma once

#include "OpenXRProtocol.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/generic/generic_device.hpp>

namespace OpenXR
{

class OpenXRDevice final : public ossia::net::generic_device
{
public:
  OpenXRDevice(
      std::unique_ptr<ossia::net::protocol_base> protocol,
      const std::string& name)
      : generic_device{std::move(protocol), name}
  {
  }

  ~OpenXRDevice() = default;

  const OpenXRProtocol& get_protocol() const
  {
    return static_cast<const OpenXRProtocol&>(generic_device::get_protocol());
  }

  OpenXRProtocol& get_protocol()
  {
    return static_cast<OpenXRProtocol&>(generic_device::get_protocol());
  }
};

}
