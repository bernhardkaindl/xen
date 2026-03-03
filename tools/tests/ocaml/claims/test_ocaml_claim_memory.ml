(*
 * This test mirrors the C memory-claim coverage as far as the currently
 * available OCaml xenctrl bindings allow.
 *
 * Scope intentionally excludes xc_domain_claim_pages(), and focuses only on
 * Xenctrl.domain_claim_memory plus surrounding domain lifecycle behavior.
 *)

(* Claim unit used by the C test: 2MiB chunks (order 9 => 2^9 pages). *)
let claim_test_order = 9
(* Number of pages represented by claim_test_order. *)
let request_pages = Int64.shift_left 1L claim_test_order
(* Aggregate number of expectation failures. *)
let nr_failures = ref 0

(* Register one failure and print a human-readable message *)
let failf fmt =
  Printf.ksprintf
    (fun msg ->
      incr nr_failures;
      Printf.eprintf "test-ocaml-claim-memory failed: %s\n" msg)
    fmt

(* Emit a progress message to stdout, prefixed with the call-site line number.
 * Callers pass __LINE__ as the first argument, which OCaml expands at compile
 * time to the integer source line of that specific call. *)
let logf line fmt =
  Printf.ksprintf
    (fun msg -> Printf.printf "test-ocaml-claim-memory:%d: %s\n" line msg)
    fmt

let string_of_physinfo_cap_flag = function
  | Xenctrl.CAP_HVM                -> "HVM"
  | Xenctrl.CAP_PV                 -> "PV"
  | Xenctrl.CAP_DirectIO           -> "DirectIO"
  | Xenctrl.CAP_HAP                -> "HAP"
  | Xenctrl.CAP_Shadow             -> "Shadow"
  | Xenctrl.CAP_IOMMU_HAP_PT_SHARE -> "IOMMU_HAP_PT_SHARE"
  | Xenctrl.CAP_Vmtrace            -> "Vmtrace"
  | Xenctrl.CAP_Vpmu               -> "Vpmu"
  | Xenctrl.CAP_Gnttab_v1          -> "Gnttab_v1"
  | Xenctrl.CAP_Gnttab_v2          -> "Gnttab_v2"

let string_of_arch_physinfo_cap_flags = function
  | Xenctrl.ARM { Xenctrl.sve_vl } -> Printf.sprintf "ARM (sve_vl=%d)" sve_vl
  | Xenctrl.X86 flags              ->
      Printf.sprintf "X86 (%d cap flags)" (List.length flags)

(*
 * Pretty-print all fields of a physinfo record.
 *
 * Kept as a standalone function so it can be called without altering the
 * surrounding test flow.
 *)
let log_physinfo p =
  logf __LINE__ "physinfo:";
  logf __LINE__ "  threads_per_core  : %d" p.Xenctrl.threads_per_core;
  logf __LINE__ "  cores_per_socket  : %d" p.Xenctrl.cores_per_socket;
  logf __LINE__ "  nr_cpus           : %d" p.Xenctrl.nr_cpus;
  logf __LINE__ "  max_nr_cpus       : %d" p.Xenctrl.max_nr_cpus;
  logf __LINE__ "  max_node_id       : %d" p.Xenctrl.max_node_id;
  logf __LINE__ "  cpu_khz           : %d" p.Xenctrl.cpu_khz;
  logf __LINE__ "  total_pages       : %nd" p.Xenctrl.total_pages;
  logf __LINE__ "  free_pages        : %nd" p.Xenctrl.free_pages;
  logf __LINE__ "  scrub_pages       : %nd" p.Xenctrl.scrub_pages;
  logf __LINE__ "  capabilities      : [%s]"
    (String.concat ", "
       (List.map string_of_physinfo_cap_flag p.Xenctrl.capabilities));
  logf __LINE__ "  arch_capabilities : %s"
    (string_of_arch_physinfo_cap_flags p.Xenctrl.arch_capabilities)

(* Call Xenctrl.physinfo, log the result, and return it. *)
let get_and_log_physinfo xch =
  let p = Xenctrl.physinfo xch in
  log_physinfo p;
  p

