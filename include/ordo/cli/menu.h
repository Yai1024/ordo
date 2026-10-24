#pragma once

// 文本交互菜单（OUTLINE §7.6）：与命令行共用同一套 Command，只多了"逐项提问"。

#include "ordo/cli/dispatcher.h"

namespace ordo {

class Menu {
public:
    explicit Menu(Dispatcher& dispatcher);

    // 返回进程退出码；输入流结束（Ctrl+Z / EOF）时正常退出。
    int run(AppContext& ctx);

private:
    bool handle_add(AppContext& ctx);
    bool handle_list(AppContext& ctx);
    bool handle_done(AppContext& ctx);
    bool handle_edit(AppContext& ctx);
    bool handle_remove(AppContext& ctx);
    bool handle_stats(AppContext& ctx);

    Dispatcher& dispatcher_;
};

}  // namespace ordo
