# Windows 坐标双栏 UI

## 基线与范围

基线：`first-year-ui-demo` / `aea57744aea7a79945cc10bbabd13d05403e8d29`。
开发分支：`feat/coordinate-console-ui`。仓库没有 `main`，PR 指向实际 UI 整合基线。
用户确认 UI 无需等待5号；不自动合并。

只替换显示层、旧 ConsoleUI 适配层、main 的输入结束检查，补充 CMake、测试和 UI 文档。
不改 Player、EventSystem、NPC、战斗、背包业务、存档或结局逻辑。
此基线仍是第一年试玩版，完成第一年即结束；本次没有合并其他剧情分支。

## 模块

- `ConsoleRenderer`：Win32 设备、测量、坐标输出、颜色、裁剪和最后一遍边框绘制。
- `TextLayout`：宽文本分段、换行、裁剪；UTF-8 仅在现有引擎接入边界转换。
- `GameUI`：只读界面快照、任务/状态/命令区、历史翻页、固定行输入编辑。
- `ConsoleUI`：保持旧主循环调用方式；现有 cout 输出被捕获到内存，不直接输出整张界面。

未添加 getHP/getSP 等重复接口：读取既有 getHealth/getStamina/getWisdom/getStrength/getReputation。
背包使用 getInventory().getItems().size() 显示“格数”，不是堆叠物品总数量。
当前任务来自主循环既有 guide 文本；没有创建第二套 Task 或修改任务判定。

## 坐标契约

`UI_WIDTH=110` 表示最后一列坐标，`UI_HEIGHT=30` 表示行数，`DIVIDER_X=73`。
Win32 从0计数，所以保留用户指定右边框 X=110 时需要111个列位置，而不是110个。

| 内容 | 坐标 |
| --- | --- |
| 左栏标题 | X=1..72，Y=1 |
| 左栏历史 | X=1..72，Y=3..24 |
| 中线 | X=73，Y=0..25 |
| 当前任务 | X=74..109，Y=1..7 |
| 玩家状态 | X=74..109，Y=9..19 |
| 快捷命令 | X=74..109，Y=21..24 |
| 输入分隔 | Y=25，X=0..110 |
| 输入行 | Y=27，X=1..109 |
| 最底边框 | Y=29 |
| 右边框 | X=110，Y=0..29 |

每帧先清除，按区域裁剪文字，再逐坐标绘制边框。没有 setw、按字符串长度计算中文宽度、
手工补空格对齐或拼接整张 ASCII 画面。清除单元格使用 FillConsoleOutputCharacterW。

Windows 中每个显示片段先写入不可见的控制台缓冲区，通过实际光标位移测量列数，再由
SetConsoleCursorPosition / WriteConsoleW / SetConsoleTextAttribute 写入可见界面。
不可见缓冲区继承当前字体；重新绘制时刷新测量缓存。Unicode 标量、常见组合附加符、
变体选择符、代理对和连接符保持为显示片段，不以字节数或代码单元数量对齐。
无效 UTF-8 转为替代符；控制字符不会执行光标移动。极长或无法测量的片段安全截断/替代。
便携测试使用确定性列宽设备，不能替代 Windows 字体/终端验收。

输入使用 ReadConsoleInputW，关闭系统回显和自动换行。输入最长1024个片段；支持左右、
Home/End、Backspace/Delete；长命令在输入行内横向滚动。Ctrl+C/Ctrl+Z 结束输入。
窗口不足时显示提示，忽略游戏命令，等待调整窗口；退出恢复控制台模式、颜色、代码页和光标。
任务过长时在自身区域换行并用省略号表示未显示部分，可用既有 guide 命令查看完整内容。

## 构建与自动回归

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\monkey_forest_first_year.exe
```

新自动测试 `ui_layout_tests` 覆盖：中文、英文、混排、数字、全角/半角标点、长剧情、
长任务、绿色智慧提示、背包显示、边框最后绘制、中央裁剪、长输入、组合字符编辑、翻页、
EOF、小窗口恢复、只读模型访问。测试只替换 OS 屏幕设备，运行真实布局、编辑器和适配代码。

## Windows 原生验收与截图

原生检查读取真实控制台边框单元格，不使用浏览器模拟图。请在附着控制台的窗口运行，
不要把输入/输出重定向到文件。它不加入普通 CTest，因为无控制台的 CI 无法满足条件。

```powershell
.\build\Debug\ui_windows_smoke.exe
.\build\Debug\ui_windows_smoke.exe --interactive
```

交互版依次显示上述10类内容，每个页面输入 Enter 继续。可在第9项智慧奖励和第10项背包
页面使用 Windows 截图工具保存真实运行图。还需手动检查：

- 输入/粘贴超过一行的中文与英文，编辑中 X=73 和 X=110 不移动。
- PageUp / PageDown 翻阅时保持已输入命令。
- 只改变窗口高度、改变字体/缩放、缩小到最低尺寸以下后恢复；不能执行不可见命令。
- 最后一行和最右侧写入不会使整个屏幕滚动；输入中文时 IME 行为正常。
- 智慧提示为绿色，错误为红色，标题青色，普通文本白色，提示黄色。

## 本次实际验证记录（2026-09-03）

- Linux GCC 13.3 C++17：全部游戏源码编译并链接成功。
- `ui_layout_tests`：通过。头文件缺失和越界裁剪用例均实际观察到修复前失败。
- 既有地图、2号剧情、4号玩家（16项）、5号状态/存档测试：通过。
- 既有3号测试：基线与本次均报 `Hertz should start`，没有修改其代码/测试。
  该测试自行实现的 WorldState 使用对象地址作全局状态键，未随局部对象销毁清理；
  不同用例复用栈地址时可能继承前例的 defeated flag。本次不扩大为战斗测试修复。
- 当前环境没有 CMake、Windows 编译器、Wine 或桌面会话。
  **未运行 Windows 编译和原生 smoke，未生成真实 Windows 截图。**
  提供验收入口不等于已通过验收；因此 PR 保持 Draft，不自动合并。
- `release/` 原有 exe/zip 未重新打包，本次 UI 需要从该分支源码重新构建。

API 依据：
[控制台坐标](https://learn.microsoft.com/en-us/windows/console/coord-str)、
[不可见屏幕缓冲区与字体继承](https://learn.microsoft.com/en-us/windows/console/createconsolescreenbuffer)、
[ReadConsoleInputW](https://learn.microsoft.com/en-us/windows/console/readconsoleinput)。
