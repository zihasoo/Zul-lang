
//SPDX-FileCopyrightText: © 2023 ByungYun Lee
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "ZulContext.h"

ZulContext::ZulContext() {
    local_var_map.reserve(50);
}

void ZulContext::remove_top_scope_vars() {
    //스코프 벗어날 때 생성한 변수들 맵에서 삭제 (IR코드에는 남아있음. 맵에서 지워서 접근만 막는 것)
    auto &cur_scope_vars = scope_stack.top();
    for (auto &name: cur_scope_vars) {
        local_var_map.erase(name);
    }
    scope_stack.pop();
}

bool ZulContext::var_exist(const std::string &name) {
    return global_var_map.contains(name) || local_var_map.contains(name);
}