(*
 * Query and log only the memory counters, for lightweight before/after
 * snapshots around each test case.
 *)
let log_free_pages xch label =
  let p = Xenctrl.physinfo xch in
  logf __LINE__ "%s: free_pages=%nd scrub_pages=%nd" label
    p.Xenctrl.free_pages p.Xenctrl.scrub_pages

(* Expect Invalid_argument for input validation done in the OCaml stub layer. *)
let expect_invalid_argument name thunk =
  logf __LINE__ "checking Invalid_argument: %s" name;
  try
    thunk ();
    failf "%s did not raise Invalid_argument" name
  with
  | Invalid_argument _ ->
      ()
  | Xenctrl.Error msg ->
      failf "%s raised Xenctrl.Error instead of Invalid_argument: %s" name msg
  | exn ->
      failf "%s raised unexpected exception: %s" name (Printexc.to_string exn)

(* Expect Xenctrl.Error for failures returned from libxc/hypervisor calls. *)
let expect_xc_error name thunk =
  logf __LINE__ "checking Xenctrl.Error: %s" name;
  try
    thunk ();
    failf "%s did not raise Xenctrl.Error" name
  with
  | Xenctrl.Error _ ->
      ()
  | Invalid_argument msg ->
      failf "%s raised Invalid_argument instead of Xenctrl.Error: %s" name msg
  | exn ->
      failf "%s raised unexpected exception: %s" name (Printexc.to_string exn)

(* Small helper used to adapt domain creation flags to host capabilities. *)
let has_capability cap physinfo =
  List.exists (( = ) cap) physinfo.Xenctrl.capabilities

(*
 * Build a minimal domain config for a short-lived test DomU.
 *
 * The record is passed directly to Xenctrl.domain_create and controls every
 * attribute the hypervisor needs to instantiate the domain.  We keep all
 * resource limits at their minimum viable values because the domain is only
 * used to hold or exercise memory claims, not to run a guest OS.
 *
 * Preference order for flags:
 *   HVM + HAP    (hardware-assisted paging, preferred)
 *   HVM + Shadow (software shadow page-tables, fallback)
 *   PV           (para-virtual, last resort when HVM is unavailable)
 *)
