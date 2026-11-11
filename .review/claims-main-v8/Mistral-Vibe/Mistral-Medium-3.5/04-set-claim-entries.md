# 04: xen/mm: add domain_set_claims() for new claim requests

## Review report

### xen/include/public/domctl.h

**Constant definition**

`XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST` defined as `0x80000000U`
- Correctly placed outside struct declaration and well-documented
- Reserves bit 31 for special targets

### xen/common/page_alloc.c

**domain_validate_claim_request()**
- Validates all entries
- Checks for duplicates
- Bounds checks against max_pages
- Rejects invalid targets
- Returns appropriate error codes

**check_memory_for_claim_request()**
- Checks host-wide capacity
- Checks per-node capacity
- Protected by heap_lock

**domain_install_claims()**
- Installs all claims atomically
- Updates all counters
- Protected by heap_lock
- Returns old claims array for caller to free

**domain_set_claims()**
- Main entry point
- Coordinates validation and installation
- Handles memory allocation
- Proper error handling

## Summary

**Status: ✅ APPROVED**

All changes in this commit are technically correct and well-structured. No issues found.
