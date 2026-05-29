// SPDX-License-Identifier: GPL-2.0-or-later
// Compile-time sanity checks for `init_conn_dispatch`. The
// dispatcher is constexpr/header-only; this TU exists so the
// `application_init` library has a translation unit to compile and
// link, and so that any drift in the dispatch table is caught at
// build time rather than at runtime.

#include "application/init/init_conn_dispatch.hpp"

namespace pvpgn::application::init {

namespace {

using protocol::bnet::init::kClassBnet;
using protocol::bnet::init::kClassBot;
using protocol::bnet::init::kClassD2csBnetd;
using protocol::bnet::init::kClassEnc;
using protocol::bnet::init::kClassFile;
using protocol::bnet::init::kClassLocalMachine;
using protocol::bnet::init::kClassTelnet;

static_assert(dispatch_init_conn({kClassBnet}).decision
              == InitDecision::kBnet);
static_assert(dispatch_init_conn({kClassFile}).decision
              == InitDecision::kFile);
static_assert(dispatch_init_conn({kClassBot}).decision
              == InitDecision::kBot);
static_assert(dispatch_init_conn({kClassTelnet}).decision
              == InitDecision::kTelnet);
static_assert(dispatch_init_conn({kClassD2csBnetd}).decision
              == InitDecision::kD2csBnetd);
static_assert(dispatch_init_conn({kClassEnc}).decision
              == InitDecision::kRejected);
static_assert(dispatch_init_conn({kClassLocalMachine}).decision
              == InitDecision::kRejected);
static_assert(dispatch_init_conn({0x00}).decision
              == InitDecision::kRejected);
static_assert(dispatch_init_conn({0xff}).decision
              == InitDecision::kRejected);

}  // namespace

}  // namespace pvpgn::application::init