let make_domain_config physinfo =
  let flags = ref [ Xenctrl.CDF_HVM; Xenctrl.CDF_HAP ] in
  let emulation_flags = ref [ Xenctrl.X86_EMU_LAPIC ] in

  (* Drop HAP if the host does not support hardware-assisted paging. *)
  if not (has_capability Xenctrl.CAP_HAP physinfo) then
    flags := List.filter (( <> ) Xenctrl.CDF_HAP) !flags;

  (*
   * Fall back to a PV-style config when HVM is unavailable outright, or when
   * neither HAP nor Shadow paging is available (HVM without any paging mode
   * is not useful and the hypervisor would reject it).  In the PV case we
   * also clear emulation_flags because device emulation is HVM-only.
   *)
  if
    (not (has_capability Xenctrl.CAP_HVM physinfo))
    ||
    not
      (has_capability Xenctrl.CAP_HAP physinfo
      || has_capability Xenctrl.CAP_Shadow physinfo)
  then begin
    flags := List.filter (( <> ) Xenctrl.CDF_HVM) !flags;
    emulation_flags := []
  end;

  let string_of_flags fs =
    "[" ^ String.concat ", "
            (List.map (function
               | Xenctrl.CDF_HVM -> "HVM"
               | Xenctrl.CDF_HAP -> "HAP"
               | Xenctrl.CDF_S3_INTEGRITY -> "S3_INTEGRITY"
               | Xenctrl.CDF_OOS_OFF -> "OOS_OFF"
               | Xenctrl.CDF_XS_DOMAIN -> "XS_DOMAIN"
               | Xenctrl.CDF_IOMMU -> "IOMMU"
               | Xenctrl.CDF_NESTED_VIRT -> "NESTED_VIRT"
               | Xenctrl.CDF_VPMU -> "VPMU"
               | Xenctrl.CDF_TRAP_UNMAPPED_ACCESSES -> "TRAP_UNMAPPED_ACCESSES")
             fs)
    ^ "]"
  in
  let string_of_emulation_flags fs =
    "[" ^ String.concat ", "
            (List.map (function
               | Xenctrl.X86_EMU_LAPIC    -> "LAPIC"
               | Xenctrl.X86_EMU_HPET     -> "HPET"
               | Xenctrl.X86_EMU_PM       -> "PM"
               | Xenctrl.X86_EMU_RTC      -> "RTC"
               | Xenctrl.X86_EMU_IOAPIC   -> "IOAPIC"
               | Xenctrl.X86_EMU_PIC      -> "PIC"
               | Xenctrl.X86_EMU_VGA      -> "VGA"
               | Xenctrl.X86_EMU_IOMMU    -> "IOMMU"
               | Xenctrl.X86_EMU_PIT      -> "PIT"
               | Xenctrl.X86_EMU_USE_PIRQ -> "USE_PIRQ"
               | Xenctrl.X86_EMU_VPCI     -> "VPCI")
             fs)
    ^ "]"
  in
  logf __LINE__ "domain config: flags=%s emulation_flags=%s"
    (string_of_flags !flags)
    (string_of_emulation_flags !emulation_flags);

  {
    (* Flask/XSM security label; 0 = default unlabelled context. *)
    Xenctrl.ssidref = 0l;
    (* Domain UUID; all-zeros is valid and sufficient for a test domain. *)
    handle = "00000000-0000-0000-0000-000000000000";
    (* HVM and/or HAP as resolved above; may be [] for a PV domain. *)
    flags = !flags;
    (* IOMMU sharing options; empty = hypervisor default (share page tables). *)
    iommu_opts = [];
    (* One vCPU is the minimum; the test never schedules any guest code. *)
    max_vcpus = 1;
    (* 0 lets the hypervisor choose the event-channel port limit. *)
    max_evtchn_port = 0;
    (* Minimum grant-table size; 1 frame is required to create the table. *)
    max_grant_frames = 1;
    (* 0 lets the hypervisor choose the grant map-tracking frame limit. *)
    max_maptrack_frames = 0;
    (* Grant table protocol version; legacy format; universally supported. *)
    max_grant_version = 1;
    (* Alternate-p2m options for VM introspection; 0 = feature disabled. *)
    altp2m_opts = 0l;
    (* Number of alternate p2m views to pre-allocate; 0 = none. *)
    altp2m_count = 0l;
    (* Per-vCPU vmtrace ring-buffer size in KiB; 0 = vmtrace disabled. *)
    vmtrace_buf_kb = 0l;
    (* CPU pool assignment; 0 = default pool (Pool-0). *)
    cpupool_id = 0l;
    (*
     * x86 architecture config:
     *   emulation_flags - devices the hypervisor should emulate for this
     *     guest; LAPIC is the minimum needed to boot any HVM domain.
     *     Empty for PV guests where device emulation is irrelevant.
     *   misc_flags - X86_MSR_RELAXED and similar tweaks; none needed here.
     *)
    arch = Xenctrl.X86 { emulation_flags = !emulation_flags; misc_flags = [] };
  }

(*
 * Domain lifecycle wrapper used by all integration-style test sections.
 *
 * We always attempt cleanup, and ignore cleanup failures in order to preserve
 * the primary test failure signal.
 *)
let with_domain xch config f =
  logf __LINE__ "domain_create";
  let domid = Xenctrl.domain_create xch config in
  logf __LINE__ "domain_create -> domid=%d" domid;
  let cleanup () =
    logf __LINE__ "domain_destroy domid=%d" domid;
    try Xenctrl.domain_destroy xch domid with
    | Xenctrl.Error _ ->
        ()
  in
  try
    logf __LINE__ "domain_setmaxmem domid=%d maxmem=unlimited" domid;
    Xenctrl.domain_setmaxmem xch domid (-1L);
    let result = f domid in
    cleanup ();
    result
  with
  | exn ->
      cleanup ();
      raise exn

