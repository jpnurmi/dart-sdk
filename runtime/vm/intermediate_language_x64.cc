// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/globals.h"  // Needed here to get TARGET_ARCH_X64.
#if defined(TARGET_ARCH_X64)

#include "vm/flow_graph_compiler.h"
#include "vm/locations.h"
#include "vm/stub_code.h"

#define __ compiler->assembler()->

namespace dart {


// True iff. the arguments to a call will be properly pushed and can
// be popped after the call.
template <typename T> static bool VerifyCallComputation(T* comp) {
  // Argument values should be consecutive temps.
  //
  // TODO(kmillikin): implement stack height tracking so we can also assert
  // they are on top of the stack.
  intptr_t previous = -1;
  for (int i = 0; i < comp->ArgumentCount(); ++i) {
    Value* val = comp->ArgumentAt(i);
    if (!val->IsUse()) return false;
    intptr_t current = val->AsUse()->definition()->temp_index();
    if (i != 0) {
      if (current != (previous + 1)) return false;
    }
    previous = current;
  }
  return true;
}


static LocationSummary* MakeSimpleLocationSummary(
    intptr_t input_count, Location out) {
  LocationSummary* summary = new LocationSummary(input_count);
  for (intptr_t i = 0; i < input_count; i++) {
    summary->set_in(i, Location::RequiresRegister());
  }
  summary->set_out(out);
  return summary;
}


LocationSummary* CurrentContextComp::MakeLocationSummary() {
  return MakeSimpleLocationSummary(0, Location::RequiresRegister());
}


void CurrentContextComp::EmitNativeCode(FlowGraphCompiler* compiler) {
  __ movq(locs()->out().reg(), CTX);
}


LocationSummary* StoreContextComp::MakeLocationSummary() {
  LocationSummary* summary = new LocationSummary(1);
  summary->set_in(0, Location::RegisterLocation(CTX));
  return summary;
}


void StoreContextComp::EmitNativeCode(FlowGraphCompiler* compiler) {
  // Nothing to do.  Context register were loaded by register allocator.
  ASSERT(locs()->in(0).reg() == CTX);
}


LocationSummary* StrictCompareComp::MakeLocationSummary() {
  return MakeSimpleLocationSummary(2, Location::SameAsFirstInput());
}


void StrictCompareComp::EmitNativeCode(FlowGraphCompiler* compiler) {
  const Bool& bool_true = Bool::ZoneHandle(Bool::True());
  const Bool& bool_false = Bool::ZoneHandle(Bool::False());

  Register left = locs()->in(0).reg();
  Register right = locs()->in(1).reg();
  Register result = locs()->out().reg();

  __ cmpq(left, right);
  Label load_true, done;
  if (kind() == Token::kEQ_STRICT) {
    __ j(EQUAL, &load_true, Assembler::kNearJump);
  } else {
    __ j(NOT_EQUAL, &load_true, Assembler::kNearJump);
  }
  __ LoadObject(result, bool_false);
  __ jmp(&done, Assembler::kNearJump);
  __ Bind(&load_true);
  __ LoadObject(result, bool_true);
  __ Bind(&done);
}


// Generic summary for call instructions that have all arguments pushed
// on the stack and return the result in a fixed register RAX.
static LocationSummary* MakeCallSummary() {
  LocationSummary* result = new LocationSummary(0);
  result->set_out(Location::RegisterLocation(RAX));
  return result;
}


LocationSummary* ClosureCallComp::MakeLocationSummary() {
  return MakeCallSummary();
}


void ClosureCallComp::EmitNativeCode(FlowGraphCompiler* compiler) {
  ASSERT(VerifyCallComputation(this));
  // The arguments to the stub include the closure.  The arguments
  // descriptor describes the closure's arguments (and so does not include
  // the closure).
  int argument_count = ArgumentCount();
  const Array& arguments_descriptor =
      CodeGenerator::ArgumentsDescriptor(argument_count - 1,
                                         argument_names());
  __ LoadObject(R10, arguments_descriptor);

  compiler->GenerateCall(token_index(),
                         try_index(),
                         &StubCode::CallClosureFunctionLabel(),
                         PcDescriptors::kOther);
  __ Drop(argument_count);
}


LocationSummary* InstanceCallComp::MakeLocationSummary() {
  return MakeCallSummary();
}


void InstanceCallComp::EmitNativeCode(FlowGraphCompiler* compiler) {
  ASSERT(VerifyCallComputation(this));
  compiler->EmitInstanceCall(cid(),
                             token_index(),
                             try_index(),
                             function_name(),
                             ArgumentCount(),
                             argument_names(),
                             checked_argument_count());
}


LocationSummary* StaticCallComp::MakeLocationSummary() {
  return MakeCallSummary();
}


void StaticCallComp::EmitNativeCode(FlowGraphCompiler* compiler) {
  ASSERT(VerifyCallComputation(this));
  compiler->EmitStaticCall(token_index(),
                           try_index(),
                           function(),
                           ArgumentCount(),
                           argument_names());
}


void BindInstr::EmitNativeCode(FlowGraphCompiler* compiler) {
  computation()->EmitNativeCode(compiler);
  __ pushq(locs()->out().reg());
}


}  // namespace dart

#undef __

#endif  // defined TARGET_ARCH_X64
