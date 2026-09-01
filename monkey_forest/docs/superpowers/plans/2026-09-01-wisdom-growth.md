# Wisdom Growth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Five one-time story rewards; any three raise initial wisdom 1 to 4, with a cap of 5.

**Architecture:** Modify the existing EventSystem/EventFactory, not a replacement event system. An internal helper owns reward checks and Chinese messages; persistent flags remain in WorldState. Existing SaveManager serializes every flag without format changes.

**Tech Stack:** C++17, existing CMake targets, GCC direct-build fallback.

**Spec:** User's wisdom-P0 specification and approved event/flag mapping in this conversation (2026-09-01).

## Global Constraints

- Base: member-5-zip at 9afe9d302318977bd4e8273bac4b2600c17496f9; project directory monkey_forest/.
- Development branch fix/wisdom-growth; PR base member-5-zip; no automatic merge.
- Wisdom starts at 1, capped at 5. Five opportunities, any three reach 4.
- Keep Player::getWisdom(), Player::changeWisdom(int), GameContext and all public signatures unchanged.
- SkillType stays Gather, Climb, Combat, Leadership. No wisdom training.
- Never consume item_chip for research; never lower final-ending requirements.
- New flags: flag_wisdom_tree, flag_wisdom_river, flag_wisdom_cave, flag_wisdom_base.
- Chip reward guard is flag_drone_analyzed OR flag_route_hack_ready. Each existing route retains its own outcome flag.
- No pickup, inventory, NPC, combat, map, UI layout or unrelated member changes. A prefix-only StatusView filter is necessary so the four new internal reward flags do not appear in normal output (user requirement 35).

## Approved event mapping

| Node | Existing event/choice | Reward state |
|---|---|---|
| Tree | event_tree_trial / 3 | flag_wisdom_tree |
| River | event_glowing_river / 2 | flag_wisdom_river |
| Cave | event_echo_tracking / 1 | flag_wisdom_cave |
| Chip | event_drought_choice / 2; event_drone_crash / 1 | Either existing research outcome flag |
| Base | event_base_infiltration / 2; research after acquiring logs by any legal route | flag_wisdom_base |

Tree stays optional. River/cave rewards have no wisdom threshold. Chip research has no wisdom threshold and requires the real chip. Keep the base's existing stealth-entry requirement; offer log research after any successful entry instead of lowering that requirement. Follow-up investigations never replay event resources, items, pending state or stage completion. They remain available before the final decision, including while the final event awaits a choice.

### Task 1: Reproduce and fix story rewards

**Files:** Create tests/wisdom_growth_test.cpp; modify src/EventSystem.cpp and src/EventFactory.cpp; register the test in CMakeLists.txt.

**Interfaces:** Consume real EventSystem, Player, WorldState, SaveManager and ProgressSystem. Produce no public interfaces. Chinese investigation targets pass through existing triggerEvent(string, GameContext&).

- [x] Write failing cases for each node at wisdom 1 and repeat investigation, all ten three-node combinations, chip/drone sharing, cap, persistence, pending recovery and retry without premature completion.

```cpp
expect(player.getWisdom() == 1, "initial wisdom");
expect(events.triggerEvent("event_glowing_river", ctx).success, "river opens");
expect(events.chooseEventOption("", 2, ctx).success, "river investigation");
expect(player.getWisdom() == 2, "wisdom-1 player earns river reward");
```

- [x] Run tests against unmodified production code; retain expected assertion failures.
- [x] Centralize four new flag names in EventSystem.cpp's anonymous namespace. Reuse chip outcome flags through an OR guard; record the corresponding route outcome even when its shared reward was previously claimed.

```cpp
const int before = ctx.player.getWisdom();
ctx.player.changeWisdom(1);
ctx.world.setFlag(rewardFlag);
const bool increased = ctx.player.getWisdom() > before;
```

- [x] Replace all six current event increment sites with five shared once-only rewards. Use the comparison above for honest cap messages.
- [x] Add Chinese investigation aliases in EventSystem only: 树冠, 管线/银色管线, 碑文/回声, 晶片/星猿晶片, 日志/控制台. Before river/cave completion the alias opens its original event; afterwards it studies without replaying the main event. Tree cannot be retroactively re-chosen after another trial solution. Base investigation requires base access, real chip and complete logs. Chip study requires a real chip and a valid story stage.
- [x] Preserve pending events during follow-up study and ensure completed choices cannot issue rewards twice. Reject invalid/insufficient-stamina choices without marking completion.
- [x] Run the new suite and original five suites, then document actual commands and results.

### Task 2: Integrated verification and handoff

**Files:** Create tests/wisdom_main_test.cpp and tests/run_wisdom_checks.sh; modify CMakeLists.txt, docs/member2-integration-notes.md and only the new-flag filter in src/StatusView.cpp.

**Interfaces:** Execute the actual src/main.cpp with redirected input/output; use the existing save/load commands and real game modules. No production main changes required.

- [x] First run a main regression against the old EventSystem: skip tree observation, investigate river/cave/chip, enter base and choose hack; assert the actual ending text is present, not just wisdom fixtures.

```cpp
expect(output.find("【结局：无声胜利】") != std::string::npos,
       "skip-tree playthrough must reach hack ending");
```

- [x] Cover skip tree + river, late log research after companion entry, new EventSystem after save/load, every fresh node at wisdom 5, old save without new flags and rejection of train wisdom/智慧.
- [x] Build every src/*.cpp with C++17 and warnings-as-errors; link and run all seven test programs. Run the same suite with undefined-behavior sanitizer. Build outputs and temporary saves stay outside the source tree.
- [x] Register both new tests in CMake, preserving existing targets and Windows UTF-8 options. If CMake is unavailable, report direct GCC verification explicitly rather than claiming CTest ran.
- [x] Document flags, exact event choices, Chinese commands, old-save policy (missing new flags default unset; no guessed history migration), and unchanged public/ending contracts.
- [ ] Review diff and git status; commit scoped story and test changes, push only fix/wisdom-growth, create PR titled fix: prevent wisdom route progression lock targeting member-5-zip. Do not merge.

## Plan self-review

Task 1 and Task 2 share CMakeLists.txt and the real EventSystem interface; test target additions are additive and public signatures stay unchanged. Task 2's main-path assertions cover reachability that isolated stage fixtures cannot prove. Save/load checks use a new Player, WorldState and EventSystem, not merely a repeated call on the original objects. The two chip paths must be tested in both orders; otherwise a sixth opportunity could slip through.

## Execution record

- Base production failed the initial new suites as expected: 13 reward/state groups and 3 main-loop groups failed.
- Implemented five rewards using four new persistent flags and the existing two chip research outcomes; no public signatures or save format changes.
- Direct full C++17 build passed all seven programs, including 19 reward/state groups and 6 main groups with all ten actual three-node routes; UBSan also passed before final review fixes.
- Independent read-only review approved the production design and identified a test-only whitespace path issue. The main save test now uses an isolated working directory plus progress.txt, asserting the exact file exists.
- A new real-main assertion exposed raw flag_wisdom_* output in unchanged StatusView. Added a two-line prefix-only filter to satisfy player-facing text requirements without changing layout or legacy flags. Independent scoped re-review approved both final fixes.
- Final GCC 13.3 normal and serial UBSan builds passed all seven test programs (19 node/state groups, 6 main groups). CMake/CTest are not installed and were not run. A parallel UBSan attempt produced an empty Inventory.o; isolated recompilation and a fresh serial full build passed without any Inventory source change.
- Existing same-instance load/active-event cache behavior is documented separately and not changed; it does not permit repeated wisdom rewards.
