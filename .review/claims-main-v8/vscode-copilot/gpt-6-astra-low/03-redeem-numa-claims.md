# Redeem per-node claims for allocations

## Failed assignment does not restore a redeemed claim

**The suspicion.** A page-assignment failure can return pages to the heap after their claim has been redeemed.

**Disposition.** Redemption precedes assignment, and freeing the pages does not replenish the reservation.
The supported domain-build contract relies on aborting the build after such a failure.

**Targeted mitigation.** Do not treat allocation retry after failed assignment as preserving the original claim.
Keep the build-abort requirement explicit.
