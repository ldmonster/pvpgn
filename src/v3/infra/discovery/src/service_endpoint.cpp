#include "infra/discovery/service_endpoint.hpp"

namespace pvpgn::infra::discovery {

std::string ServiceEndpoint::address() const {
    return host + ":" + std::to_string(port);
}

} // namespace pvpgn::infra::discovery