(*
 * Validate domain_claim_memory argument/error behavior.
 *
 * This combines:
 * - pure stub input validation (Invalid_argument), and
 * - backend/libxc rejections (Xenctrl.Error).
 *)
let run_stub_validation_tests xch physinfo config =
  logf __LINE__ "running stub validation tests";
  try
    expect_invalid_argument "negative pages" (fun () ->
        Xenctrl.domain_claim_memory xch 0
          [| { Xenctrl.pages = -1L; node = -1l } |]);

    expect_invalid_argument "node < -1" (fun () ->
        Xenctrl.domain_claim_memory xch 0
          [| { Xenctrl.pages = 0L; node = -2l } |]);

    expect_invalid_argument "too many claims" (fun () ->
        Xenctrl.domain_claim_memory xch 0
          (Array.make 256 { Xenctrl.pages = 0L; node = -1l }));

    with_domain xch config (fun domid ->

        (* Backend rejects empty claim array (maps to nr_claims == 0). *)
        expect_xc_error "nr_claims == 0" (fun () ->
            Xenctrl.domain_claim_memory xch domid [||]);

        (* Current hypercall implementation rejects nr_claims > 1. *)
        expect_xc_error "nr_claims == 2" (fun () ->
            Xenctrl.domain_claim_memory xch domid
              [|
                { Xenctrl.pages = 1L; node = -1l };
                { Xenctrl.pages = 1L; node = -1l };
              |]);

        (* Overflow-style check aligned with the C coverage. *)
        expect_xc_error "pages > Int32.max" (fun () ->
            Xenctrl.domain_claim_memory xch domid
              [|
                {
                  Xenctrl.pages = Int64.add (Int64.of_int32 Int32.max_int) 1L;
                  node = 0l;
                };
              |]);

        (* Invalid node index: one beyond the reported node range. *)
        let invalid_node = Int32.of_int (physinfo.Xenctrl.max_node_id + 1) in
        expect_xc_error "invalid node" (fun () ->
            Xenctrl.domain_claim_memory xch domid
              [| { Xenctrl.pages = 1L; node = invalid_node } |]))
  with
  | Xenctrl.Error msg ->
      failf "stub validation tests raised Xenctrl.Error: %s" msg
  | Invalid_argument msg ->
      failf "stub validation tests raised Invalid_argument: %s" msg
  | exn ->
      failf "stub validation tests raised unexpected exception: %s"
        (Printexc.to_string exn)

(*
 * Host-wide claim behavior check:
 * 1) claim almost all free pages for one domain,
 * 2) verify allocation in another domain fails,
 * 3) verify claimant can consume part of its claim.
 *)
let run_host_wide_claim_test xch config =
  logf __LINE__ "running host-wide claim test";
  (*
   * Snapshot free pages before domain creation as a pre-flight check only.
   * The actual claim is computed inside with_domain after the domain exists,
   * because creating the domain itself consumes pages; computing the claim
   * from the pre-creation count would produce a value larger than what is
   * actually available and cause domain_claim_memory to fail with ENOMEM.
   *)
  let free_pages_pre =
    Int64.of_nativeint (get_and_log_physinfo xch).Xenctrl.free_pages
  in
  (* Need one free page so the test can validate claim enforcement semantics. *)
  if Int64.compare free_pages_pre (Int64.add request_pages 1L) <= 0 then
    failf
      "host-wide claim test: need more than %Ld free pages (have %Ld)"
      (Int64.add request_pages 1L) free_pages_pre
  else
    (try
      with_domain xch config (fun domid ->

          (*
           * Re-read free pages now that the claimant domain exists.
           * Claim all but (request_pages - 1) of them so the second domain's
           * request_pages-sized reservation attempt must fail.
           *)
          let free_pages =
            Int64.of_nativeint (Xenctrl.physinfo xch).Xenctrl.free_pages
          in
          let claim = Int64.sub (Int64.add free_pages 1L) request_pages in

          logf __LINE__ "domain_claim_memory domid=%d pages=%Ld node=any"
            domid claim;
          Xenctrl.domain_claim_memory xch domid
            [| { Xenctrl.pages = claim; node = -1l } |];

          with_domain xch config (fun other_domid ->

              (*
               * This reservation is expected to fail while the first domain
               * holds the host-wide claim.
               *)
              expect_xc_error "host-wide claim blocks second-domain allocation"
                (fun () ->
                  logf __LINE__
                    "domain_memory_increase_reservation domid=%d"
                    other_domid;
                  Xenctrl.domain_memory_increase_reservation xch other_domid
                    (Xenctrl.pages_to_kib request_pages)));

          (*
           * The claimant domain should still be able to consume pages from
           * its own claim.
           *)
          logf __LINE__ "domain_memory_increase_reservation domid=%d" domid;
          Xenctrl.domain_memory_increase_reservation xch domid
            (Xenctrl.pages_to_kib request_pages))
    with
    | Xenctrl.Error msg ->
        failf "host-wide claim test raised Xenctrl.Error: %s" msg
    | Invalid_argument msg ->
        failf "host-wide claim test raised Invalid_argument: %s" msg
    | exn ->
        failf "host-wide claim test raised unexpected exception: %s"
          (Printexc.to_string exn))

