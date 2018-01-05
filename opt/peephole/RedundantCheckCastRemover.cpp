/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "RedundantCheckCastRemover.h"

#include <unordered_map>

#include "DexUtil.h"
#include "IRCode.h"
#include "Match.h"
#include "Walkers.h"

RedundantCheckCastRemover::RedundantCheckCastRemover(
    PassManager& mgr, const std::vector<DexClass*>& scope)
    : m_mgr(mgr), m_scope(scope) {}

void remove_instructions(
    const std::unordered_map<DexMethod*, std::vector<IRInstruction*>>&
        to_remove) {
  for (auto& method_instrs : to_remove) {
    DexMethod* method = method_instrs.first;
    const auto& instrs = method_instrs.second;
    auto code = method->get_code();
    for (auto instr : instrs) {
      code->remove_opcode(instr);
    }
  }
}

void RedundantCheckCastRemover::run() {
  auto match = std::make_tuple(m::invoke(),
                               m::is_opcode(OPCODE_MOVE_RESULT_OBJECT),
                               m::is_opcode(OPCODE_CHECK_CAST),
                               m::is_opcode(IOPCODE_MOVE_RESULT_PSEUDO_OBJECT));

  std::unordered_map<DexMethod*, std::vector<IRInstruction*>> to_remove;

  auto& mgr =
      this->m_mgr; // so the lambda doesn't have to capture all of `this`
  walk::matching_opcodes_in_block(
      m_scope,
      match,
      [&mgr, &to_remove](const DexMethod* method,
                         const std::vector<IRInstruction*>& insns) {
        if (RedundantCheckCastRemover::can_remove_check_cast(insns)) {
          to_remove[const_cast<DexMethod*>(method)].push_back(insns[2]);
          mgr.incr_metric("redundant_check_casts_removed", 1);

          TRACE(PEEPHOLE, 8, "found redundant check cast\n");
          for (IRInstruction* insn : insns) {
            TRACE(PEEPHOLE, 8, "%s\n", SHOW(insn));
          }
        }
      });

  remove_instructions(to_remove);
}

bool RedundantCheckCastRemover::can_remove_check_cast(
    const std::vector<IRInstruction*>& insns) {
  always_assert(insns.size() == 4);
  IRInstruction* invoke_op = insns[0];
  IRInstruction* move_result_op = insns[1];
  IRInstruction* check_cast_op = insns[2];
  IRInstruction* move_result_pseudo = insns[3];

  auto invoke_return = invoke_op->get_method()->get_proto()->get_rtype();
  auto check_type = check_cast_op->get_type();
  return move_result_op->dest() == check_cast_op->src(0) &&
         move_result_pseudo->dest() == check_cast_op->src(0) &&
         check_cast(invoke_return, check_type);
}
