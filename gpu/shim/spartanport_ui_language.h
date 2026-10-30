// SPDX-License-Identifier: GPL-2.0-or-later
// The in-game menu's languages: every table is keyed by the English text, and Tr() looks a text up in
// the language in use. English needs no table. Chinese is used once its system font has been loaded
// (spartanport_ui_font.h); the other languages are drawn with the embedded DejaVu Sans.
#pragma once
#include <atomic>
#include <cstddef>
#include <string_view>
#include <utility>

#include "ui_language/spartanport_ui_de.h"
#include "ui_language/spartanport_ui_es.h"
#include "ui_language/spartanport_ui_fr.h"
#include "ui_language/spartanport_ui_id.h"
#include "ui_language/spartanport_ui_it.h"
#include "ui_language/spartanport_ui_pl.h"
#include "ui_language/spartanport_ui_pt.h"
#include "ui_language/spartanport_ui_ru.h"
#include "ui_language/spartanport_ui_tr.h"

namespace SpartanUi {
inline std::atomic<bool> chinese{false};      // Chinese is the language in use (its font is loaded).
inline std::atomic<bool> chinese_font{false}; // The CJK font is loaded: Chinese can be chosen.
inline bool WantsChinese(std::string_view language) {
    return language == "zh" || language.starts_with("zh-") || language.starts_with("zh_");
}
inline constexpr std::pair<std::string_view, const char*> ChineseText[] = {
    {"Windowed", "窗口化"},
    {"Borderless", "无边框"},
    {"Fullscreen", "全屏"},
    {" FPS", " 帧"},
    {"%d FPS (engine frame rate)", "%d 帧（引擎帧率）"},
    {"%d change%s need%s a restart (unsaved progress is lost).", "%d 项更改需要重启（未保存进度会丢失）。%.0s%.0s"},
    {"(nothing selected)", "（未选择任何项目）"},
    {"0% is no background.", "0% 表示无背景。"},
    {"1 (lowest latency)", "1（最低延迟）"},
    {"1440 × 1080 · 4:3 (beta)", "1440 × 1080 · 4:3（测试）"},
    {"1920 × 1200 · 16:10 (beta)", "1920 × 1200 · 16:10（测试）"},
    {"1920 × 1440 · 4:3 (beta)", "1920 × 1440 · 4:3（测试）"},
    {"2048 × 1536 · 4:3 (beta)", "2048 × 1536 · 4:3（测试）"},
    {"2560 × 1080 · 21:9 (beta)", "2560 × 1080 · 21:9（测试）"},
    {"2560 × 1600 · 16:10 (beta)", "2560 × 1600 · 16:10（测试）"},
    {"2560 × 1920 · 4:3 (beta)", "2560 × 1920 · 4:3（测试）"},
    {"3440 × 1440 · 21:9 (beta)", "3440 × 1440 · 21:9（测试）"},
    {"3840 × 1600 · 21:9 (beta)", "3840 × 1600 · 21:9（测试）"},
    {"5120 × 1440 · 32:9 (beta)", "5120 × 1440 · 32:9（测试）"},
    {"5120 × 2160 · 21:9 (beta)", "5120 × 2160 · 21:9（测试）"},
    {"60 (original)", "60（原始）"},
    {"60 FPS: the frame rate patch is not available for this game build", "60 帧：此游戏版本无法使用帧率补丁"},
    {"A bar at the top for the last enemy hit.", "在顶部显示最近击中敌人的血条。"},
    {"About", "关于"},
    {"Absolute", "绝对位置"},
    {"Anisotropic filtering", "各向异性过滤"},
    {"Appearance", "外观"},
    {"Applied on restart", "重启后生效"},
    {"Apply and restart", "应用并重启"},
    {"Automatic QTE", "自动完成 QTE"},
    {"Background opacity", "背景不透明度"},
    {"Big FPS gain. Switch it off if lighting or objects look wrong.", "大幅提高帧率；若光照或物体异常，请关闭。"},
    {"Blue orbs (magic)", "蓝魂（魔法）"},
    {"Bottom left", "左下角"},
    {"Bottom right", "右下角"},
    {"CPU (game)", "CPU（游戏）"},
    {"Caps the frame rate. The engine frame rate (below) is the highest it can go.", "限制显示帧率，最高不超过下方设置的引擎帧率。"},
    {"Cheats", "作弊选项"},
    {"Close", "关闭"},
    {"Compile new shaders in the background", "后台编译新着色器"},
    {"Controller", "手柄"},
    {"Controller: A = OK, B = cancel", "手柄：A 确认，B 取消"},
    {"Controller: B = OK, A = cancel", "手柄：B 确认，A 取消"},
    {"Controller: Cross (A) = OK, Circle (B) = cancel", "手柄：叉号（A）确认，圆圈（B）取消"},
    {"Corner", "位置"},
    {"Damage Kratos does to enemies.", "奎托斯对敌人造成的伤害。"},
    {"Damage Kratos takes.", "奎托斯受到的伤害。"},
    {"Damage dealt", "造成伤害"},
    {"Damage taken", "受到伤害"},
    {"Deferred GPU readbacks", "延迟 GPU 回读"},
    {"Depth of field", "景深"},
    {"Display", "显示"},
    {"Display mode", "显示模式"},
    {"Effective limit", "实际帧率上限"},
    {"Effects", "画面效果"},
    {"Enable before opening New Game. Does not change an existing save difficulty.", "请在打开“新游戏”前启用，不改变已有存档难度。"},
    {"Enemy health bar", "敌人血条"},
    {"Engine frame rate", "引擎帧率"},
    {"Engine frame rate: ", "引擎帧率："},
    {"Engine frame rate: 60 FPS only (no frame rate patch for this game build)", "引擎帧率：仅支持 60 帧（此版本无可用帧率补丁）"},
    {"Every multiplier back to 1x (the game's normal values).", "将所有倍率恢复为 1 倍（游戏正常数值）。"},
    {"F12 saves a screenshot to the user\\screenshots folder.", "按 F12 将截图保存到 user\\screenshots 目录。"},
    {"Fewer stutters in new areas; an object or effect may appear a moment later.", "减少新区域卡顿；物体或特效可能稍后出现。"},
    {"Fewer waits for the GPU. Switch it off if textures or shadows look wrong.", "减少等待 GPU；若纹理或阴影异常，请关闭。"},
    {"Font size", "字号"},
    {"Frame rate limit", "帧率上限"},
    {"Frames queued", "排队帧数"},
    {"Frametime", "帧时间"},
    {"GPU (name and use)", "GPU（名称和使用率）"},
    {"Game", "游戏"},
    {"Game default", "游戏默认"},
    {"Game hooks", "游戏功能挂钩"},
    {"Game image", "游戏画面"},
    {"Game reads GPU data without waiting", "游戏读取 GPU 数据时不等待"},
    {"Gold orbs (Rage of Sparta)", "金魂（斯巴达之怒）"},
    {"Green orbs (health)", "绿魂（生命）"},
    {"HUD", "游戏界面"},
    {"Health from each green orb. The bar stays within its maximum.", "每个绿魂恢复的生命，最高不超过生命上限。"},
    {"Health stays full. Only Kratos is protected. Turning it off does not undo past refills.", "生命保持满值，仅保护奎托斯。关闭后不会撤销已恢复的生命。"},
    {"How strong the sharpening is.", "锐化效果的强度。"},
    {"Image", "画面"},
    {"Infinite Double Jumps", "无限空中跳跃"},
    {"Infinite Rage of Sparta", "无限斯巴达之怒"},
    {"Infinite health", "无限生命"},
    {"Infinite item meter", "无限道具能量"},
    {"Infinite magic", "无限魔法"},
    {"Information", "信息"},
    {"Items side by side or stacked.", "各项并排显示或分行显示。"},
    {"Keep mouse in window", "将鼠标限制在窗口内"},
    {"Keyboard: type, Backspace to erase, Enter = OK, Esc = cancel", "键盘：输入文字，Backspace 删除，Enter 确认，Esc 取消"},
    {"L / R or Q / E: switch tabs. Settings are saved in spartanport.ini.", "L / R 或 Q / E：切换选项卡。设置保存在 spartanport.ini。"},
    {"L1 / R1 or Q / E: switch tabs. Settings are saved in spartanport.ini.", "L1 / R1 或 Q / E：切换选项卡。设置保存在 spartanport.ini。"},
    {"L2 aim with the mouse (Head of Helios, Bow of Apollo). Relative: the stick follows the movement and goes back to neutral when the mouse stops. Absolute: the stick points from where the mouse was when L2 was pressed to where it is.", "按 L2 用鼠标瞄准（赫利俄斯之首、阿波罗之弓）。相对模式跟随鼠标移动，停止时回中；绝对模式以按下 L2 时的位置为中心，根据偏移控制摇杆。"},
    {"L2 aim with the mouse: lower needs more mouse movement for the same stick, higher less.", "L2 鼠标瞄准：数值越低，所需鼠标移动距离越大；越高则越小。"},
    {"LB / RB or Q / E: switch tabs. Settings are saved in spartanport.ini.", "LB / RB 或 Q / E：切换选项卡。设置保存在 spartanport.ini。"},
    {"Layout", "布局"},
    {"Live", "即时生效"},
    {"Loading shader cache %u%%", "正在加载着色器缓存 %u%%"},
    {"Loading shader cache...", "正在加载着色器缓存……"},
    {"Magic from each blue orb. The bar stays within its maximum.", "每个蓝魂恢复的魔法，最高不超过魔法上限。"},
    {"Magic stays full.", "魔法保持满值。"},
    {"Max red orbs", "最大红魂"},
    {"More frames queued: steadier FPS, slower response to input.", "增加排队帧数可稳定帧率，但输入响应更慢。"},
    {"Mouse prompt distance", "鼠标旋转提示距离"},
    {"Multipliers", "倍率"},
    {"Native · 1920 × 1080 · 16:9", "原生 · 1920 × 1080 · 16:9"},
    {"No effect: the game's image is larger than the window.", "不生效：游戏画面大于窗口。"},
    {"No effect: the game's image is smaller than the window.", "不生效：游戏画面小于窗口。"},
    {"One item per line", "每项一行"},
    {"One line", "单行"},
    {"Overlay", "性能叠加层"},
    {"Performance", "性能"},
    {"Preparing graphics...", "正在准备图形……"},
    {"Preview", "预览"},
    {"QTE prompts", "QTE 提示"},
    {"Quick-time prompts answer themselves: single presses, repeated presses, holds and stick rotations.", "自动完成快速反应提示：单次按键、连按、长按及摇杆旋转。"},
    {"RAM (game / total)", "内存（游戏／总量）"},
    {"Rage of Sparta from each gold orb. The bar stays within its maximum.", "每个金魂恢复的斯巴达之怒，最高不超过上限。"},
    {"Rage of Sparta stays full.", "斯巴达之怒保持满值。"},
    {"Red orbs", "红魂"},
    {"Red orbs gained. Orbs you have and prices stay the same.", "获得的红魂数量倍率，不改变已有红魂和物品价格。"},
    {"Red orbs stay at the maximum.", "红魂保持最大值。"},
    {"Relative", "相对移动"},
    {"Render resolution", "渲染分辨率"},
    {"Repeat air jumps without landing. Turn off for normal movement.", "可连续空中跳跃，无需落地。关闭恢复正常移动。"},
    {"Reset all to 1x", "全部恢复为 1 倍"},
    {"Right stick sensitivity", "右摇杆灵敏度"},
    {"Right stick style", "右摇杆模式"},
    {"Shader cache", "着色器缓存"},
    {"Sharpening", "锐化"},
    {"Sharpening (RCAS)", "锐化 (RCAS)"},
    {"Sharpening strength", "锐化强度"},
    {"Sharpens the final image.", "对最终画面进行锐化。"},
    {"Sharper floors and walls seen at an angle. Little cost on most GPUs.", "斜视地面和墙壁时更清晰，多数显卡的性能开销很小。"},
    {"Show overlay", "显示性能叠加层"},
    {"Size of the overlay text.", "性能叠加层文字大小。"},
    {"Strength of the controller's vibration. 0% turns it off.", "手柄震动强度，0% 为关闭。"},
    {"Textures (applied on restart)", "纹理（重启后生效）"},
    {"The first start after an update takes longer.", "更新后的首次启动需要更长时间。"},
    {"The game's own blur of distant objects, strongest in cutscenes. Off keeps the whole picture sharp.", "游戏自带的远景模糊，过场动画中最明显。关闭后整个画面保持清晰。"},
    {"The highest frame rate the game itself runs at. 60 for spots that break above it (a ledge Kratos cannot cross, a button-mash prompt). R3 + L1 + R1 or F9 go through the list.", "游戏引擎自身运行的最高帧率。如果出现无法跨越边缘或连按提示异常，请改回 60。按 R3 + L1 + R1 或 F9 循环切换。"},
    {"The item meter (bow, Helios' head, boots) stays full.", "道具能量（弓、赫利俄斯之首、靴子）保持满值。"},
    {"The performance overlay in a corner of the screen.", "在屏幕角落显示性能信息。"},
    {"The resolution the game draws at. Higher is sharper and slower.", "游戏渲染画面的分辨率，越高越清晰，但性能开销越大。"},
    {"Top left", "左上角"},
    {"Top right", "右上角"},
    {"Turn off Max red orbs to use the multiplier.", "先关闭“最大红魂”才能使用倍率。"},
    {"Unavailable: game version or code signature mismatch.", "不可用：游戏版本或代码特征不匹配。"},
    {"Unlimited", "无限制"},
    {"Unlock Very Hard", "解锁极难模式"},
    {"Up/Down or D-pad: choose   Enter or A: OK   Esc or B: cancel", "上下方向键或十字键选择；Enter 或 A 确认；Esc 或 B 取消"},
    {"Up/Down or D-pad: choose   Enter or B: OK   Esc or A: cancel", "上下方向键或十字键选择；Enter 或 B 确认；Esc 或 A 取消"},
    {"Up/Down or D-pad: choose   Enter or Cross (A): OK   Esc or Circle (B): cancel", "上下方向键或十字键选择；Enter 或叉号（A）确认；Esc 或圆圈（B）取消"},
    {"Upscaler", "超分辨率"},
    {"VRAM (used / available)", "显存（已用／可用）"},
    {"VSync", "垂直同步"},
    {"Vibration", "震动"},
    {"Waits for the monitor: no tearing, a little more input delay.", "等待显示器同步：消除画面撕裂，但会略微增加输入延迟。"},
    {"What to show", "显示项目"},
    {"Where the overlay sits.", "性能叠加层的显示位置。"},
    {"While you play, the mouse stays in the game window; this menu and other windows get it back.", "游玩时限制鼠标在游戏窗口内；打开本菜单或切换窗口时释放鼠标。"},
    {"Window", "窗口"},
    {"Windowed, borderless window or exclusive fullscreen.", "窗口化、无边框窗口或独占全屏。"},
    {"With Joystick prompts on Adaptive mouse: how far the mouse moves for one step of a rotation.", "摇杆提示选择自适应鼠标时，每次旋转步进所需的鼠标移动距离。"},
    {"applied", "已应用"},
    {"damage", "伤害"},
    {"enemy health", "敌人生命"},
    {"loading %u%%", "加载中 %u%%"},
    {"not applied", "未应用"},
    {"orb pickups", "魂拾取"},
    {"ready", "就绪"},
    {"red orbs", "红魂"},
    // the trophy window and the language option
    {"Theme", "主题"},
    {"Original", "原版"},
    {"Card", "卡片"},
    {"Card, cinematic platinum", "卡片（电影式白金）"},
    {"Corner popup", "角落弹窗"},
    {"How an unlocked trophy is shown. Original is the port's own notice.", "解锁奖杯时的显示方式。原版是移植版自带的通知。"},
    {"Mute the sound", "静音"},
    {"No sound plays without a sound file in user/custom_trophy.", "user/custom_trophy 中没有声音文件时不会播放声音。"},
    {"Volume", "音量"},
    {"Time on screen", "显示时间"},
    {"Theme default", "主题默认"},
    {"Platinum light", "白金光效"},
    {"Animate the platinum", "白金动画"},
    {"Off: the platinum shows its own picture, like the other trophies.", "关闭：白金奖杯和其他奖杯一样显示自己的图片。"},
    {"Write the grade", "显示等级"},
    {"The grade's name before the second line.", "在第二行前显示等级名称。"},
    {"Bronze", "铜"},
    {"Silver", "银"},
    {"Gold", "金"},
    {"Platinum", "白金"},
    {"A preview: it looks and sounds like an unlock; nothing is unlocked or saved.", "预览：外观和声音与解锁相同；不会解锁或保存任何内容。"},
    {"All four in a row", "四个连续"},
    {"Three at once", "三个同时"},
    {"Your files", "你的文件"},
    {"Sounds:", "声音："},
    {"Pictures:", "图片："},
    {"Your own sounds, pictures, animations and texts go in user/custom_trophy (docs/TROPHIES.md). Sounds reload here; pictures when the game starts.", "你自己的声音、图片、动画和文字放在 user/custom_trophy（docs/TROPHIES.md）。声音在此重新加载；图片在游戏启动时加载。"},
    {"Reload sounds", "重新加载声音"},
    {"Trophy unlocked!", "奖杯已解锁！"},
    {"Language", "语言"},
    {"Pause the game", "暂停游戏"},
    {"The game stops while this menu or the trophy list is open.", "打开此菜单或奖杯列表时游戏暂停。"},
    {"Same as the launcher", "与启动器相同"},
    {"The language of this menu and of the trophies. Until you choose one, the launcher's is used.", "此菜单和奖杯使用的语言。在你选择之前，使用启动器的语言。"},
    {"none", "无"},
    {"drawn", "绘制"},
    {"artwork", "奖杯图片"},
    {"Badge", "徽章"},
    {"Supported features", "支持的功能"},
    {"What this game version can use. Each one is turned on or off in its own tab.", "此游戏版本可以使用的功能。每项功能在各自的选项卡中开启或关闭。"},
    {"supported", "支持"},
    {"not supported", "不支持"},
    {"Orb pickups", "魂拾取"},
    {"Damage", "伤害"},
    {"All grades", "全部等级"},
    {"Square/G: grade", "方块/G：等级"},
};
using TextTable = std::pair<std::string_view, const char*>;

struct Language {
    std::string_view code; // as the launcher names it (SPARTAN_UI_LANGUAGE)
    const char* name;      // in the language itself
    const TextTable* texts;
    std::size_t count;
};
template <std::size_t N>
constexpr Language Table(std::string_view code, const char* name, const TextTable (&texts)[N]) {
    return {code, name, texts, N};
}
/// The menu's languages, in the order of the language list; 0 is English.
inline constexpr Language Languages[] = {
    {"en", "English", nullptr, 0},
    Table("pt", "Português (Brasil)", PortugueseText),
    Table("es", "Español", SpanishText),
    Table("fr", "Français", FrenchText),
    Table("de", "Deutsch", GermanText),
    Table("it", "Italiano", ItalianText),
    Table("pl", "Polski", PolishText),
    Table("tr", "Türkçe", TurkishText),
    Table("id", "Bahasa Indonesia", IndonesianText),
    Table("ru", "Русский", RussianText),
    Table("zh", "中文", ChineseText),
};
inline constexpr int LanguageCount = int(sizeof(Languages) / sizeof(Languages[0]));
inline std::atomic<int> language{0}; // index into Languages

/// The index of a launcher language ("pt", "pt-BR", "zh_CN"...); 0 (English) for any other.
inline int FindLanguage(std::string_view code) {
    if (WantsChinese(code)) return LanguageCount - 1;
    const std::string_view two = code.substr(0, 2);
    for (int i = 1; i < LanguageCount; ++i) {
        if (Languages[i].code == two && Languages[i].code != "zh") return i;
    }
    return 0;
}

/// Uses the language at once; Chinese only once its font is loaded (else English). Returns the
/// index in use.
inline int SetLanguage(int index) {
    if (index < 0 || index >= LanguageCount) index = 0;
    if (Languages[index].code == "zh" && !chinese_font) index = 0;
    language = index;
    chinese = Languages[index].code == "zh";
    return index;
}

inline const char* Lookup(const Language& table, const char* text) {
    for (std::size_t i = 0; i < table.count; ++i) {
        if (table.texts[i].first == text) return table.texts[i].second;
    }
    return text;
}

inline const char* Tr(const char* text) {
    if (!text) return text;
    if (chinese) return Lookup(Languages[LanguageCount - 1], text);
    const int index = language;
    return index > 0 && index < LanguageCount ? Lookup(Languages[index], text) : text;
}
} // namespace SpartanUi
