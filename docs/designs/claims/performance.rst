.. SPDX-License-Identifier: CC-BY-4.0

Performance
***********

These product observations concern an earlier single-node implementation and
its surrounding toolstack integration, not a benchmark of the v8 multi-node
API. They do not isolate claims from other placement changes or establish a
performance guarantee for this series.

The single-node claims implementation which is the basis of the
NUMA claims v4 series and the multi-node claim sets design forms
the groundwork for the NUMA design and implementation in XenServer 9.

An early version of it is available as the XenServer XS9 preview
release: https://www.xenserver.com/downloads/xs9-preview.
The performance of this release has been tested in real
customer environments with customer workloads.

On dual-socket Intel servers, the **average aggregate CPU usage across
all VMs at peak times** (peak user load) was **~16% less** than with
`XenServer 8.4` (overall average at all times **~8.5% less**) compared
to the previous release, which is a significant improvement in CPU
efficiency for memory-intensive workloads, attributed to the
improved NUMA placement enabled by `NUMA-aware claims`.

The customer's response time metric from their application, which is the
key measure the customer uses for end user observed performance, showed
an ~8% improvement, matching the improvement in average CPU usage.

These numbers were observed using `Intel dual-socket servers`.
The performance benefits with AMD servers (judging by preliminary tests) are
expected to be considerably higher than the results with dual-socket Intel
servers.

The multi-node claim sets design is expected to extend these benefits
to configurations that require claiming memory from multiple NUMA nodes
adjacent to each other for optimal performance.