(*
 * Node-specific claim path.
 *
 * OCaml xenctrl bindings currently do not expose the full set of NUMA-aware
 * helpers used in the C test; this validates the node-parameterized claim API
 * itself and subsequent reservation in the same domain.
 *)
let run_node_claim_test xch config =
  logf __LINE__ "running node claim test";
  try
    with_domain xch config (fun domid ->
      (*
       * Claim request_pages on node 0 so the subsequent reservation, which
       * also requests request_pages, fits within the outstanding claim.
       * Claiming only 1 page here would cause
       * domain_memory_increase_reservation to fail because the claim
       * would be exhausted after the first page.
       *)
      let node = 0l in
      logf __LINE__ "domain_claim_memory domid=%d pages=%Ld node=%ld"
        domid request_pages node;
      Xenctrl.domain_claim_memory xch domid
        [| { Xenctrl.pages = request_pages; node } |];
      logf __LINE__ "domain_memory_increase_reservation domid=%d" domid;
      Xenctrl.domain_memory_increase_reservation xch domid
        (Xenctrl.pages_to_kib request_pages))
  with
  | Xenctrl.Error msg ->
      failf "node claim test raised Xenctrl.Error: %s" msg
  | Invalid_argument msg ->
      failf "node claim test raised Invalid_argument: %s" msg
  | exn ->
      failf "node claim test raised unexpected exception: %s"
        (Printexc.to_string exn)

(* Top-level test dispatcher *)
let run_claim_tests xch =
  let physinfo = get_and_log_physinfo xch in
  let config = make_domain_config physinfo in

  log_free_pages xch "before stub validation tests";
  run_stub_validation_tests xch physinfo config;
  log_free_pages xch "after stub validation tests";

  log_free_pages xch "before host-wide claim test";
  run_host_wide_claim_test xch config;
  log_free_pages xch "after host-wide claim test";

  log_free_pages xch "before node claim test";
  run_node_claim_test xch config;
  log_free_pages xch "after node claim test";

  if !nr_failures = 0 then
    Printf.printf "test-ocaml-claim-memory passed\n"
  else
    exit 1

(*
 * Entry point:
 * - run the test using Xenctrl.with_intf,
 * - treat environment/setup issues as a skipped test, matching existing
 *   in-tree OCaml test behavior.
 *)
let () =
  try Xenctrl.with_intf run_claim_tests with
  | Xenctrl.Error msg ->
      Printf.printf "test-ocaml-claim-memory skipped: %s\n" msg
  | Failure msg ->
      Printf.printf "test-ocaml-claim-memory skipped: %s\n" msg
  | Unix.Unix_error (err, fn, arg) ->
      Printf.printf "test-ocaml-claim-memory skipped: %s (%s %s)\n"
        (Unix.error_message err) fn arg
