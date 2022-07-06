/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "BranchPrefixHoisting.h"
#include "RedexTest.h"
#include "Show.h"

class BranchPrefixWhenTest : public RedexIntegrationTest {};

TEST_F(BranchPrefixWhenTest, switch_test) {
  auto scope = build_class_scope(stores);

  auto meth =
      DexMethod::get_method("Lcom/facebook/redextest/Foo;.bar_packed:(I)V");
  auto code = meth->as_def()->get_code();
  code->build_cfg(true);
  type_inference::TypeInference type_inference(code->cfg());
  type_inference.run(meth->as_def());
  constant_uses::ConstantUses constant_uses(code->cfg(), meth->as_def());
  auto v = BranchPrefixHoistingPass::process_cfg(code->cfg(), type_inference,
                                                 constant_uses);
  // All the cases in the switch precedes with System.out.print, including the
  // default path. Hence, all the paths has the following two instructions in
  // the prefix:
  // SGET_OBJECT Ljava/lang/System;.out:Ljava/io/PrintStream;
  // IOPCODE_MOVE_RESULT_PSEUDO_OBJECT v0
  EXPECT_EQ(v, 2);
}
