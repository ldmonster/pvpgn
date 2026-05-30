// =====================================================================
// auth.cpp — Thin coordinator: includes all auth sub-modules
// Refactored from monolithic auth.cpp (plan 15 §3 / SOLID-S)
//
// Original 1758-LOC file split into focused units:
//   auth_account.cpp  — account creation (createaccountw3, createacctreq1/2)
//   auth_password.cpp — password change (changepassreq, passchangereq/proof)
//   auth_login.cpp    — login flow (loginreq1/2/w3, logonproofreq)
//   auth_cdkey.cpp    — CD-key validation (cdkey, cdkey2, cdkey3)
//   auth_version.cpp  — version check + misc (authreq1/109, iconreq, echo, ping)
// =====================================================================

// All implementations live in the sub-files below.
// This file is intentionally empty of logic — it exists only so that
// the CMakeLists.txt entry `src/handle_bnet/auth.cpp` continues to
// compile (as an empty translation unit) while the new files are
// added alongside it.  Once the CMakeLists.txt is updated to list the
// new files directly, this file can be removed.
