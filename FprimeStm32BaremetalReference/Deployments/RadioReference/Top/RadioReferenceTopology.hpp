#ifndef RADIOREFERENCE_RADIOREFERENCETOPOLOGY_HPP
#define RADIOREFERENCE_RADIOREFERENCETOPOLOGY_HPP

#include <FprimeStm32BaremetalReference/Deployments/RadioReference/Top/RadioReferenceTopologyDefs.hpp>

namespace Deployments {

void setupTopology(const TopologyState& state);
void teardownTopology(const TopologyState& state);

}  // namespace Deployments

#endif
