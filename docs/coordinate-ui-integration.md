# Windows 坐标双栏 UI

## 范围

只做显示层：`ConsoleRenderer`、`TextLayout`、`GameUI`、`InteractiveGameUI`，以及旧的
`ConsoleUI` 适配层和 `main` 里的输入结束检查。不改 Player、EventSystem、NPC、战斗、
背包业务、存档或结局逻辑。

当前是第一年试玩版，完成第一年后结束，没有合并其他剧情分支。

## 模块

- `ConsoleRenderer`：Win32 设备、字符宽度测量、坐标输出、颜色、裁剪，最后一遍绘制边框。
- `TextLayout`：宽文本分段、换行、裁剪。UTF-8 只在界面边界转换一次。
- `GameUI`：只读界面快照、任务/状态/命令三个区域、历史翻页、固定行输入编辑。
- `ConsoleUI`：保持旧主循环的调用方式。原有 cout 输出被捕获进内存，不再直接输出整张界面。

没有新增 getHP/getSP 之类的重复接口，读的是既有的 getHealth/getStamina/getWisdom/
getStrength/getReputation。背包用 `getInventory().getItems().size()` 显示「格数」，
不是堆叠物品的总数量。当前任务直接取主循环已有的目标文本，没有另建一套任务模型。

## 坐标契约

`UI_WIDTH=110` 表示最后一列的坐标，`UI_HEIGHT=30` 表示行数，`DIVIDER_X=73`。
Win32 从 0 开始计数，所以右边框定在 X=110 时实际需要 111 个列位置，不是 110 个。

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

每帧先清除，按区域裁剪文字，再逐坐标画边框。没有 `setw`，没有按字符串长度估算中文
宽度，没有手工补空格对齐，也没有拼接整张 ASCII 画面。清除单元格用
`FillConsoleOutputCharacterW`。

中文宽度是量出来的，不是算出来的：把显示片段写进一个不显示的控制台缓冲区，读实际
光标位移得到列数，再用 `SetConsoleCursorPosition` / `WriteConsoleW` /
`SetConsoleTextAttribute` 写进可见界面。测量缓冲区继承当前字体，换字体时清空缓存。
Unicode 标量、组合附加符、变体选择符、代理对和连接符都保持为一个显示片段，不按
字节数或代码单元数对齐。无效 UTF-8 转成替代符，控制字符不会执行光标移动。极长或
无法测量的片段安全截断。

非 Windows 平台没有测量设备，测试里用一个确定性列宽的替代实现顶上，它不能替代真机验收。

输入用 `ReadConsoleInputW`，关掉系统回显和自动换行。输入最长 1024 个片段，支持左右、
Home/End、Backspace/Delete，长命令在输入行内横向滚动，Ctrl+C / Ctrl+Z 结束输入。
窗口不够大时显示提示、忽略游戏命令、等用户调整；退出时恢复控制台模式、颜色、代码页
和光标。中文输入法下字母键会变成拼音串、字符为空，这时用虚拟键码回退，让 WASD 和
数字键在两种输入法下都能操作；打字输入不走这条回退。

## 构建与测试

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`ui_layout_tests` 覆盖：中文、英文、混排、数字、全角/半角标点、长剧情、长任务、绿色
智慧提示、背包显示、边框最后绘制、中央裁剪、长输入、组合字符编辑、翻页、EOF、小窗口
恢复、只读模型访问。测试只替换 OS 屏幕设备，布局、换行、编辑器和模型适配都是真实代码。

## 原生验收

真机检查要读真实控制台的边框单元格，必须在附着控制台的窗口里运行，不能把输入输出
重定向到文件。它不进普通 CTest，因为没有控制台的 CI 满足不了这个条件。

```powershell
.\build\Release\ui_windows_smoke.exe
.\build\Release\ui_windows_smoke.exe --interactive
```

交互版依次显示上述 10 类内容，每页按 Enter 继续。第 9 项智慧奖励和第 10 项背包页面
可以用 Windows 截图工具留存运行图。另外还需要手动确认：

- 输入或粘贴超过一行的中英文，编辑过程中 X=73 和 X=110 不移动。
- PageUp / PageDown 翻阅时保持已输入的命令。
- 只改窗口高度、改字体或缩放、缩到最低尺寸以下再恢复；不能执行不可见命令。
- 最后一行和最右侧写入不会让整个屏幕滚动；输入中文时 IME 行为正常。
- 智慧提示绿色、错误红色、标题青色、普通文本白色、提示黄色。

## 参考资料

- [控制台坐标](https://learn.microsoft.com/en-us/windows/console/coord-str)
- [屏幕缓冲区](https://learn.microsoft.com/en-us/windows/console/createconsolescreenbuffer)
- [ReadConsoleInputW](https://learn.microsoft.com/en-us/windows/console/readconsoleinput)
