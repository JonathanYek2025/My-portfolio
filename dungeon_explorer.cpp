/* Dungeon Explorer (C++) — In-Source Technical Documentation (Specific)
 * - Each function has a Category and a UI Scope (when applicable).
 * - This version also sanitizes stray '}' and balances braces.
 * - No runtime behavior was intentionally changed.
 */

// -----------------------------------------------------------------------------
// Dungeon Explorer (C++) — Complete, fixed & annotated (Oct 28, 2025)
// Jonathan's all-in-one single-file build with clear section markers.
//
// ▶ Core menus & UI
// ✔ Main Menu (Single / Multi / Load / Settings / Quit) + class/element customization
// ✔ Pause Menu (Resume / Save / Load / Settings / Quit)
// ✔ Victory / Defeat overlays with retry (Defeat music does NOT loop)
// ✔ Inventory readability: purple treasure tiles, stronger borders, wrapped item names
// ✔ Word-wrap for save-slot & inventory text (no mid-word truncation)
// ✔ Full-screen scrollable log viewer (H) — wheel/PgUp/PgDn/Home/End
// ✔ Clear per-function documentation (Category + UI Scope) directly in code
//
// ▶ Gameplay & systems
// ✔ Procedural dungeon generation (entrance fixed, randomized content, guaranteed boss)
// ✔ Co-op combat fixes: cell-locked battle; both players can act; item use ends turn
// ✔ 10% miss chance (players & enemies); ≥3-turn skill cooldown via helper
//
// ▶ Persistence
// ✔ Save/Load modals (3 slots): hover smoothing, wrapped labels, slot previews
// ✔ VER 2 file format compatibility + robust reader (staged load → commit on success)
// ✔ Boss persistence guard (boss cell always present until victory)
//
// ▶ Audio
// ✔ Music volume slider (0–100) in Main/Pause Settings with live application
// ✔ Defeat theme does not loop
//
// ▶ Code quality & maintenance
// ✔ Descriptive reference names (e.g., EnemyData &enemy, PlayerData &player, AppState &app)
// ✔ Specific per-function doc blocks (Category/Purpose; UI Scope where relevant)
// ✔ Removed duplicate helper definitions (labels/skills/cat) to avoid drift
// ✔ Pruned unused constants where safe (e.g., INV2_* if not referenced)
// ✔ Bracket sanitizer applied: stray '}' commented; missing closers auto-appended
// ✔ Lean “doc-lite” variant available (function headers only) to keep line count modest
//
// Build:
//   g++ -std=c++17 -O2 -o dungeon_explorer dungeon_explorer.cpp -lsplashkit
// -----------------------------------------------------------------------------

// =====[ includes ]============================================================
#include "splashkit.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <limits>
#include <cmath>

// Shorthands
using std::string;
using std::vector;

// =====[ layout constants ]====================================================
// Panels & UI baseline. Inventory Y becomes dynamic with P2 enabled (see helper).
static constexpr float INV_X = 24.0f;
static constexpr float INV_Y = 170.0f;     // default; overridden by inventory_y_for(...)
static constexpr int   LOG_MAX_LINES = 14; // compact on-screen log size
static constexpr float INV2_X = INV_X + 260.0f + 24.0f; // second panel to the right
static constexpr float INV2_Y = INV_Y;                  // same vertical baseline

// =====[ tiny utility types & helpers ]=======================================
struct GridPoint { int x{0}; int y{0}; };

// RNG & quick string helpers
/**
 * Category: RNG Helpers
 * Purpose : Random utilities (ranges, probabilities).
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static int random_in_range(int minValue, int maxValue) { return minValue + (rand() % (maxValue - minValue + 1)); }
/**
 * Category: RNG Helpers
 * Purpose : Random utilities (ranges, probabilities).
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool chance_percent(int percent) { return random_in_range(1, 100) <= percent; }
/**
 * Category: Formatting/Utility
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string to_str(int v) { return std::to_string(v); }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static inline std::string S(const char *x) { return std::string(x); }
template <class... Args>
/**
 * Category: Formatting/Utility
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static std::string cat(const Args&... args) { std::ostringstream oss; (oss << ... << args); return oss.str(); }

// Simple file-name escaping (spaces <-> underscores) for metadata tokens
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string escape_name(const string &s) { string r=s; for(char &c:r) if(c==' ') c='_'; return r; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string unescape_name(const string &s){ string r=s; for(char &c:r) if(c=='_') c=' '; return r; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool file_exists(const string &path){ std::ifstream f(path); return (bool)f; }

// =====[ text wrapping ]=======================================================
// SplashKit lacks precise font metrics by default; we use ~8 px/char heuristic.
// This stabilizes button labels, slot previews, and inventory captions.
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static float measure_approx_char_px() { return 8.0f; }

// Greedy word wrap respecting word boundaries (hard-break long words if needed)
/**
 * Category: Formatting/Utility
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static vector<string> wrap_lines(const string &s, float max_px, int max_lines)
{
    const float CHAR_PX = measure_approx_char_px();
    const int max_chars = (int)std::max(1.0f, std::floor(max_px / CHAR_PX));
    vector<string> out;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (max_chars <= 1) { out.push_back("."); return out; }

    std::istringstream iss(s);
    string word, line;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    while (iss >> word)
    {
        string candidate = line.empty() ? word : (line + " " + word);
        if ((int)candidate.size() <= max_chars)
        {
            line = candidate;
        }
        else
        {
            if (!line.empty()) out.push_back(line);
            if ((int)word.size() > max_chars)
            {
                // hard-break single huge word
                int start = 0;
                while (start < (int)word.size())
                {
                    out.push_back(word.substr(start, max_chars));
                    start += max_chars;
                    if ((int)out.size() == max_lines) return out;
                }
                line.clear();
            }
            else line = word;
        }
        if ((int)out.size() == max_lines) break;
    }
    if ((int)out.size() < max_lines && !line.empty()) out.push_back(line);
    return out;
}
/**
 * Category: Rendering/UI
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_wrapped_text(const string &s, color c, float x, float y,
                              float max_px, int max_lines, float line_h)
{
    vector<string> lines = wrap_lines(s, max_px, max_lines);
    float yy = y;
    for (int i=0;i<(int)lines.size();++i){ draw_text(lines[i], c, x, yy); yy += line_h; }
}

// =====[ basic drawing widgets ]==============================================
// Progress bar with border (0..1)
/**
 * Category: Rendering/UI
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void draw_bar_with_border(float x, float y, float w, float h, float pct)
{
    draw_rectangle(COLOR_GRAY, x, y, w, h);
    float clamped = std::max(0.0f, std::min(1.0f, pct));
    fill_rectangle(COLOR_GREEN, x + 1, y + 1, (w - 2) * clamped, h - 2);
}

// Buttons (with internal wrap to avoid mid-word truncation)
struct Button { float x{0}, y{0}, w{0}, h{0}; string label; };
/**
 * Category: UI Controls
 * UI Scope: UI: Button/Widget
 * Purpose : Implements menus, modals, and interactive widgets.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool point_in_button(float mx, float my, const Button &b){ return mx>=b.x && mx<=b.x+b.w && my>=b.y && my<=b.y+b.h; }
/**
 * Category: Rendering/UI
 * UI Scope: UI: Button/Widget
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_button(const Button &b, bool hover)
{
    fill_rectangle(hover ? rgba_color(220,220,220,255) : rgba_color(180,180,180,255), b.x,b.y,b.w,b.h);
    draw_rectangle(COLOR_WHITE, b.x,b.y,b.w,b.h);
    const float pad_l = 12.0f, pad_r = 12.0f, pad_t = 8.0f;
    const float max_text_w = std::max(1.0f, b.w - (pad_l + pad_r));
    draw_wrapped_text(b.label, COLOR_BLACK, b.x + pad_l, b.y + pad_t, max_text_w, 3, 16.0f);
}

// =====[ game data model ]=====================================================
enum class ItemKind    { Potion, Weapon, Treasure };
enum class PotionKind  { Heal, Poison };
enum class Element     { Neutral, Ice, Fire };
enum class CharacterClass { Wizard, Barbarian, Cleric };

struct StatusBlockEnemy {
    int stun_turns{0};
    int burn_turns{0};
    int burn_dps{0};
    int poison_turns{0};
    int poison_dps{0};
};
struct StatusBlockPlayer {
    int decay_turns{0};
    int decay_dps{0};
};
struct BuffBlock {
    int atk_bonus_turns{0};
    int atk_bonus_amount{0};
};

struct ItemData {
    int      id{0};
    ItemKind kind{ItemKind::Potion};
    string   name{"Potion"};
    int      heal{0};       // heal amount OR DPS for poison
    int      atk{0};        // weapon bonus
    PotionKind ptype{PotionKind::Heal};
};
struct ItemSlot {
    ItemData item{};
    int      count{0};
    bool is_empty() const { return count <= 0; }
};
struct Inventory {
    int max_slots{8};
    vector<ItemSlot> slots;
};

struct EnemyData {
    string kind{"goblin"};
    int level{1}, max_hp{10}, hp{10}, atk{3};
    bool alive{true};
    StatusBlockEnemy status;
};
struct PlayerData {
    string    display_name{"Hero"};
    GridPoint pos{0,0};
    int level{1}, exp{0}, exp_to_next{10};
    int max_hp{20}, hp{20};
    int base_atk{4}, atk{4}, equipped_atk_bonus{0};
    int skill_cooldown{0};          // shared cooldown, enforced >=3 turns
    Inventory bag;
    CharacterClass   cls{CharacterClass::Wizard};
    Element          element{Element::Ice};
    ItemData         equipped_weapon{};
    bool             has_equipped_weapon{false};
    BuffBlock        buff;
    StatusBlockPlayer pstatus;
    bool alive() const { return hp > 0; }
    int coins{0};
    int lifetime_earned{0};
};

enum class RoomType { Empty, Entrance, Enemy, Treasure, Boss };
struct DungeonCell {
    RoomType  type{RoomType::Empty};
    bool      visited{false};
    bool      explored{false};
    GridPoint gridLocation{0,0};
    bool      has_enemy{false};
    EnemyData enemy;
    bool      has_item{false};
    ItemData  item;
};
struct DungeonGrid {
    static constexpr int GridWidth  = 8;
    static constexpr int GridHeight = 8;
    DungeonCell cells[GridHeight][GridWidth];
};

// =====[ ui/audio enums & containers ]========================================
enum class UIMode { Menu, Exploring, Combat, Victory, Defeat, Pause, Shop, Settings };

enum class CombatTurn { PlayerTurn, EnemyTurn };
/**
 * Category: Enemy System
 * Purpose : Creates/updates enemy data and behaviors.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static string enemy_display_name(const string &k){
    if (k=="orc") return "Orc";
    if (k=="troll") return "Troll";
    if (k=="goblin") return "Goblin";
    if (k=="slime") return "Slime";
    if (k=="skeleton") return "Skeleton";
    if (k=="boss") return "Dungeon Boss";
    return "Enemy";
}
/**
 * Category: Formatting/Utility
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string element_label(Element e){ switch(e){case Element::Ice:return "Ice";case Element::Fire:return "Fire";default:return "Neutral";} }
/**
 * Category: Formatting/Utility
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string class_label(CharacterClass c){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    switch(c){ case CharacterClass::Wizard:return "Wizard"; case CharacterClass::Barbarian:return "Barbarian"; case CharacterClass::Cleric:return "Cleric"; }
    return "Unknown";
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static color class_dot_color(CharacterClass c){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    switch(c){ case CharacterClass::Wizard:return COLOR_BLUE; case CharacterClass::Barbarian:return COLOR_RED; case CharacterClass::Cleric:return COLOR_GREEN; }
    return COLOR_WHITE;
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string primary_skill_name(const PlayerData &player){
    if (player.cls==CharacterClass::Wizard) return (player.element==Element::Ice)?"Ice Nova (stun)":"Ignite (burn)";
    if (player.cls==CharacterClass::Barbarian) return "Quick Power (burst)";
    if (player.cls==CharacterClass::Cleric) return "Heal";
    return "Skill";
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string secondary_skill_name(const PlayerData &player){
    if (player.cls==CharacterClass::Wizard) return (player.element==Element::Ice)?"Ember (burn)":"Chill (stun)";
    if (player.cls==CharacterClass::Barbarian) return "Rage (+ATK)";
    if (player.cls==CharacterClass::Cleric) return "Divine Smite";
    return "Alt";
}

// =====[ app-wide UI/Save state ]=============================================
/**
 * Category: General Helper
 * UI Scope: UI: Save/Load Modal
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string slot_path(int slotIndex) { return string("save_slot") + to_str(slotIndex) + ".sav"; }
enum class Modal { None, SaveSelect, LoadSelect };
struct SaveLoadUI {
    Modal modal{Modal::None};
    string info;
    Button slotBtn[3];
    Button closeBtn;
    int last_hover{-1};
    int hover_frames{0};
};

struct HighScore {
    int    best_single{0};
    string best_single_name{"-"};

    int    best_team{0};
    string best_team_name{"-"};
};

static const string HIGHSCORE_FILE = "highscore.sav";

struct LogViewer {
    bool open{false};   // modal open
    int  top_index{0};  // 0 = bottom (show latest); larger => scrolled up
};

// =====[ master AppState ]====================================================
struct AppState {
    DungeonGrid dungeon;
    PlayerData  playerOne, playerTwo;
    bool        co_op_enabled{false};
    bool        has_player_two{false};
    int         active_player_index{1};
    string      party_name{"Party"};
    UIMode      mode{UIMode::Menu};
    vector<string> log;
    int         next_item_id{1};
    CombatTurn  turn{CombatTurn::PlayerTurn};
    SaveLoadUI  saveui;
    music       music_explore{};
    music       music_battle{};
    music       music_boss{};
    music       music_victory{};
    music       music_defeat{};
    bool        in_boss_fight{false};
    HighScore   highscore;
    LogViewer   logview;
    int pause_debounce{0};
    int shop_debounce{0};   // NEW: guard for Shop modal
    int log_debounce{0};    // NEW: guard for full-screen Log viewer;
    // Who receives purchases while the shop is open: 1 = P1, 2 = P2
    int shop_buyer{1};
    // ---- Combat cell-lock to stabilize co-op battles (both act on same tile)
    bool combat_locked{false};
    int  combat_cx{0}, combat_cy{0};
    int combat_hero_index{1};  // 1 = P1, 2 = P2; ONLY this hero fights this combat
    UIMode settings_return_to{UIMode::Menu};   // where to go when Settings closes
};

// ---------- P2 inventory vertical placement (below P1 on the left) ----------
/**
 * Category: Inventory/Items
 * UI Scope: UI: Inventory Panel
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static float inventory_panel_height(const Inventory &bag)
{
    int rows = (bag.max_slots + 3) / 4;       // 4 columns per row
    return rows * 56.0f + 24.0f;              // slot rows + header/footer space
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static inline float INV2_X_FOR(const AppState &) { return INV_X; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static float INV2_Y_FOR(const AppState &app)
{
    return INV_Y + inventory_panel_height(app.playerOne.bag) + 28.0f; // gap
}
// ---- Inventory Y that clears the player panel (adds space if P2 is visible)
/**
 * Category: Inventory/Items
 * UI Scope: UI: Inventory Panel
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static inline float inventory_y_for(const AppState &app)
{
    // P2 panel starts ~160px below P1 in your draw_player_panel.
    // Shift the inventory down by +160 when Player 2 is present.
    return app.has_player_two ? (INV_Y + 160.0f) : INV_Y;
}

// Optional: if you also draw P2’s bag stacked under P1’s bag in Exploring
/**
 * Category: Inventory/Items
 * UI Scope: UI: Inventory Panel
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static inline float inventory2_y_for(const AppState &app, int max_slots=8)
{
    // Each row of the bag is a 56px cell; 4 columns -> rows = ceil(slots/4).
    const int rows = (max_slots + 3) / 4;
    const float bag_h = rows * 56.0f;
    const float gap   = 28.0f;  // breathing room between bags
    return inventory_y_for(app) + bag_h + gap;
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static inline bool is_multiplayer(const AppState &app){ return app.has_player_two; }
/**
 * Category: Formatting/Utility
 * UI Scope: UI: Shop
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static inline const char* current_buyer_label(const AppState &app){
    if (!app.has_player_two) return "Player 1";
    return (app.shop_buyer == 2) ? "Player 2" : "Player 1";
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static inline int total_coins(const AppState &app){
    return app.has_player_two ? (app.playerOne.coins + app.playerTwo.coins)
                              : app.playerOne.coins;
}

// =====[ log pipeline ]========================================================
/**
 * Category: General Helper
 * UI Scope: UI: Log Viewer
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void push_log(AppState &app, const string &message){
    app.log.push_back(message);
    if ((int)app.log.size() > LOG_MAX_LINES) app.log.erase(app.log.begin());
}

// =====[ active player convenience ]==========================================
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static PlayerData& active_player(AppState &a) { return (a.active_player_index==1)?a.playerOne:a.playerTwo; }
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static const PlayerData& active_player(const AppState &a) { return (a.active_player_index==1)?a.playerOne:a.playerTwo; }

// =====[ inventory helpers ]===================================================
/**
 * Category: Player System
 * UI Scope: UI: Inventory Panel
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void init_player_inventory(PlayerData &player, int cap=8){ player.bag.max_slots=cap; player.bag.slots.assign(cap, ItemSlot{}); }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void recompute_attack(PlayerData &player){
    int weapon_bonus = (player.has_equipped_weapon ? player.equipped_weapon.atk : 0);
    int buff_bonus   = (player.buff.atk_bonus_turns > 0 ? player.buff.atk_bonus_amount : 0);
    player.atk = player.base_atk + player.equipped_atk_bonus + weapon_bonus + buff_bonus;
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool is_stackable(const ItemData &it){ return it.kind != ItemKind::Weapon; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static int find_stack(Inventory &bag, const ItemData &it){
    if (!is_stackable(it)) return -1;
    for (int i=0;i<(int)bag.slots.size();++i){
        ItemSlot &slot = bag.slots[i];
        if (!slot.is_empty() && slot.item.kind==it.kind && slot.item.name==it.name) return i;
    }
    return -1;
}
/**
 * Category: General Helper
 * UI Scope: UI: Save/Load Modal
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static int find_empty_slot(Inventory &bag){ for (int i=0;i<(int)bag.slots.size();++i) if (bag.slots[i].is_empty()) return i; return -1; }
/**
 * Category: Inventory/Items
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool add_item(Inventory &bag, const ItemData &it, int count=1){
    if (count<=0) return true;
    if (is_stackable(it)){
        int s = find_stack(bag, it);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (s>=0){ bag.slots[s].count += count; return true; }
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int k=0;k<count;++k){
        int e = find_empty_slot(bag); if (e<0) return false;
        bag.slots[e] = ItemSlot{it,1};
    }
    return true;
}
/**
 * Category: General Helper
 * UI Scope: UI: Save/Load Modal
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void consume_slot(Inventory &bag, int idx, int amt=1){
    if (idx<0 || idx>=(int)bag.slots.size()) return;
    ItemSlot &slot = bag.slots[idx]; if (slot.is_empty()) return;
    slot.count -= amt; if (slot.count<=0) slot = ItemSlot{};
}
/**
 * Category: Formatting/Utility
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string kind_label(ItemKind k){
    return (k==ItemKind::Potion)?"potion":(k==ItemKind::Weapon)?"weapon":"treasure";
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static PlayerData& hero_by_index(AppState &app, int idx)
{ return (idx==2 && app.has_player_two) ? app.playerTwo : app.playerOne; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static const PlayerData& hero_by_index(const AppState &app, int idx)
{ return (idx==2 && app.has_player_two) ? app.playerTwo : app.playerOne; }

// =====[ labels/colors ]=======================================================
static string primary_skill_name(const PlayerData &);    // fwd; already defined above signature-wise
static string secondary_skill_name(const PlayerData &);  // (kept here to calm compilers)

// =====[ generation, items & AI ]=============================================
static bool g_excalibur_spawned = false;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static ItemData weapon_named(int id, const string &name, int bonus){ ItemData it; it.id=id; it.kind=ItemKind::Weapon; it.name=name; it.atk=bonus; return it; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static ItemData heal_potion(int id, int heal){ ItemData it; it.id=id; it.kind=ItemKind::Potion; it.name="Heal Potion"; it.heal=heal; it.ptype=PotionKind::Heal; return it; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static ItemData poison_potion(int id, int dps){ ItemData it; it.id=id; it.kind=ItemKind::Potion; it.name="Poison Potion"; it.heal=dps; it.ptype=PotionKind::Poison; return it; }
/**
 * Category: Inventory/Items
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static ItemData gem_item(int id){ ItemData it; it.id=id; it.kind=ItemKind::Treasure; it.name="Gem"; return it; }
/**
 * Category: Dungeon Generation
 * Purpose : Builds or manipulates the runtime dungeon grid.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static int cell_depth(int col, int row){ return (col + row) / 3; }
/**
 * Category: Enemy System
 * Purpose : Creates/updates enemy data and behaviors.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static EnemyData make_enemy_for_depth(int depth, bool boss=false){
    EnemyData enemy;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    // AFTER (nerfed)
    if (boss){
        enemy.kind   = "boss";
        enemy.level  = std::max(2, depth + 1);
        enemy.max_hp = 28 + depth * 4;   // was 40 + 6*depth
        enemy.hp     = enemy.max_hp;
        enemy.atk    = 5 + depth;        // was 7 + depth
        return enemy;
    }
    int r = random_in_range(1,5);
    enemy.kind = (r==1)?"goblin":(r==2)?"orc":(r==3)?"troll":(r==4)?"slime":"skeleton";
    int d = std::max(1,depth);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (enemy.kind=="orc"){ enemy.level=d; enemy.max_hp=18+d*5; enemy.atk=4+d; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (enemy.kind=="troll"){ enemy.level=d; enemy.max_hp=18+d*3; enemy.atk=3+d; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (enemy.kind=="slime"){ enemy.level=d; enemy.max_hp=10+d*2; enemy.atk=3+d; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (enemy.kind=="skeleton"){ enemy.level=d; enemy.max_hp=12+d*3; enemy.atk=4+d; }
    else { enemy.level=d; enemy.max_hp=10+d*2; enemy.atk=3+d; }
    enemy.hp=enemy.max_hp; return enemy;
}
/**
 * Category: Inventory/Items
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static ItemData make_random_item(AppState &app, int col, int row)
{
    int depth = cell_depth(col,row);
    int r = random_in_range(1, 100);
    ItemData it; it.id = app.next_item_id++;

    if (!g_excalibur_spawned && chance_percent(2)){
        g_excalibur_spawned = true;
        return weapon_named(it.id, "Excalibur", 5);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (r <= 35) {
        if (chance_percent(60)) return heal_potion(it.id, 6 + depth * 2);
        else return poison_potion(it.id, 2 + (depth>2?1:0));
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (r <= 60) {
        return gem_item(it.id);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (r <= 80) {
        return weapon_named(it.id, "Steel Dagger", 1);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (r <= 95) {
        return weapon_named(it.id, "Steel Sword", 1);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (r <= 99) {
        return weapon_named(it.id, "Black Greatsword", 3);
    }
    else {
        return gem_item(it.id);
    }
}

// Generate fresh dungeon & ensure at least one enemy/treasure exists, boss bottom-right
/**
 * Category: Dungeon Generation
 * Purpose : Builds or manipulates the runtime dungeon grid.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void generate_dungeon(AppState &app)
{
    g_excalibur_spawned = false;
    for (int row=0; row<DungeonGrid::GridHeight; ++row)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int col=0; col<DungeonGrid::GridWidth;  ++col){
        DungeonCell &cell = app.dungeon.cells[row][col];
        cell.gridLocation = {col,row}; cell.type=RoomType::Empty; cell.visited=false; cell.explored=false;
        cell.has_enemy=false; cell.has_item=false; cell.enemy=EnemyData{}; cell.item=ItemData{};
    }

    app.playerOne.pos = {0,0}; app.playerTwo.pos = {0,0};
    DungeonCell &entrance = app.dungeon.cells[0][0];
    entrance.type=RoomType::Entrance; entrance.visited=true; entrance.explored=true;

    int enemy_count=0, treasure_count=0;
    for (int row=0; row<DungeonGrid::GridHeight; ++row)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int col=0; col<DungeonGrid::GridWidth;  ++col){
        if (row==0 && col==0) continue;
        DungeonCell &cell = app.dungeon.cells[row][col];
        int depth = cell_depth(col,row);
        int roll = random_in_range(1,100);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (roll <= 32){
            cell.type=RoomType::Enemy; cell.has_enemy=true; cell.enemy=make_enemy_for_depth(depth,false); ++enemy_count;
        } else if (roll <= 52){
            cell.type=RoomType::Treasure; cell.has_item=true; cell.item=make_random_item(app,col,row); ++treasure_count;
        } else {
            cell.type=RoomType::Empty;
        }
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (enemy_count==0){
        int r=random_in_range(0,DungeonGrid::GridHeight-1), c=random_in_range(0,DungeonGrid::GridWidth-1);
        if (r==0&&c==0) c=1;
        DungeonCell &cell=app.dungeon.cells[r][c];
        cell.type=RoomType::Enemy; cell.has_enemy=true; cell.enemy=make_enemy_for_depth(cell_depth(c,r),false);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (treasure_count==0){
        int r=random_in_range(0,DungeonGrid::GridHeight-1), c=random_in_range(0,DungeonGrid::GridWidth-1);
        if (r==0&&c==0) c=1;
        DungeonCell &cell=app.dungeon.cells[r][c];
        cell.type=RoomType::Treasure; cell.has_item=true; cell.item=make_random_item(app,c,r);
    }

    // Boss bottom-right
    DungeonCell &boss = app.dungeon.cells[DungeonGrid::GridHeight-1][DungeonGrid::GridWidth-1];
    boss.gridLocation = {DungeonGrid::GridWidth-1, DungeonGrid::GridHeight-1};
    boss.type=RoomType::Boss; boss.has_enemy=true; boss.enemy=make_enemy_for_depth(cell_depth(boss.gridLocation.x,boss.gridLocation.y),true);
}

// Boss persistence guard: if removed by error, restore while not in end states
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void enforce_boss_presence(AppState &app)
{
    if (app.mode==UIMode::Victory || app.mode==UIMode::Defeat) return;
    DungeonCell &boss = app.dungeon.cells[DungeonGrid::GridHeight-1][DungeonGrid::GridWidth-1];
    const int bx = DungeonGrid::GridWidth-1, by = DungeonGrid::GridHeight-1;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (boss.type != RoomType::Boss) {
        boss.type = RoomType::Boss;
        boss.gridLocation = {bx,by};
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (!boss.has_enemy || !boss.enemy.alive) {
        boss.has_enemy = true;
        boss.enemy = make_enemy_for_depth(cell_depth(bx,by), true);
        boss.enemy.alive = true;
        boss.explored = false;
    }
}

// =====[ music helpers + volume ]=============================================
static float g_music_volume = 0.8f; // 0..1
/**
 * Category: Combat Mechanics
 * UI Scope: UI: Settings Panel
 * Purpose : Resolves turns, damage, effects, or cooldowns.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void apply_volume(){ set_music_volume(g_music_volume); }
/**
 * Category: Audio
 * Purpose : Controls music playback, switching, and volume.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void play_music_if_needed(AppState &, music m){ if (!m) return; apply_volume(); if (!music_playing()) play_music(m, 1.0); }
/**
 * Category: Audio
 * Purpose : Controls music playback, switching, and volume.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void switch_to_music(AppState &, music m){ if (music_playing()) stop_music(); if (m){ apply_volume(); play_music(m, 1.0); } }

// =====[ highscore I/O ]=======================================================
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void load_highscore(HighScore &hs)
{
    if (!file_exists(HIGHSCORE_FILE)) return;
    std::ifstream in(HIGHSCORE_FILE);
    if (!in) return;

    string tag; 
    if (!(in >> tag)) return;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (tag == "HSVER") {
        int ver = 0;
        if (!(in >> ver)) return;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

        if (ver == 1) {
            // legacy: BEST_COINS + BEST_NAME
            string k;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            while (in >> k) {
                if (k == "BEST_COINS") in >> hs.best_single;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
                else if (k == "BEST_NAME") { string nm; in >> nm; hs.best_single_name = unescape_name(nm); }
            }
        } else if (ver == 2) {
            string k;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            while (in >> k) {
                if (k == "BEST_SINGLE")       in >> hs.best_single;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
                else if (k == "BEST_SINGLE_NAME") { string nm; in >> nm; hs.best_single_name = unescape_name(nm); }
                else if (k == "BEST_TEAM")    in >> hs.best_team;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
                else if (k == "BEST_TEAM_NAME"){ string nm; in >> nm; hs.best_team_name = unescape_name(nm); }
            }
        }
    } else {
        // No HSVER line? Treat as legacy BEST_COINS BEST_NAME
        // `tag` already holds the first token
        // Try to interpret as BEST_COINS <n>
        if (tag == "BEST_COINS") in >> hs.best_single;
        string k;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        while (in >> k) {
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (k == "BEST_NAME") { string nm; in >> nm; hs.best_single_name = unescape_name(nm); }
        }
    }
}
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void save_highscore_if_best(AppState &app)
{
    const bool coop = app.has_player_two;

    // Compute coins based on mode
    const int  single_score = app.playerOne.coins;
    const int  team_score   = app.playerOne.coins + (coop ? app.playerTwo.coins : 0);

    const string single_name = app.playerOne.display_name;
    const string team_name   = app.party_name.empty() ? "Team" : app.party_name;

    HighScore hs = app.highscore;  // start from current
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (!coop)
    {
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (single_score > hs.best_single)
        {
            hs.best_single = single_score;
            hs.best_single_name = single_name;
        }
    }
    else
    {
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (team_score > hs.best_team)
        {
            hs.best_team = team_score;
            hs.best_team_name = team_name;
        }
    }

    // Write HSVER 2 (and keep backward tags for safety)
    std::ofstream out(HIGHSCORE_FILE, std::ios::out | std::ios::trunc);
    if (!out) return;

    out << "HSVER 2\n";
    out << "BEST_SINGLE " << hs.best_single << "\n";
    out << "BEST_SINGLE_NAME " << escape_name(hs.best_single_name) << "\n";
    out << "BEST_TEAM "   << hs.best_team   << "\n";
    out << "BEST_TEAM_NAME " << escape_name(hs.best_team_name) << "\n";

    app.highscore = hs;  // commit back to memory
}

// =====[ save / load I/O ]=====================================================
// Note: write_save writes a simple tagged format (VER 2).
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool write_save(const AppState &app, const string &path)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) return false;

    out << "VER 2\n";
    out << "MODE " << (app.co_op_enabled ? "MULTI" : "SINGLE") << "\n";
    out << "PNAME " << escape_name(app.party_name) << "\n";
    out << "P1_COINS " << app.playerOne.coins << "\n";
    out << "P2_COINS " << (app.has_player_two ? app.playerTwo.coins : 0) << "\n";


    const PlayerData &player = app.playerOne;

    // Optional metadata (kept for compatibility / future expansion)
    out << "META " << player.level << ' ' << player.hp << ' ' << player.pos.x << ' ' << player.pos.y << '\n';

    // P line: core player block + bag size
    out << "P "
        << player.pos.x << ' ' << player.pos.y << ' '
        << player.level << ' ' << player.exp << ' ' << player.exp_to_next << ' '
        << player.max_hp << ' ' << player.hp << ' '
        << player.base_atk << ' ' << player.equipped_atk_bonus << ' '
        << player.bag.max_slots << '\n';

    // BAG lines (non-empty only)
    for (int i = 0; i < (int)player.bag.slots.size(); ++i)
    {
        const auto &slot = player.bag.slots[i];
        if (slot.is_empty()) continue;

        int kind = (slot.item.kind == ItemKind::Potion) ? 0
                 : (slot.item.kind == ItemKind::Weapon) ? 1
                 : 2;
        int pot  = (slot.item.ptype == PotionKind::Heal) ? 0 : 1;

        out << "BAG " << i << ' '
            << kind << ' '
            << slot.item.id << ' '
            << escape_name(slot.item.name) << ' '
            << slot.item.heal << ' '
            << slot.item.atk  << ' '
            << pot << ' '
            << slot.count << '\n';
    }

    // Grid
    for (int r = 0; r < DungeonGrid::GridHeight; ++r)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int c = 0; c < DungeonGrid::GridWidth;  ++c)
    {
        const auto &cell = app.dungeon.cells[r][c];
        out << "R " << c << ' ' << r << ' '
            << (int)cell.type << ' '
            << (cell.visited ? 1 : 0)  << ' '
            << (cell.explored ? 1 : 0) << ' '
            << (cell.has_enemy ? 1 : 0) << ' '
            << (cell.has_item ? 1 : 0) << '\n';
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

        if (cell.has_enemy)
        {
            out << "E "
                << cell.enemy.kind << ' '
                << cell.enemy.level << ' '
                << cell.enemy.max_hp << ' '
                << cell.enemy.hp << ' '
                << cell.enemy.atk << ' '
                << (cell.enemy.alive ? 1 : 0) << '\n';
        }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

        if (cell.has_item)
        {
            int kind = (cell.item.kind == ItemKind::Potion) ? 0
                     : (cell.item.kind == ItemKind::Weapon) ? 1
                     : 2;
            int pot  = (cell.item.ptype == PotionKind::Heal) ? 0 : 1;

            out << "T "
                << cell.item.id << ' '
                << kind << ' '
                << escape_name(cell.item.name) << ' '
                << cell.item.heal << ' '
                << cell.item.atk  << ' '
                << pot << '\n';
        }
    }

    return (bool)out;
}

// Robust reader: mirrors writer, builds into temp, commits on full success
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool read_save(AppState &app, const std::string &path)
{
    std::ifstream in(path);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (!in) { app.saveui.info = S("Load failed: open"); return false; }

    // --- small helpers (local to reader) -------------------------------------
    auto trim = [](std::string s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string();
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    };
    auto starts = [&](const std::string &line, const char *pfx){ return line.rfind(pfx, 0) == 0; };
    auto next_nonempty = [&](std::string &out)->bool {
        while (std::getline(in, out)) {
            out = trim(out);
            if (!out.empty()) return true;
        }
        return false;
    };
    auto fail = [&](const char* stage)->bool {
        app.saveui.info = cat("Load failed: ", stage);
        return false;
    };

    // --- header: "VER <int>" -------------------------------------------------
    std::string tag; int ver = 0;
    if (!(in >> tag) || tag != "VER") return fail("header");
    if (!(in >> ver))                 return fail("version");
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // consume rest of line

    // Build into a copy first (so partial loads don't corrupt running game)
    AppState loaded = app;

    // sane defaults in case header keys are absent
    loaded.co_op_enabled = false;
    loaded.has_player_two = false;
    loaded.party_name = "Party";


    // --- scan header lines until first "P " line ------------------------------
    std::string line;
    if (!next_nonempty(line)) return fail("missing P line");
    while (!starts(line, "P ")) {
        if (starts(line, "MODE")) {
            if (line.find("MULTI") != std::string::npos) {
                loaded.co_op_enabled = true;
                loaded.has_player_two = true;
            } else {
                loaded.co_op_enabled = false;
                loaded.has_player_two = false;
            }
        }
        else if (starts(line, "PNAME")) {
            std::istringstream ss(line.substr(5));
            std::string nm; ss >> nm;
            loaded.party_name = unescape_name(nm);
        }
        else if (starts(line, "COINS")) {
            std::istringstream ss(line.substr(5));
            ss >> loaded.playerOne.coins;   // map old shared coin to P1
            continue;
        }
        // "META" and any unknown tags are tolerated/ignored
        if (!next_nonempty(line)) return fail("missing P line");
    }

    // --- parse "P " (player core + bag size) ---------------------------------
    {
        std::istringstream ps(line.substr(1));  // drop the 'P'
        PlayerData &pl = loaded.playerOne;
        int bag_slots = 0;
        if (!(ps >> pl.pos.x >> pl.pos.y
                 >> pl.level >> pl.exp >> pl.exp_to_next
                 >> pl.max_hp >> pl.hp
                 >> pl.base_atk >> pl.equipped_atk_bonus
                 >> bag_slots))
            return fail("P data");

        init_player_inventory(pl, std::max(1, bag_slots));
        recompute_attack(pl);
    }

    // --- 0..N "BAG" lines (until first non-BAG) ------------------------------
    if (!next_nonempty(line)) return fail("EOF before grid");
    while (starts(line, "BAG")) {
        std::istringstream bs(line.substr(3));
        int idx, kind, id, heal, atk, pot, count;
        std::string name;
        if (!(bs >> idx >> kind >> id >> name >> heal >> atk >> pot >> count))
            return fail("BAG data");

        ItemData it;
        it.id   = id;
        it.name = unescape_name(name);
        it.heal = heal;
        it.atk  = atk;
        it.kind = (kind == 0) ? ItemKind::Potion : (kind == 1) ? ItemKind::Weapon : ItemKind::Treasure;
        it.ptype= (pot  == 0) ? PotionKind::Heal : PotionKind::Poison;

        PlayerData &pl = loaded.playerOne;
        if (idx < 0 || idx >= (int)pl.bag.slots.size()) return fail("BAG index");
        pl.bag.slots[idx] = ItemSlot{ it, count };

        if (!next_nonempty(line)) return fail("EOF before grid");
    }

    // --- grid: exactly H×W "R" blocks; each may be followed by E/T lines -----
    if (!starts(line, "R ")) return fail("R tag");

    bool first_R_consumed = true; // we've already got the first R line in `line`
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int r = 0; r < DungeonGrid::GridHeight; ++r) {
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (int c = 0; c < DungeonGrid::GridWidth; ++c) {
            std::string rl;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (first_R_consumed) { rl = line; first_R_consumed = false; }
            else if (!next_nonempty(rl)) return fail("EOF in grid");

            if (!(rl.rfind("R ",0)==0)) return fail("R tag");

            std::istringstream rs(rl.substr(1));
            int gx, gy, ti, vis, expd, hasE, hasT;
            if (!(rs >> gx >> gy >> ti >> vis >> expd >> hasE >> hasT))
                return fail("R data");
            if (gx < 0 || gx >= DungeonGrid::GridWidth || gy < 0 || gy >= DungeonGrid::GridHeight)
                return fail("R coord");

            DungeonCell cell;
            cell.gridLocation = { gx, gy };
            cell.type      = (RoomType)ti;
            cell.visited   = (vis   != 0);
            cell.explored  = (expd  != 0);
            cell.has_enemy = (hasE  != 0);
            cell.has_item  = (hasT  != 0);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

            if (cell.has_enemy) {
                std::string el;
                if (!next_nonempty(el) || !(el.rfind("E ",0)==0))
                    return fail("E tag");
                std::istringstream es(el.substr(1));
                int aliveInt = 1;
                if (!(es >> cell.enemy.kind
                        >> cell.enemy.level
                        >> cell.enemy.max_hp
                        >> cell.enemy.hp
                        >> cell.enemy.atk
                        >> aliveInt))
                    return fail("E data");
                cell.enemy.alive = (aliveInt != 0);
            }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (cell.has_item) {
                std::string tl;
                if (!next_nonempty(tl) || !(tl.rfind("T ",0)==0))
                    return fail("T tag");
                std::istringstream ts(tl.substr(1));
                int kind, id, heal, atk, pot; std::string name;
                if (!(ts >> id >> kind >> name >> heal >> atk >> pot))
                    return fail("T data");
                cell.item.id   = id;
                cell.item.name = unescape_name(name);
                cell.item.heal = heal;
                cell.item.atk  = atk;
                cell.item.kind = (kind == 0) ? ItemKind::Potion : (kind == 1) ? ItemKind::Weapon : ItemKind::Treasure;
                cell.item.ptype= (pot  == 0) ? PotionKind::Heal   : PotionKind::Poison;
            }

            loaded.dungeon.cells[gy][gx] = cell;
        }
    }
    // (no extra reads between rows; we consumed exactly H×W R-blocks)

    // --- finalize & commit ----------------------------------------------------
    recompute_attack(loaded.playerOne);
    if (loaded.has_player_two) recompute_attack(loaded.playerTwo);

    loaded.mode = UIMode::Exploring;
    loaded.turn = CombatTurn::PlayerTurn;
    loaded.log.clear();
    loaded.in_boss_fight = false;
    loaded.combat_locked = false;

    app = loaded;
    app.saveui.info = S("Loaded OK");
    return true;
}

// =====[ status effects & cooldowns ]========================================
// Why: centralize all status logic so skills & passives remain simple/consistent.

// Enemy debuffs ---------------------------------------------------------------
/**
 * Category: Combat Mechanics
 * Purpose : Resolves turns, damage, effects, or cooldowns.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void apply_stun  (EnemyData &enemy, int turns){ enemy.status.stun_turns   = std::max(enemy.status.stun_turns,  turns); }
/**
 * Category: Combat Mechanics
 * Purpose : Resolves turns, damage, effects, or cooldowns.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void apply_burn  (EnemyData &enemy, int turns, int dps){ enemy.status.burn_turns = std::max(enemy.status.burn_turns,turns); enemy.status.burn_dps  = std::max(enemy.status.burn_dps, dps); }
/**
 * Category: Combat Mechanics
 * Purpose : Resolves turns, damage, effects, or cooldowns.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void apply_poison(EnemyData &enemy, int turns, int dps){ enemy.status.poison_turns= std::max(enemy.status.poison_turns,turns); enemy.status.poison_dps= std::max(enemy.status.poison_dps,dps); }

// Player damage-over-time (used by skeleton "Decay") -------------------------
/**
 * Category: Combat Mechanics
 * Purpose : Resolves turns, damage, effects, or cooldowns.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void apply_decay(PlayerData &player, int turns, int dps){
    player.pstatus.decay_turns = std::max(player.pstatus.decay_turns, turns);
    player.pstatus.decay_dps   = std::max(player.pstatus.decay_dps,   dps);
}
// =====[ tiny HP clamp ]=======================================================
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static inline void clamp_hp(PlayerData &player){ if (player.hp < 0) player.hp = 0; }

// Per-turn ticks --------------------------------------------------------------
/**
 * Category: Enemy System
 * Purpose : Creates/updates enemy data and behaviors.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void tick_enemy_status(AppState &app, EnemyData &enemy){
    if (!enemy.alive) return;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (enemy.status.burn_turns>0){
        enemy.status.burn_turns--;
        int dmg = std::max(1, enemy.status.burn_dps);
        enemy.hp -= dmg; push_log(app, "Burn deals " + to_str(dmg) + ".");
        if (enemy.hp<=0) enemy.alive=false;
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (enemy.status.poison_turns>0 && enemy.alive){
        enemy.status.poison_turns--;
        int dmg = std::max(1, enemy.status.poison_dps);
        enemy.hp -= dmg; push_log(app, "Poison ticks for " + to_str(dmg) + ".");
        if (enemy.hp<=0) enemy.alive=false;
    }
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void tick_player_decay(AppState &app){
    PlayerData &player= hero_by_index(app, app.combat_hero_index);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (player.pstatus.decay_turns>0){
        player.pstatus.decay_turns--;
        int dmg = std::max(1, player.pstatus.decay_dps);
        player.hp -= dmg;
        clamp_hp(player);
        push_log(app, "Decay saps " + to_str(dmg) + " HP.");

    }
}

// Accuracy & cooldown helpers (≥3-turn skills) -------------------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool roll_miss_10() { return chance_percent(10); }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void set_skill_cooldown(PlayerData &player, int desired_turns){ player.skill_cooldown = std::max(3, desired_turns); }

// =====[ drawing: player, inventory, grid, log ]==============================
// Purpose: keep rendering logic localized & predictable (panels → grid → log).
/**
 * Category: Rendering/UI
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_player_panel(const AppState &app, float x, float y, float w)
{
    const auto &p1=app.playerOne; float yy=y;
    string p1Header = app.has_player_two ? string("Player 1") : (string("Player: ")+p1.display_name);
    draw_text(p1Header, COLOR_YELLOW, x, yy); yy+=18;
    draw_text("Class: "+class_label(p1.cls), COLOR_WHITE, x, yy); yy+=16;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (p1.cls==CharacterClass::Wizard){ draw_text("Element: "+element_label(p1.element), COLOR_WHITE, x, yy); yy+=16; }
    string wep= p1.has_equipped_weapon ? (p1.equipped_weapon.name+" (+"+to_str(p1.equipped_weapon.atk)+")") : "None";
    draw_text("Weapon: "+wep, COLOR_WHITE, x, yy); yy+=16;
    draw_text("Lvl: "+to_str(p1.level)+"  ATK: "+to_str(p1.atk), COLOR_WHITE, x, yy); yy+=16;

    draw_text("HP", COLOR_WHITE, x, yy);
    draw_bar_with_border(x+40, yy, w-60, 12, (float)p1.hp/(float)p1.max_hp);
    draw_text(to_str(p1.hp)+"/"+to_str(p1.max_hp), COLOR_WHITE, x+w-70, yy-1); yy+=20;

    draw_text("EXP", COLOR_WHITE, x, yy);
    draw_bar_with_border(x+40, yy, w-60, 12, (float)p1.exp/(float)p1.exp_to_next);
    draw_text(to_str(p1.exp)+"/"+to_str(p1.exp_to_next), COLOR_WHITE, x+w-70, yy-1); yy+=20;

    // P1 coins (always shown)
    draw_text((app.has_player_two ? "P1 Coins: " : "Coins: ") + to_str(app.playerOne.coins), COLOR_YELLOW, x, yy);
    yy += 18;

    // P2 block ONLY in co-op
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (app.has_player_two)
    {
        const auto &p2 = app.playerTwo;
        float y2 = y + 160;

        draw_text("Player 2", COLOR_YELLOW, x, y2); y2 += 18;
        draw_text("Class: " + class_label(p2.cls), COLOR_WHITE, x, y2); y2 += 16;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (p2.cls == CharacterClass::Wizard){
            draw_text("Element: " + element_label(p2.element), COLOR_WHITE, x, y2);
            y2 += 16;
        }
        string wep2 = p2.has_equipped_weapon ? (p2.equipped_weapon.name + " (+" + to_str(p2.equipped_weapon.atk) + ")") : "None";
        draw_text("Weapon: " + wep2, COLOR_WHITE, x, y2); y2 += 16;
        draw_text("Lvl: " + to_str(p2.level) + "  ATK: " + to_str(p2.atk), COLOR_WHITE, x, y2); y2 += 16;

        draw_text("HP", COLOR_WHITE, x, y2);
        draw_bar_with_border(x+40, y2, w-60, 12, (float)p2.hp/(float)p2.max_hp);
        draw_text(to_str(p2.hp)+"/"+to_str(p2.max_hp), COLOR_WHITE, x+w-70, y2-1);

        // P2 coins (co-op only)
        draw_text("P2 Coins: " + to_str(app.playerTwo.coins), COLOR_YELLOW, x, yy);
        yy += 18;
    }
}

static string kind_label(ItemKind k); // already declared above, keep forward
/**
 * Category: Rendering/UI
 * UI Scope: UI: Inventory Panel
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_inventory_panel(const AppState &app,
                                 const PlayerData &who,
                                 float ox, float oy)
{
    const float cell = 56.0f;
    const float pad  = 4.0f;
    const float inner = cell - pad*2;
    const int   cols = 4;

    const auto &bag = who.bag;
    int used=0; for (const auto &slot:bag.slots) if (!slot.is_empty()) ++used;

    fill_rectangle(rgba_color(20,20,20,120), ox-6, oy-22, 260, 20);
    draw_text("Inventory ("+to_str(used)+"/"+to_str(bag.max_slots)+")", COLOR_YELLOW, ox, oy-12);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    for (int i=0;i<bag.max_slots;++i){
        float x = ox + (i % cols) * cell;
        float y = oy + (i / cols) * cell;

        fill_rectangle(rgba_color(30,30,30,140), x, y, cell, cell);
        draw_rectangle(COLOR_SILVER, x, y, cell, cell);

        if (i<(int)bag.slots.size() && !bag.slots[i].is_empty()){
            const auto &slot = bag.slots[i];
            const auto &it   = slot.item;

            color face =
                (it.kind==ItemKind::Potion)?((it.ptype==PotionKind::Heal)?COLOR_RED:COLOR_PURPLE):
                (it.kind==ItemKind::Weapon)?COLOR_ORANGE:COLOR_PURPLE;

            fill_rectangle(face, x+pad, y+pad, inner, inner);

            float text_x = x + pad + 2;
            float text_w = inner - 4.0f;
            float line_h = 12.0f;

            draw_wrapped_text(it.name, COLOR_WHITE, text_x, y + pad + 2, text_w, 2, line_h);
            draw_wrapped_text(kind_label(it.kind), COLOR_WHITE, text_x, y + pad + 2 + 2*line_h, text_w, 1, line_h);

            string stat="";
            if (it.kind==ItemKind::Potion && it.ptype==PotionKind::Heal) stat = "Heal +" + to_str(it.heal);
            else if (it.kind==ItemKind::Potion && it.ptype==PotionKind::Poison) stat = "Poison " + to_str(it.heal) + "/t";
            else if (it.kind==ItemKind::Weapon) stat = "ATK +" + to_str(it.atk);

            if (!stat.empty())
                draw_wrapped_text(stat, COLOR_WHITE, text_x, y + pad + inner - (line_h + 2), text_w, 1, line_h);

            if (slot.count>1) draw_text("x"+to_str(slot.count), COLOR_WHITE, x + pad + inner - 18, y + pad + 2);
        }
    }

    float footer_y = oy + (bag.max_slots/4)*cell + 8;
    draw_text("Use: 1..8 or Left-Click  -  Right-Click = Sell", COLOR_SILVER, ox, footer_y);
}
/**
 * Category: Rendering/UI
 * UI Scope: UI: Grid/World
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_grid(const AppState &app, float ox, float oy)
{
    const float cs=48;
    for (int r=0;r<DungeonGrid::GridHeight;++r)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int c=0;c<DungeonGrid::GridWidth;++c){
        const auto &cell=app.dungeon.cells[r][c];
        color col=COLOR_BLACK;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (cell.visited){
            col = cell.explored?COLOR_GREEN:COLOR_DARK_GRAY;
            if (cell.type==RoomType::Entrance) col=COLOR_GREEN;
            if (cell.type==RoomType::Treasure && cell.has_item) col=COLOR_PURPLE; // treasure highlight
            if (cell.type==RoomType::Enemy && cell.has_enemy) col=COLOR_RED;
            if (cell.type==RoomType::Boss && cell.has_enemy) col=COLOR_PURPLE;
        }
        float x=ox+c*cs, y=oy+r*cs;
        fill_rectangle(col, x,y,cs-4,cs-4);
        draw_rectangle(COLOR_BLACK, x,y,cs-4,cs-4);

        if (app.playerOne.pos.x==c && app.playerOne.pos.y==r) fill_circle(class_dot_color(app.playerOne.cls), x+(cs-4)/2, y+(cs-4)/2, 8);
        if (app.has_player_two && app.playerTwo.pos.x==c && app.playerTwo.pos.y==r) fill_circle(class_dot_color(app.playerTwo.cls), x+(cs-4)/2-10, y+(cs-4)/2+10, 6);
    }

    draw_text("Pos: ("+to_str(app.playerOne.pos.x)+","+to_str(app.playerOne.pos.y)+")",
              COLOR_SILVER, ox, oy + DungeonGrid::GridHeight*cs + 6);
    draw_text("Move: WASD / Arrows  |  Save: F5  |  Load: F9/L ",
          COLOR_SILVER, ox, oy + DungeonGrid::GridHeight*cs + 28);
    draw_text("Pause: P  |  Log: H  |  Shop: J",
          COLOR_SILVER, ox, oy + DungeonGrid::GridHeight*cs + 40);

}
/**
 * Category: Rendering/UI
 * UI Scope: UI: Log Viewer
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_log_box(const AppState &app, float x, float y, float w, float h)
{
    draw_text("Log (H=Full Screen)", COLOR_YELLOW, x, y-14);
    draw_rectangle(COLOR_SILVER, x,y,w,h);
    float yy=y+6;
    for (int i=(int)app.log.size()-1;i>=0;--i){
        draw_text(app.log[i], COLOR_WHITE, x+6, yy);
        yy+=18; if (yy>y+h-18) break;
    }
}

// =====[ full-screen log viewer ]=============================================
// Why: readable history when the compact on-screen log overflows.
/**
 * Category: Rendering/UI
 * UI Scope: UI: Log Viewer
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void draw_log_viewer(AppState &app, int W, int H)
{
    fill_rectangle(rgba_color(0,0,0,220), 0,0,W,H);
    draw_text("Dialog Log  (H to close | PgUp/PgDn scroll | Mouse Wheel | Home/End)",
              COLOR_YELLOW, 24, 16);

    // Toggle close on a single H press
    if (key_typed(H_KEY)) {
        app.logview.open = false;
        return;
    }

    const float vx = 24, vy = 44, vw = W - 48, vh = H - 88;
    draw_rectangle(COLOR_SILVER, vx, vy, vw, vh);

    // Mouse wheel scrolling (positive y = scroll up)
    vector_2d wh = mouse_wheel_scroll();
    if (wh.y > 0.1f) app.logview.top_index += (int)(wh.y * 2);
    if (wh.y < -0.1f) app.logview.top_index = std::max(0, app.logview.top_index - (int)(-wh.y * 2));

    // Keys
    if (key_typed(PAGE_UP_KEY))   app.logview.top_index += 6;
    if (key_typed(PAGE_DOWN_KEY)) app.logview.top_index = std::max(0, app.logview.top_index - 6);
    if (key_typed(HOME_KEY))      app.logview.top_index = (int)app.log.size();
    if (key_typed(END_KEY))       app.logview.top_index = 0;

    // Clamp & render
    int n = (int)app.log.size();
    app.logview.top_index = std::max(0, std::min(app.logview.top_index, std::max(0, n)));

    float line_h = 18.0f;
    int max_lines = (int)std::floor((vh - 12) / line_h);
    int visible_from = std::max(0, n - (app.logview.top_index + max_lines));
    int visible_to   = std::min(n, visible_from + max_lines);

    float yy = vy + 8;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int i=visible_from; i<visible_to; ++i){
        draw_text(app.log[i], COLOR_WHITE, vx + 8, yy);
        yy += line_h;
    }
}

// =====[ volume slider widget ]================================================
// Why: immediate music loudness control in Main & Pause menus (0..100%).
/**
 * Category: Rendering/UI
 * UI Scope: UI: Settings Panel
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool draw_volume_slider(float x, float y, float w, float h, float &value01)
{
    fill_rectangle(rgba_color(40,40,40,200), x, y, w, h);
    draw_rectangle(COLOR_SILVER, x, y, w, h);

    float kx = x + value01 * (w - 14.0f);
    fill_rectangle(COLOR_YELLOW, kx, y+2, 14.0f, h-4);

    draw_text(cat("Music: ", (int)std::round(value01*100), "%"), COLOR_SILVER, x, y - 18);

    bool changed = false;
    const float mx = mouse_x(), my = mouse_y();
    const bool  down = mouse_down(LEFT_BUTTON);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (down && mx >= x && mx <= x+w && my >= y && my <= y+h){
        float t = (mx - x) / std::max(1.0f, w);
        float nv = std::max(0.0f, std::min(1.0f, t));
        if (std::fabs(nv - value01) > 1e-3f){
            value01 = nv; apply_volume(); changed = true;
        }
    }
    return changed;
}
/**
 * Category: UI Controls
 * UI Scope: UI: Save/Load Modal
 * Purpose : Implements menus, modals, and interactive widgets.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool show_settings_modal(AppState &app)
{
    const int W = 1024, H = 640;
    const float pw = 420, ph = 220;
    const float px = (W - pw) * 0.5f;
    const float py = (H - ph) * 0.5f;

    fill_rectangle(rgba_color(0,0,0,180), 0,0,W,H);
    fill_rectangle(COLOR_WHITE, px, py, pw, ph);
    draw_rectangle(COLOR_BLACK, px, py, pw, ph);

    draw_text("Settings", COLOR_BLACK, px + 16, py + 14);

    // Volume slider
    const float sx = px + 40;
    const float sy = py + 70;
    const float sw = pw - 80;
    const float sh = 14;
    draw_volume_slider(sx, sy, sw, sh, g_music_volume);

    draw_text("ESC or Close to return", COLOR_SILVER, px + 16, py + ph - 28);

    // Close button
    Button closeBtn{ px + pw - 100, py + ph - 46, 84, 32, "Close" };
    const float mx = mouse_x(), my = mouse_y();
    draw_button(closeBtn, point_in_button(mx,my,closeBtn));

    // Interactions
    if (key_typed(ESCAPE_KEY) || (mouse_clicked(LEFT_BUTTON) && point_in_button(mx,my,closeBtn))) {
        app.mode = app.settings_return_to;
        return true;
    }
    return true; // modal blocks
}

// =====[ shop ]================================================================
// Simple numeric purchase UI toggled with 'J' anywhere outside modals.
struct ShopItem { string name; int cost; ItemData proto; };
/**
 * Category: General Helper
 * UI Scope: UI: Shop
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static vector<ShopItem> build_shop_list(AppState &app){
    vector<ShopItem> v;
    v.push_back({"Steel Dagger (+1)", 10, weapon_named(app.next_item_id, "Steel Dagger", 1)});
    v.push_back({"Heal Potion",        5,  heal_potion(app.next_item_id, 8)});
    v.push_back({"Poison Potion",      7,  poison_potion(app.next_item_id, 2)});
    v.push_back({"Gem",               20,  gem_item(app.next_item_id)});
    return v;
}

// Optional tiny icon helper (put once near your other small draw helpers)
/**
 * Category: Rendering/UI
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void draw_coin_icon(float cx, float cy, float r){
    fill_circle(COLOR_YELLOW, cx, cy, r);
    draw_circle(COLOR_ORANGE, cx, cy, r);
    draw_text("$", COLOR_BROWN, cx-3, cy-6);
}
/**
 * Category: General Helper
 * UI Scope: UI: Shop
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static inline int& shop_current_purse(AppState &app){
    if (app.has_player_two && app.shop_buyer==2) return app.playerTwo.coins;
    return app.playerOne.coins;
}

// Draw + handle the Shop as a modal. Returns true to block the frame (always).
/**
 * Category: Rendering/UI
 * UI Scope: UI: Save/Load Modal
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool draw_shop_modal(AppState &app)
{
    // ---------- INPUT: quick closes & buyer switch ----------
    if (app.has_player_two && key_typed(TAB_KEY)) {
        app.shop_buyer = (app.shop_buyer == 1 ? 2 : 1);
        push_log(app, app.shop_buyer == 1 ? "Shop target: Player 1" : "Shop target: Player 2");
    }
    if (key_typed(J_KEY) || key_typed(ESCAPE_KEY)) {
        app.mode = UIMode::Exploring;
        return true;  // consume the frame
    }

    // ---------- DATA ----------
    auto items = build_shop_list(app);
    const float W = 560, H = 400;
    const float X = (1024 - W) * 0.5f;
    const float Y = (640  - H) * 0.5f;

    // ---------- BACKDROP ----------
    fill_rectangle(rgba_color(0,0,0,180), 0, 0, 1024, 640);     // dim
    fill_rectangle(rgba_color(0,0,0,70),  X + 6, Y + 8, W, H);  // soft shadow

    // ---------- PANEL ----------
    fill_rectangle(rgba_color(245,245,245,255), X, Y, W, H);
    draw_rectangle(COLOR_BLACK, X, Y, W, H);

    // Title bar
    const float th = 44;
    fill_rectangle(rgba_color(30,30,30,255), X, Y, W, th);
    draw_text("SHOP", COLOR_WHITE, X + 16, Y + 12);

    // Coin counter (top-right) — current buyer’s coins
    {
        const float cx = X + W - 130;
        const float cy = Y + 12;
        // Reuse your existing draw_coin_icon(...)
        draw_coin_icon(cx, cy + 6, 9.0f);
        int &purse = shop_current_purse(app);
        draw_text(cat(" ", purse), COLOR_WHITE, cx + 16, cy + 2);
    }

    // Buyer + hint
    draw_text(
        cat("Buying for: ", current_buyer_label(app),
            app.has_player_two ? "   (TAB to switch)   |   J = close" : "   |   J = close"),
        COLOR_SILVER, X + 16, Y + th + 10
    );

    // Optional: show both purses in co-op as a subtle header line
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (app.has_player_two) {
        string both = cat("P1: ", app.playerOne.coins, "   P2: ", app.playerTwo.coins);
        draw_text(both, COLOR_DARK_GRAY, X + 32, Y + th + 28);
    }

    // ---------- LIST ----------
    float listX = X + 16;
    float listY = Y + th + (app.has_player_two ? 46 : 36);
    float rowH  = 40;

    draw_text("Item",  COLOR_DARK_GRAY, listX,           listY - 18);
    draw_text("Price", COLOR_DARK_GRAY, listX + 360,     listY - 18);
    draw_text("Key",   COLOR_DARK_GRAY, listX + 460,     listY - 18);

    const float mx = mouse_x(), my = mouse_y();
    const bool  click = mouse_clicked(LEFT_BUTTON);

    int hoverIndex = -1;

    for (int i = 0; i < (int)items.size(); ++i)
    {
        float ry = listY + i * rowH;

        // row rect
        color base  = rgba_color(255,255,255,255);
        color bord  = rgba_color(200,200,200,255);
        bool over = (mx >= listX - 4 && mx <= listX - 4 + W - 32 && my >= ry - 4 && my <= ry - 4 + rowH);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (over) { base = rgba_color(235,235,235,255); bord = rgba_color(160,160,160,255); hoverIndex = i; }

        fill_rectangle(base, listX - 4, ry - 4, W - 32, rowH);
        draw_rectangle(bord, listX - 4, ry - 4, W - 32, rowH);

        // name
        draw_text(items[i].name, COLOR_BLACK, listX, ry + 8);

        // price (with coin icon)
        float px = listX + 360;
        draw_coin_icon(px, ry + 12, 8.0f);
        draw_text(cat(" ", items[i].cost), COLOR_BLACK, px + 14, ry + 6);

        // key hint
        draw_text(cat(i + 1), COLOR_DARK_GRAY, listX + 465, ry + 6);

        // BUY via mouse
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (click && over) {
            int &purse = shop_current_purse(app);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (purse >= items[i].cost){
                ItemData it = items[i].proto; it.id = app.next_item_id++;
                PlayerData &buyer = (app.has_player_two && app.shop_buyer==2) ? app.playerTwo : app.playerOne;
                if (add_item(buyer.bag, it, 1)){ purse -= items[i].cost; push_log(app, "Bought " + items[i].name + "."); }
                else push_log(app, "Inventory full!");
            } else push_log(app, "Not enough coins.");
        }
    }

    // BUY via number keys
    for (int i = 0; i < (int)items.size(); ++i) {
        if (key_typed((key_code)((int)NUM_1_KEY + i))) {
            int &purse = shop_current_purse(app);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (purse >= items[i].cost){
                ItemData it = items[i].proto; it.id = app.next_item_id++;
                PlayerData &buyer = (app.has_player_two && app.shop_buyer==2) ? app.playerTwo : app.playerOne;
                if (add_item(buyer.bag, it, 1)){ purse -= items[i].cost; push_log(app, "Bought " + items[i].name + "."); }
                else push_log(app, "Inventory full!");
            } else push_log(app, "Not enough coins.");
        }
    }

    return true; // modal blocks the rest of UI
}

// =====[ movement & world interaction ]========================================
/**
 * Category: Dungeon Generation
 * Purpose : Builds or manipulates the runtime dungeon grid.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool can_move_to_cell(int x,int y){ return x>=0&&y>=0&&x<DungeonGrid::GridWidth&&y<DungeonGrid::GridHeight; }

// Lightweight overworld enemy drift
/**
 * Category: State Update / Input
 * Purpose : Processes input and advances UI/game state.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void update_enemy_ai_outside_combat(AppState &app)
{
    if (app.mode != UIMode::Exploring) return;
    static int tick = 0; tick++; if (tick % 12 != 0) return;  // slow

    bool moved[DungeonGrid::GridHeight][DungeonGrid::GridWidth]={{false}};
    for (int r=0;r<DungeonGrid::GridHeight;++r)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int c=0;c<DungeonGrid::GridWidth;++c)
    {
        DungeonCell &cell = app.dungeon.cells[r][c];
        if (!cell.has_enemy || moved[r][c]) continue;
        if (cell.type == RoomType::Boss) continue;
        if (!chance_percent(25)) continue;

        int dirs[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (int i=0;i<4;++i){ int j=random_in_range(i,3); std::swap(dirs[i][0],dirs[j][0]); std::swap(dirs[i][1],dirs[j][1]); }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (int k=0;k<4;++k){
            int nx=c+dirs[k][0], ny=r+dirs[k][1];
            if (nx<0||ny<0||nx>=DungeonGrid::GridWidth||ny>=DungeonGrid::GridHeight) continue;
            DungeonCell &dst=app.dungeon.cells[ny][nx];
            if (dst.has_enemy) continue;
            if ((app.playerOne.pos.x==nx && app.playerOne.pos.y==ny) ||
                (app.has_player_two && app.playerTwo.pos.x==nx && app.playerTwo.pos.y==ny)) continue;
            dst.has_enemy=true; dst.enemy=cell.enemy;
            if (dst.type==RoomType::Empty) dst.type=RoomType::Enemy;
            cell.has_enemy=false; moved[ny][nx]=true;
            break;
        }
    }
}

// Entering a cell triggers loot pickup or combat start ------------------------
/**
 * Category: Dungeon Generation
 * Purpose : Builds or manipulates the runtime dungeon grid.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static inline DungeonCell& combat_cell(AppState &app){ return app.dungeon.cells[app.combat_cy][app.combat_cx]; }
/**
 * Category: Dungeon Generation
 * Purpose : Builds or manipulates the runtime dungeon grid.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void resolve_cell_entry(AppState &app, PlayerData &mover)
{
    DungeonCell &cell=app.dungeon.cells[mover.pos.y][mover.pos.x];
    cell.visited=true;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (cell.type==RoomType::Treasure && cell.has_item){
        if (add_item(mover.bag, cell.item, 1)){   // <-- to whoever moved
            push_log(app, "Found "+cell.item.name+" ("+kind_label(cell.item.kind)+")");
            cell.has_item=false; cell.explored=true;
        } else push_log(app,"Inventory full!");
    }


    if ((cell.type==RoomType::Enemy||cell.type==RoomType::Boss) && cell.has_enemy){
        app.mode=UIMode::Combat; app.log.clear(); app.turn=CombatTurn::PlayerTurn;
        app.in_boss_fight = (cell.type==RoomType::Boss);
        app.combat_hero_index = app.active_player_index;  // <- only this player fights
        app.turn = CombatTurn::PlayerTurn;

        // Lock combat to the engager's tile to stabilize co-op
        app.combat_locked = true; app.combat_cx = mover.pos.x; app.combat_cy = mover.pos.y;
        switch_to_music(app, app.in_boss_fight ? app.music_boss : app.music_battle);
        push_log(app, "A wild "+enemy_display_name(cell.enemy.kind)+" appears!");
    }
}

// Tick Decay for BOTH players when we're outside combat (C2: each step)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void tick_decay_on_step(AppState &app){
    auto tick_one = [&](PlayerData &player, const char* who){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (player.pstatus.decay_turns > 0){
            player.pstatus.decay_turns--;
            int dmg = std::max(1, player.pstatus.decay_dps);
            player.hp -= dmg; clamp_hp(player);
            push_log(app, std::string("Decay saps ") + who + " for " + to_str(dmg) + " HP.");
        }
    };
    tick_one(app.playerOne, "P1");
    if (app.has_player_two) tick_one(app.playerTwo, "P2");

    // If both are dead during exploration, it’s game over
    bool p1_dead = !app.playerOne.alive();
    bool p2_dead = !app.has_player_two ? true : !app.playerTwo.alive();
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (p1_dead && p2_dead){
        app.mode = UIMode::Defeat;
        switch_to_music(app, app.music_defeat);
    }
}

// Attempt movement (WASD for P1, arrows for P2) -------------------------------
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void try_move_player(AppState &app, PlayerData &mover, int dx,int dy)
{
    if (app.mode!=UIMode::Exploring) return;
    int nx=mover.pos.x+dx, ny=mover.pos.y+dy; if (!can_move_to_cell(nx,ny)) return;

    // Make the mover the active player for things like Shop
    app.active_player_index = (&mover == &app.playerOne) ? 1 : 2;

    mover.pos={nx,ny};
    // NEW: decay ticks once per move while exploring
    if (app.mode == UIMode::Exploring) tick_decay_on_step(app);

    resolve_cell_entry(app, mover);
    update_enemy_ai_outside_combat(app);
    if (app.playerOne.skill_cooldown>0) app.playerOne.skill_cooldown--;
    if (app.playerTwo.skill_cooldown>0) app.playerTwo.skill_cooldown--;
}

// =====[ XP, coins, and combat turn helpers ]==================================
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static int current_depth_for_player(const AppState &app){
    const auto &player=hero_by_index(app, app.combat_hero_index); return (player.pos.x + player.pos.y) / 3;
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void grant_experience(AppState &app, int amt){
    PlayerData &player=hero_by_index(app, app.combat_hero_index); player.exp+=amt;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    while (player.exp>=player.exp_to_next){
        player.exp -= player.exp_to_next; player.level += 1; player.exp_to_next = int(player.exp_to_next*1.35f) + 5;
        player.max_hp += 4; player.base_atk += 1; recompute_attack(player); player.hp = std::min(player.max_hp, player.hp + 6);
        push_log(app,"Level up! Now level "+to_str(player.level));
    }
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void drop_coins_on_defeat(AppState &app)
{
    if (!chance_percent(35)) return;

    int base  = random_in_range(1, 3);
    int depth = current_depth_for_player(app);
    int drop  = base + std::max(0, depth);

    if (is_multiplayer(app))
    {
        // Give coins to whichever hero is currently active in combat
        PlayerData &who = (app.active_player_index==1 ? app.playerOne : app.playerTwo);
        who.coins += drop;
        who.lifetime_earned += drop;
    }
    else
    {
        // Singleplayer: P1 only
        app.playerOne.coins += drop;
        app.playerOne.lifetime_earned += drop;
    }

    push_log(app, "Found " + to_str(drop) + " coins.");
}
/**
 * Category: Combat Mechanics
 * Purpose : Resolves turns, damage, effects, or cooldowns.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void exit_combat_to_explore(AppState &app){
    app.mode=UIMode::Exploring; app.in_boss_fight=false; switch_to_music(app, app.music_explore);
    app.combat_locked=false;
}
/**
 * Category: Enemy System
 * Purpose : Creates/updates enemy data and behaviors.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void on_enemy_killed(AppState &app, DungeonCell &cell)
{
    cell.enemy.alive=false; cell.has_enemy=false; cell.explored=true;
    int xp=(cell.type==RoomType::Boss)?25:(6+random_in_range(0,4));
    push_log(app,"Enemy defeated!");
    grant_experience(app,xp); drop_coins_on_defeat(app);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (cell.type==RoomType::Boss){ app.mode=UIMode::Victory; switch_to_music(app, app.music_victory); }
    else exit_combat_to_explore(app);
}
/**
 * Category: Enemy System
 * Purpose : Creates/updates enemy data and behaviors.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void end_player_turn_to_enemy(AppState &app)
{
    PlayerData &player = hero_by_index(app, app.combat_hero_index);

    if (player.skill_cooldown > 0) player.skill_cooldown--;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (player.buff.atk_bonus_turns > 0) {
        player.buff.atk_bonus_turns--;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (player.buff.atk_bonus_turns == 0) { player.buff.atk_bonus_amount = 0; recompute_attack(player); }
    }

    // Decay ticks at end of the hero's turn
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (player.pstatus.decay_turns > 0) {
        player.pstatus.decay_turns--;
        int dmg = std::max(1, player.pstatus.decay_dps);
        player.hp -= dmg;
        push_log(app, "Decay saps " + to_str(dmg) + " HP.");

        // >>> instant defeat if the engaged hero dies
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (player.hp <= 0) {
            app.mode = UIMode::Defeat;
            switch_to_music(app, app.music_defeat);
            return;
        }
    }

    app.turn = CombatTurn::EnemyTurn;
}

// =====[ player actions ]======================================================
// Cell fetch that respects combat lock
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static inline DungeonCell& locked_or_here(AppState &app, const PlayerData &player){
    return app.combat_locked ? combat_cell(app) : app.dungeon.cells[player.pos.y][player.pos.x];
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void player_attack(AppState &app)
{
    if (app.turn!=CombatTurn::PlayerTurn) return;
    PlayerData &player = hero_by_index(app, app.combat_hero_index);
    DungeonCell &cell  = locked_or_here(app, player);
    if (!cell.has_enemy||!cell.enemy.alive) return;

    if (roll_miss_10()) {
        push_log(app, "You swing and miss!");
        end_player_turn_to_enemy(app);
        return;
    }
    int damage = std::max(1, player.atk + (chance_percent(15)?2:0) + random_in_range(0,2));

    // Wizard passives
    if (player.cls==CharacterClass::Wizard && player.element==Element::Fire && chance_percent(20)){
        apply_burn(cell.enemy,2,2); push_log(app,"Fire passive burns the enemy.");
    }
    if (player.cls==CharacterClass::Wizard && player.element==Element::Ice && chance_percent(15)){
        apply_stun(cell.enemy,1); push_log(app,"Ice passive chills (stun).");
    }

    cell.enemy.hp -= damage; push_log(app,"Hit for "+to_str(damage)+".");
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (cell.enemy.hp<=0) { on_enemy_killed(app, cell); return; }
    end_player_turn_to_enemy(app);
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void player_try_flee(AppState &app){
    if (app.turn!=CombatTurn::PlayerTurn) return;
    if (chance_percent(55)){ push_log(app,"You fled."); exit_combat_to_explore(app); }
    else { push_log(app,"Failed to flee!"); end_player_turn_to_enemy(app); }
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void player_primary_skill(AppState &app)
{
    if (app.turn!=CombatTurn::PlayerTurn) return;
    PlayerData &player=hero_by_index(app, app.combat_hero_index); DungeonCell &cell=locked_or_here(app, player);
    if (!cell.has_enemy||!cell.enemy.alive) return;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (player.skill_cooldown>0){ push_log(app,"Skill on cooldown."); return; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (player.cls==CharacterClass::Wizard){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (player.element==Element::Ice){ apply_stun(cell.enemy,2); push_log(app,"Ice Nova! (stun 2)"); }
        else { int dps=3+random_in_range(0,2); apply_burn(cell.enemy,3,dps); push_log(app,"Ignite! ("+to_str(dps)+"/turn)"); }
        set_skill_cooldown(player, 3);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (player.cls==CharacterClass::Barbarian){
        if (roll_miss_10()) { push_log(app,"Quick Power whiffs!"); set_skill_cooldown(player,4); end_player_turn_to_enemy(app); return; }
        int damage=player.atk+5+random_in_range(2,4);
        cell.enemy.hp-=damage; push_log(app,"Quick Power for "+to_str(damage)+"!");
        set_skill_cooldown(player,4);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (cell.enemy.hp<=0){ on_enemy_killed(app,cell); return; }
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (player.cls==CharacterClass::Cleric){
        int before=player.hp, heal=7+random_in_range(0,4);
        player.hp=std::min(player.max_hp, player.hp+heal); push_log(app,"Heal +"+to_str(player.hp-before)+" HP.");
        set_skill_cooldown(player,3);
    }
    end_player_turn_to_enemy(app);
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void player_secondary_skill(AppState &app)
{
    if (app.turn!=CombatTurn::PlayerTurn) return;
    PlayerData &player=hero_by_index(app, app.combat_hero_index); DungeonCell &cell=locked_or_here(app, player);
    if (!cell.has_enemy||!cell.enemy.alive) return;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (player.skill_cooldown>0){ push_log(app,"Skill on cooldown."); return; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (player.cls==CharacterClass::Wizard){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (player.element==Element::Ice){ apply_burn(cell.enemy,2,2); push_log(app,"Ember ignites lightly."); }
        else { apply_stun(cell.enemy,1); push_log(app,"Chill stuns 1."); }
        set_skill_cooldown(player,3);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (player.cls==CharacterClass::Barbarian){
        player.buff.atk_bonus_amount=2; player.buff.atk_bonus_turns=3; recompute_attack(player);
        push_log(app,"Rage! +2 ATK for 3 turns."); set_skill_cooldown(player,4);
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (player.cls==CharacterClass::Cleric){
        if (roll_miss_10()) { push_log(app,"Divine Smite misses!"); set_skill_cooldown(player,3); end_player_turn_to_enemy(app); return; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (cell.enemy.kind=="skeleton"){ cell.enemy.hp=0; on_enemy_killed(app,cell); return; }
        int holy=player.atk+3+random_in_range(0,2); cell.enemy.hp-=holy;
        push_log(app,"Divine Smite deals "+to_str(holy)+"."); if (cell.enemy.hp<=0){ on_enemy_killed(app,cell); return; }
        set_skill_cooldown(player,3);
    }
    end_player_turn_to_enemy(app);
}
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool consume_first_poison_potion(AppState &, PlayerData &player){
    Inventory &bag= player.bag;
    for (int i=0;i<(int)bag.slots.size();++i){
        if (!bag.slots[i].is_empty() && bag.slots[i].item.kind==ItemKind::Potion && bag.slots[i].item.ptype==PotionKind::Poison){
            consume_slot(bag,i,1); return true;
        }
    }
    return false;
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void player_throw_poison(AppState &app){
    if (app.turn!=CombatTurn::PlayerTurn) return;
    PlayerData &player=hero_by_index(app, app.combat_hero_index); DungeonCell &cell=locked_or_here(app, player);
    if (!cell.has_enemy||!cell.enemy.alive) return;
    if (!consume_first_poison_potion(app,player)){ push_log(app,"No Poison Potion!"); return; }
    if (roll_miss_10()) { push_log(app,"You hurl the vial... and miss!"); end_player_turn_to_enemy(app); return; }
    int dps = 2 + random_in_range(0,1);
    apply_poison(cell.enemy, 3, dps); push_log(app,"You hurl a Poison Potion! ("+to_str(dps)+"/turn)");
    end_player_turn_to_enemy(app);
}

// =====[ inventory actions ]===================================================
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static int gem_coin_value_option1(const AppState &app){ int depth=current_depth_for_player(app); return depth*2 + random_in_range(5,15); }
/**
 * Category: Inventory/Items
 * UI Scope: UI: Shop
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool sell_item_at(AppState &app, PlayerData &plr, int idx)
{
    Inventory &bag=plr.bag; if (idx<0||idx>=(int)bag.slots.size()) return false;
    ItemSlot &slot=bag.slots[idx]; if (slot.is_empty()) return false; const ItemData &it=slot.item;
    int addCoins=0; string action="Sold ";
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (it.kind==ItemKind::Treasure && it.name=="Gem"){ addCoins=gem_coin_value_option1(app); action="Converted Gem -> "; }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (it.kind==ItemKind::Potion){ addCoins= std::max(2, it.ptype==PotionKind::Heal ? it.heal/2 : 5); }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    else if (it.kind==ItemKind::Weapon){ addCoins= std::max(1, it.atk*5); }
    else addCoins=5;
    consume_slot(bag,idx,1);
    plr.coins += addCoins; plr.lifetime_earned += addCoins;
    push_log(app, action+to_str(addCoins)+" coins."); return true;
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool index_allowed_for_player(const AppState &, int){ return true; }
/**
 * Category: Inventory/Items
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool use_item_at(AppState &app, PlayerData &player, int idx)
{
    Inventory &bag=player.bag; if (idx<0||idx>=(int)bag.slots.size()) return false;
    if (!index_allowed_for_player(app, idx)){ push_log(app,"That slot belongs to your teammate."); return false; }
    ItemSlot &slot=bag.slots[idx]; if (slot.is_empty()) return false; ItemData &it=slot.item;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (it.kind==ItemKind::Treasure && it.name=="Gem"){
        int xp=5+random_in_range(0,5); player.exp += xp; push_log(app,"Used Gem: +"+to_str(xp)+" XP.");
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        while (player.exp>=player.exp_to_next){
            player.exp -= player.exp_to_next; player.level += 1; player.exp_to_next=int(player.exp_to_next*1.35f)+5; player.max_hp+=4; player.base_atk+=1; recompute_attack(player);
            player.hp = std::min(player.max_hp, player.hp+6); push_log(app,"Level up! Now "+to_str(player.level));
        }
        consume_slot(bag,idx,1); return true;
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (it.kind==ItemKind::Potion){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (it.ptype==PotionKind::Heal){
            int before=player.hp; player.hp=std::min(player.max_hp, player.hp+it.heal);
            push_log(app, (player.hp>before?"Drank ":"Tried ") + it.name+" ("+to_str(player.hp)+"/"+to_str(player.max_hp)+")");
            consume_slot(bag,idx,1); return true;
        } else {
            consume_slot(bag,idx,1);
            DungeonCell &cell=locked_or_here(app, player);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (app.mode==UIMode::Combat && cell.has_enemy && cell.enemy.alive){
                if (roll_miss_10()) { push_log(app,"You splash poison but miss!"); return true; }
                int dps=std::max(2,it.heal); apply_poison(cell.enemy,3,dps);
                push_log(app,"You throw a Poison Potion! ("+to_str(dps)+"/turn)");
            } else push_log(app,"You waste the poison on the floor...");
            return true;
        }
    }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (it.kind==ItemKind::Weapon){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (player.has_equipped_weapon){
            if (!add_item(bag, player.equipped_weapon, 1)){ push_log(app,"No space to swap!"); return false; }
        }
        player.equipped_weapon=it; player.has_equipped_weapon=true; recompute_attack(player);
        consume_slot(bag,idx,1); push_log(app,"Equipped "+it.name+" (+"+to_str(it.atk)+")");
        return true;
    }
    return false;
}

// =====[ enemy turn ]==========================================================
/**
 * Category: Enemy System
 * Purpose : Creates/updates enemy data and behaviors.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void enemy_take_turn(AppState &app)
{
    if (app.mode != UIMode::Combat) return;

    PlayerData  &fighter = hero_by_index(app, app.combat_hero_index);
    DungeonCell &cell    = app.combat_locked ? app.dungeon.cells[app.combat_cy][app.combat_cx]
                                             : app.dungeon.cells[fighter.pos.y][fighter.pos.x];
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (!cell.has_enemy || !cell.enemy.alive) { app.turn = CombatTurn::PlayerTurn; return; }

    // DoTs on enemy
    tick_enemy_status(app, cell.enemy);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (!cell.enemy.alive) {
        cell.has_enemy=false; cell.explored=true;
        int xp=(cell.type==RoomType::Boss)?25:(6+random_in_range(0,4));
        push_log(app,"Enemy defeated by damage over time!");
        grant_experience(app,xp);
        drop_coins_on_defeat(app);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (cell.type==RoomType::Boss){ app.mode=UIMode::Victory; switch_to_music(app, app.music_victory); }
        else exit_combat_to_explore(app);
        return;
    }

    // Stunned enemy skips
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (cell.enemy.status.stun_turns > 0) {
        cell.enemy.status.stun_turns--;
        push_log(app, "Enemy is stunned and skips the turn.");
        app.turn = CombatTurn::PlayerTurn;
        return;
    }

    // Specials
    if (cell.enemy.kind=="goblin" && chance_percent(25)) {
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (auto &s : fighter.bag.slots) { if (!s.is_empty()) { push_log(app,"Goblin steals your "+s.item.name+"!"); s=ItemSlot{}; break; } }
    }
    if (cell.enemy.kind=="slime" && chance_percent(20) && fighter.has_equipped_weapon) {
        push_log(app,"Slime dissolves your weapon!");
        fighter.has_equipped_weapon=false; fighter.equipped_weapon=ItemData{}; recompute_attack(fighter);
    }
    if (cell.enemy.kind=="troll" && chance_percent(25)) {
        push_log(app,"Troll prank! You are stunned for 1 turn.");
        app.turn = CombatTurn::PlayerTurn;
        return;
    }
    if (cell.enemy.kind=="skeleton" && chance_percent(30)) {
        apply_decay(fighter, 3, 2);
        push_log(app,"Skeleton inflicts Decay!");
    }

    // Damage
    int total = 0;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (cell.type==RoomType::Boss) {
        if (app.has_player_two && chance_percent(35)) {
            int aoe = std::max(2, cell.enemy.atk-1+random_in_range(0,2));
            app.playerOne.hp -= aoe;
            if (app.has_player_two) app.playerTwo.hp -= aoe;
            push_log(app, "Boss unleashes AOE roar! ("+to_str(aoe)+" to all)");

            // >>> instant defeat if ANY hero dies
            if (app.playerOne.hp <= 0 || (app.has_player_two && app.playerTwo.hp <= 0)) {
                app.mode = UIMode::Defeat;
                switch_to_music(app, app.music_defeat);
                return;
            }

            app.turn = CombatTurn::PlayerTurn;
            return;
        } else if (chance_percent(50)) {
            int d1=std::max(1, cell.enemy.atk-1+random_in_range(0,1));
            int d2=std::max(1, cell.enemy.atk-1+random_in_range(0,1));
            total = d1 + d2; push_log(app,"Boss double strike! ("+to_str(d1)+"+"+to_str(d2)+")");
        } else {
            int d=std::max(2, cell.enemy.atk+2+random_in_range(0,2));
            total = d; push_log(app,"Boss heavy smash! ("+to_str(d)+")");
        }
    } else {
        total = std::max(1, cell.enemy.atk);
        if (cell.enemy.kind=="orc" && chance_percent(30)) {
            total += 3+random_in_range(0,2);
            push_log(app,"Orc Brutal Swing!");
        }
    }

    if (roll_miss_10()) {
        push_log(app, "Enemy attack misses!");
        app.turn = CombatTurn::PlayerTurn;
        return;
    }

    fighter.hp -= total; push_log(app, "Enemy hits for " + to_str(total) + ".");

    // >>> instant defeat if the engaged hero dies
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (fighter.hp <= 0) {
        push_log(app, "You fall!");
        app.mode = UIMode::Defeat;
        switch_to_music(app, app.music_defeat);
        return;
    }

    app.turn = CombatTurn::PlayerTurn;
}

// Handle inventory input for a specific player --------------------------------
/**
 * Category: Inventory/Items
 * UI Scope: UI: Inventory Panel
 * Purpose : Manages items, stacking, equip/use, drawing.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void handle_inventory_input(AppState &app,
                                   PlayerData &who,
                                   float ox, float oy)
{
    Inventory &bag=who.bag;
    // number keys
    for (int i=0;i<std::min(8,(int)bag.slots.size());++i){
        if (key_typed((key_code)((int)NUM_1_KEY+i))){
            bool used=use_item_at(app, who, i);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (used && app.mode==UIMode::Combat){
                end_player_turn_to_enemy(app);
                enemy_take_turn(app);
            }
        }
    }

    // mouse
    const float cs=56; const int cols=4; float mx=mouse_x(), my=mouse_y();
    if (mouse_clicked(LEFT_BUTTON)){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (int idx=0; idx<bag.max_slots; ++idx){
            float x=ox+(idx%cols)*cs, y=oy+(idx/cols)*cs;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (mx>=x && mx<=x+cs && my>=y && my<=y+cs){
                bool used=use_item_at(app, who, idx);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
                if (used && app.mode==UIMode::Combat){
                    end_player_turn_to_enemy(app);
                    enemy_take_turn(app);
                }
                break;
            }
        }
    }
    if (mouse_clicked(RIGHT_BUTTON)){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (int idx=0; idx<bag.max_slots; ++idx){
            float x=ox+(idx%cols)*cs, y=oy+(idx/cols)*cs;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (mx>=x && mx<=x+cs && my>=y && my<=y+cs){
                bool sold=sell_item_at(app, who, idx);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
                if (sold && app.mode==UIMode::Combat){
                    end_player_turn_to_enemy(app);
                    enemy_take_turn(app);
                }
                break;
            }
        }
    }
}
// Put this in place of your current draw_inventories_exploring(AppState&).
/**
 * Category: Rendering/UI
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void draw_inventories_exploring(AppState &app)
{
    // P1 inventory: use dynamic Y that clears the player panel area
    const float p1y = inventory_y_for(app);
    draw_inventory_panel(app, app.playerOne, INV_X, p1y);
    handle_inventory_input(app, app.playerOne, INV_X, p1y);

    // P2 inventory: stack directly under P1 on the *left* (no right-side panel)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (app.has_player_two)
    {
        const float p2y = inventory2_y_for(app, app.playerTwo.bag.max_slots);
        draw_inventory_panel(app, app.playerTwo, INV_X, p2y);
        handle_inventory_input(app, app.playerTwo, INV_X, p2y);
    }
}

// =====[ save/load modal ]=====================================================
// Modal layout/preview helpers (wrapped labels, anti-flicker hover)
/**
 * Category: UI Controls
 * UI Scope: UI: Save/Load Modal
 * Purpose : Implements menus, modals, and interactive widgets.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void ensure_modal_layout(SaveLoadUI &ui){
    float cx=(1024-420)/2.0f, cy=(640-300)/2.0f;
    for (int i=0;i<3;++i) ui.slotBtn[i]=Button{cx+40, cy+60+i*70, 340,48, "Slot "+to_str(i+1)};
    ui.closeBtn=Button{cx+330, cy+250, 80,40, "Close"};
}
/**
 * Category: Formatting/Utility
 * UI Scope: UI: Save/Load Modal
 * Purpose : String/label helpers, wrappers, and layout.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static string slot_label_preview(const string &path)
{
    if (!file_exists(path)) return "Empty";
    std::ifstream in(path); if (!in) return "Empty";
    string tag; int ver=0; if (!(in>>tag) || tag!="VER" || !(in>>ver)) return "Unknown";
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    string name="", mode="SINGLE"; int coins=0;
    int lvl=0,hp=0,x=0,y=0,exp=0,expn=10,maxhp=0,batk=0,eb=0,slots=0; string line; bool gotP=false;
    while(std::getline(in,line)){
        if(line.rfind("PNAME",0)==0){ std::istringstream ss(line.substr(5)); string nm; ss>>nm; name=unescape_name(nm); }
        else if(line.rfind("MODE",0)==0){ if(line.find("MULTI")!=string::npos) mode="MULTI"; }
        else if(line.rfind("COINS",0)==0){ std::istringstream ss(line.substr(5)); ss>>coins; }
        else if(line.rfind("P ",0)==0){
            std::istringstream ss(line.substr(1));
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (ss>>x>>y>>lvl>>exp>>expn>>maxhp>>hp>>batk>>eb>>slots) { gotP = true; break; }
        }
    }
    if(!gotP) return (name.empty()?"Saved game":"Saved: "+name);

    string who = (mode=="MULTI" ? "Team " : "Player ") + (name.empty()?string("?"):name);
    return who + " Level " + to_str(lvl) + ", HP " + to_str(hp) +
           ", Coins " + to_str(coins) + ", Pos (" + to_str(x) + "," + to_str(y) + ")";
}

// Render the Save/Load modal and handle clicks --------------------------------
// ===== Forward declarations required for draw_save_load_modal =====
// Safer: validate slot, write to temp, then atomic rename on success.
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static bool save_to_slot(AppState &app, int slot)
{
    // Accept only Slot 1..3
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (slot < 1 || slot > 3)
    {
        app.saveui.info = cat("Save failed: bad slot ", slot);
        return false;
    }

    const string final_path = slot_path(slot);
    const string tmp_path   = final_path + ".tmp";

    // Try writing to a temp file first
    const bool ok_tmp = write_save(app, tmp_path);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (!ok_tmp)
    {
        app.saveui.info = cat("Save failed (slot ", slot, "): write error");
        // best effort cleanup
        std::remove(tmp_path.c_str());
        return false;
    }

    // Atomic-ish replace of the final file
    // (std::rename will overwrite on POSIX; on Windows it replaces if target absent.
    // If your toolchain requires, you can std::remove() the target first.)
    std::remove(final_path.c_str());         // ignore result
    if (std::rename(tmp_path.c_str(), final_path.c_str()) != 0)
    {
        app.saveui.info = cat("Save failed (slot ", slot, "): rename error");
        // leave tmp for debugging, or cleanup:
        std::remove(tmp_path.c_str());
        return false;
    }

    app.saveui.info = cat("Saved to Slot ", slot);
    return true;
}
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool load_from_slot(AppState &app, int slot)
{
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (slot < 1 || slot > 3)
    {
        app.saveui.info = cat("Load failed: bad slot ", slot);
        return false;
    }

    const string path = slot_path(slot);
    if (!file_exists(path))
    {
        app.saveui.info = cat("Slot ", slot, " is empty");
        return false;
    }

    // read_save already builds into a temp AppState and commits on full success
    const bool ok = read_save(app, path);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (ok)
    {
        app.saveui.info  = cat("Loaded Slot ", slot);
        app.saveui.modal = Modal::None;          // close the modal now that we’re in-game
        // read_save() already set mode, music, etc., but it’s safe to ensure:
        switch_to_music(app, app.music_explore);
        return true;
    }

    if (app.saveui.info.empty())
        app.saveui.info = cat("Load failed (slot ", slot, ")");

    return false;
}
/**
 * Category: Rendering/UI
 * UI Scope: UI: Save/Load Modal
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void draw_save_load_modal(AppState &app, bool isSave)
{
    auto &ui=app.saveui; ensure_modal_layout(ui);

    // background dim
    fill_rectangle(rgba_color(0,0,0,180),0,0,1024,640);

    const float bx=(1024-420)/2.0f, by=(640-300)/2.0f;
    fill_rectangle(COLOR_WHITE,bx,by,420,300);
    draw_rectangle(COLOR_BLACK,bx,by,420,300);
    draw_text(isSave?"Save Game":"Load Game", COLOR_BLACK, bx+16,by+14);

    // assign labels & hover smoothing
    float mx=mouse_x(), my=mouse_y();
    int hover_now=-1;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int i=0;i<3;++i){
        ui.slotBtn[i].label = "Slot " + to_str(i+1) + ": " + slot_label_preview(slot_path(i+1));
        if (point_in_button(mx,my,ui.slotBtn[i])) hover_now=i;
    }
    if (hover_now==ui.last_hover) ui.hover_frames++; else { ui.last_hover=hover_now; ui.hover_frames=0; }

    // draw buttons
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    for (int i=0;i<3;++i){
        bool hover = (i==ui.last_hover && ui.hover_frames>=2);
        draw_button(ui.slotBtn[i], hover);
    }
    draw_button(ui.closeBtn, point_in_button(mx,my,ui.closeBtn));

    if (!ui.info.empty()) draw_wrapped_text(ui.info, COLOR_DARK_GRAY, bx+16, by+260, 388.0f, 2, 14.0f);
    
    // quick-pick via keys 1..3
    if (key_typed(NUM_1_KEY)) { if (isSave) (void)save_to_slot(app,1); else if (load_from_slot(app,1)) { ui.modal=Modal::None; app.mode=UIMode::Exploring; switch_to_music(app,app.music_explore); } }
    if (key_typed(NUM_2_KEY)) { if (isSave) (void)save_to_slot(app,2); else if (load_from_slot(app,2)) { ui.modal=Modal::None; app.mode=UIMode::Exploring; switch_to_music(app,app.music_explore); } }
    if (key_typed(NUM_3_KEY)) { if (isSave) (void)save_to_slot(app,3); else if (load_from_slot(app,3)) { ui.modal=Modal::None; app.mode=UIMode::Exploring; switch_to_music(app,app.music_explore); } }
    
    // ESC closes the modal
    if (key_typed(ESCAPE_KEY)) { ui.modal = Modal::None; return; }

    // clicks
    if (mouse_clicked(LEFT_BUTTON)){
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (int i=0;i<3;++i){
            if (point_in_button(mx,my,ui.slotBtn[i])){
                if (isSave) (void)save_to_slot(app,i+1);
                else if (load_from_slot(app,i+1)) { ui.modal=Modal::None; app.mode=UIMode::Exploring; switch_to_music(app,app.music_explore); }
                return;
            }
        }
        if (point_in_button(mx,my,ui.closeBtn)) { ui.modal=Modal::None; return; }
    }
}

// =====[ pause overlay ]=======================================================
/**
 * Category: Rendering/UI
 * UI Scope: UI: Pause Menu
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void draw_pause_menu(const Button &resumeBtn,
                            const Button &saveBtn,
                            const Button &loadBtn,
                            const Button &quitBtn,
                            const Button &settingsBtn,
                            const Button &backMenuBtn)
{
    fill_rectangle(rgba_color(0,0,0,160), 0,0,1024,640);
    draw_text("PAUSED", COLOR_SILVER, 470, 180);

    const float mx = mouse_x(), my = mouse_y();
    draw_button(resumeBtn,    point_in_button(mx,my,resumeBtn));
    draw_button(saveBtn,      point_in_button(mx,my,saveBtn));
    draw_button(loadBtn,      point_in_button(mx,my,loadBtn));
    draw_button(settingsBtn,  point_in_button(mx,my,settingsBtn));
    draw_button(backMenuBtn,  point_in_button(mx,my,backMenuBtn));
    draw_button(quitBtn,      point_in_button(mx,my,quitBtn));
}


// =====[ combat modal (overlay + controls) ]===================================
// Why: central place for combat UI text + keybind hints (works with mouse/keys).
// -----------------------------------------------------------------------------
// Combat Modal 
// - Two columns: Player (left) vs Enemy (right)
// - HP bars under each
// - Big inline log box inside the modal (last ~6 lines)
// - Footer actions: A/F/Q/E (no R/poison hint)
// -----------------------------------------------------------------------------
/**
 * Category: Rendering/UI
 * UI Scope: UI: Save/Load Modal
 * Purpose : Draws a specific UI element or screen component.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void draw_combat_modal(AppState &app, DungeonCell &cell, int W, int H)
{
    const float rw = 620, rh = 320;
    const float rx = (W - rw) * 0.5f;
    const float ry = (H - rh) * 0.5f;

    fill_rectangle(rgba_color(0,0,0,180), 0, 0, W, H);
    fill_rectangle(COLOR_WHITE, rx, ry, rw, rh);
    draw_rectangle(COLOR_BLACK, rx, ry, rw, rh);

    // Title
    draw_text("Combat!", COLOR_RED, rx + 16, ry + 12);

    // Column layout
    const float col_w = (rw - 32) * 0.5f;      // inside padding
    const float left_x  = rx + 16;
    const float right_x = rx + 16 + col_w;
    float ly = ry + 42;
    float ry2 = ry + 42;

    const PlayerData &player= hero_by_index(app, app.combat_hero_index);
    draw_text(cat("Player (Lvl ", player.level, ")"), COLOR_BLACK, left_x, ly);    ly += 20;
    draw_text(cat("ATK ", player.atk), COLOR_BLACK, left_x, ly);                   ly += 18;
    // player HP bar
    draw_text("HP", COLOR_BLACK, left_x, ly);
    draw_bar_with_border(left_x + 28, ly, col_w - 56, 12,
                         player.max_hp ? (float)player.hp / (float)player.max_hp : 0.0f);
    ly += 22;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (cell.has_enemy)
    {
        EnemyData &e = cell.enemy;
        draw_text(enemy_display_name(e.kind), COLOR_BLACK, right_x, ry2);     ry2 += 20;
        draw_text(cat("ATK ", e.atk), COLOR_BLACK, right_x, ry2);             ry2 += 18;
        // enemy HP bar
        draw_text("HP", COLOR_BLACK, right_x, ry2);
        draw_bar_with_border(right_x + 28, ry2, col_w - 56, 12,
                             e.max_hp ? (float)e.hp / (float)e.max_hp : 0.0f);
        ry2 += 22;
    }

    // Inline log box (center area)
    const float log_x = rx + 16;
    const float log_y = ry + 120;
    const float log_w = rw - 32;
    const float log_h = 140;
    draw_rectangle(COLOR_SILVER, log_x, log_y, log_w, log_h);

    // Render last ~6 entries, wrapped
    int n = (int)app.log.size();
    int max_lines = 6;
    int start = std::max(0, n - max_lines);
    float t_y = log_y + 10;
    const float line_h = 18.0f;
    const float max_px = log_w - 20.0f;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    for (int i = start; i < n; ++i)
    {
        // wrap each log line to at most 2 visual lines to keep box tidy
        auto lines = wrap_lines(app.log[i], max_px, 2);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        for (const auto &one : lines)
        {
            draw_text(one, COLOR_BLACK, log_x + 10, t_y);
            t_y += line_h;
            if (t_y > log_y + log_h - line_h) break;
        }
        if (t_y > log_y + log_h - line_h) break;
    }

    // Footer actions (use actual skill names)
    const float foot_y = ry + rh - 36;

    string qName = primary_skill_name(hero_by_index(app, app.combat_hero_index));
    string eName = secondary_skill_name(hero_by_index(app, app.combat_hero_index));

    // Line 1
    draw_text("[A] Attack      [F] Flee", COLOR_DARK_GRAY, rx + 16, foot_y);

    // Line 2 (real skill names)
    draw_text(cat("[Q] ", qName, "      [E] ", eName, "      [R] Poison"),
            COLOR_DARK_GRAY, rx + 16, foot_y + 16);

    // Input hint for consumables
    draw_text("[Use 1..8 to drink/equip]", COLOR_SILVER, rx + 16, foot_y + 32);

        // Current turn indicator (optional)
    // draw_text(app.turn == CombatTurn::PlayerTurn ? "Turn: Player" : "Turn: Enemy", COLOR_BLACK, rx + rw - 130, foot_y);
}

// ============================ resources & setup ==============================
// Implementations for functions you prototyped earlier.
/**
 * Category: Persistence (Save/Load)
 * UI Scope: UI: Save/Load Modal
 * Purpose : Serializes or deserializes game state safely.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void load_all_music(AppState &app)
{
    // If your filenames differ, change them here.
    app.music_explore = load_music("explore", "01 - Prologue _ Forbidden Arts _ Law _ Black Blood _ Resurrection.mp3");
    app.music_battle  = load_music("battle",  "06. Hornet.mp3");
    app.music_boss    = load_music("boss",    "ruler_of_death.flac");
    app.music_victory = load_music("victory", "the-bards-tale-63078.mp3");
    app.music_defeat  = load_music("defeat",  "dark-souls-you-died-sound-effect_hm5sYFG.mp3");
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void init_default_players(AppState &app)
{
    // Start both players with empty 8-slot bags and base stats.
    init_player_inventory(app.playerOne, 8);
    init_player_inventory(app.playerTwo, 8);

    app.playerOne.base_atk = 4;
    app.playerTwo.base_atk = 4;

    // Default classes/elements (same as earlier build).
    app.playerOne.cls = CharacterClass::Wizard;
    app.playerOne.element = Element::Ice;

    app.playerTwo.cls = CharacterClass::Barbarian;
    app.playerTwo.element = Element::Neutral;

    recompute_attack(app.playerOne);
    recompute_attack(app.playerTwo);

    // Starter daggers.
    add_item(app.playerOne.bag, weapon_named(app.next_item_id++,"Steel Dagger",1), 1);
    add_item(app.playerTwo.bag, weapon_named(app.next_item_id++,"Steel Dagger",1), 1);
}

// Optional: if you didn't already define this earlier, include it here.
// Resets the world and jumps to Exploring with explore music.
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static void reset_to_exploring(AppState &app)
{
    stop_music();
    app.log.clear();
    generate_dungeon(app);
    app.mode = UIMode::Exploring;
    switch_to_music(app, app.music_explore);
}

// Cycle classes: Wizard → Barbarian → Cleric → Wizard
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
static CharacterClass next_class(CharacterClass c)
{
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    switch (c)
    {
        case CharacterClass::Wizard:    return CharacterClass::Barbarian;
        case CharacterClass::Barbarian: return CharacterClass::Cleric;
        case CharacterClass::Cleric:    return CharacterClass::Wizard;
    }
    return CharacterClass::Wizard;
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void start_singleplayer(AppState &app, const std::string &name)
{
    app.co_op_enabled   = false;
    app.has_player_two  = false;

    app.party_name              = name;
    app.playerOne.display_name  = name;
        
    app.shop_buyer = 1;   // default buyer is Player 1

    reset_to_exploring(app);
    push_log(app, "Singleplayer started.");
}
/**
 * Category: Player System
 * Purpose : Creates/updates player data, stats, or effects.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static void start_multiplayer(AppState &app, const std::string &team_name)
{
    app.co_op_enabled   = true;
    app.has_player_two  = true;

    app.party_name             = team_name;
    app.playerOne.display_name = team_name;
        
    app.shop_buyer = 1;   // default buyer is Player 1

    // Ensure P2 inventory/stats are initialized.
    init_player_inventory(app.playerTwo, 8);
    app.playerTwo.base_atk = 4;
    recompute_attack(app.playerTwo);
    app.playerTwo.pos = {0,0};

    reset_to_exploring(app);
    recompute_attack(app.playerOne);
    recompute_attack(app.playerTwo);

    push_log(app, "Local co-op started.");
}

/** help back to menu reset once go back */
static void reset_to_menu(AppState &app) {
    stop_music();
    app = AppState{};
    init_default_players(app);
    load_all_music(app);
    load_highscore(app.highscore);
    generate_dungeon(app);
    switch_to_music(app, app.music_explore);
    app.mode = UIMode::Menu;
}

/**
 * Category: UI Controls
 * UI Scope: UI: Pause Menu
 * Purpose : Implements menus, modals, and interactive widgets.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool show_pause_modal(AppState &app,
                             const Button &resumeBtn,
                             const Button &saveBtn,
                             const Button &loadBtn,
                             const Button &quitBtn,
                             const Button &settingsBtn,
                             const Button &backMenuBtn)
{
    // Dim + panel
    fill_rectangle(rgba_color(0,0,0,160), 0,0,1024,640);
    draw_text("PAUSED", COLOR_SILVER, 470, 180);

    float mx = mouse_x(), my = mouse_y();
    draw_button(resumeBtn, point_in_button(mx,my,resumeBtn));
    draw_button(saveBtn,   point_in_button(mx,my,saveBtn));
    draw_button(loadBtn,   point_in_button(mx,my,loadBtn));
    draw_button(settingsBtn, point_in_button(mx,my,settingsBtn));
    draw_button(backMenuBtn, point_in_button(mx,my,backMenuBtn));
    draw_button(quitBtn,   point_in_button(mx,my,quitBtn));

    // Debounce first few frames so the opening P press isn't seen again
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
    if (app.pause_debounce > 0) {
        app.pause_debounce--;
    } else {
        // Close with ESC only (avoid P so it can't immediately re-toggle)
        if (key_typed(ESCAPE_KEY)) {
            app.mode = UIMode::Exploring;
            return true;
        }
        // Optional: if you really want P to also close, keep it but after debounce
        // if (key_typed(P_KEY)) { app.mode = UIMode::Exploring; return true; }
    }

    // Mouse clicks
    if (mouse_clicked(LEFT_BUTTON)) {
        if      (point_in_button(mx,my,resumeBtn))   { app.mode = UIMode::Exploring; }
        else if (point_in_button(mx,my,saveBtn))     { app.saveui.modal=Modal::SaveSelect; app.saveui.info.clear(); }
        else if (point_in_button(mx,my,loadBtn))     { app.saveui.modal=Modal::LoadSelect; app.saveui.info.clear(); }
        else if (point_in_button(mx,my,settingsBtn)) { app.settings_return_to = UIMode::Pause; app.mode = UIMode::Settings; }
        else if (point_in_button(mx,my,backMenuBtn)) { stop_music(); reset_to_menu(app); }
        else if (point_in_button(mx,my,quitBtn))     { close_window("Dungeon Explorer (C++)"); }
        return true;
    }

    return true; // block the frame
}
/**
 * Category: UI Controls
 * UI Scope: UI: Save/Load Modal
 * Purpose : Implements menus, modals, and interactive widgets.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool show_shop_modal(AppState &app)
{
    // Draw and handle purchases
    draw_shop_modal(app);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (app.shop_debounce > 0) {
        app.shop_debounce--;
    } else {
        if (key_typed(ESCAPE_KEY) || key_typed(J_KEY)) {
            app.mode = UIMode::Exploring;
            return true;
        }
    }
    return true; // modal blocks rest of UI this frame
}
/**
 * Category: UI Controls
 * UI Scope: UI: Save/Load Modal
 * Purpose : Implements menus, modals, and interactive widgets.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

static bool show_log_modal(AppState &app)
{
    draw_log_viewer(app, 1024, 640);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (app.log_debounce > 0) {
        app.log_debounce--;
    } else {
        if (key_typed(ESCAPE_KEY) || key_typed(H_KEY)) {
            app.logview.open = false;
            return true;
        }
    }
    return true;
}

// =====[ main ]===============================================================
/**
 * Category: Program Entry
 * Purpose : Initializes resources and runs main loop.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
int main()
{
    srand((unsigned)time(nullptr));
    open_window("Dungeon Explorer (C++)", 1024, 640);

    AppState app{};
    init_default_players(app);
    load_all_music(app);
    load_highscore(app.highscore);
    generate_dungeon(app);
    switch_to_music(app, app.music_explore);

    // Main menu buttons (evenly spaced; no collisions)
    const float menuX    = (1024 - 320) / 2.0f;  // center horizontally
    const float menuY    = 120.0f;               // top of first button
    const float menuStep = 76.0f;                // vertical spacing between buttons

    Button singleButton    { menuX, menuY + 0*menuStep, 320,56, "Singleplayer" };
    Button multiButton     { menuX, menuY + 1*menuStep, 320,56, "Multiplayer (local co-op)" };
    Button loadButton      { menuX, menuY + 2*menuStep, 320,56, "Load Game" };
    Button settingsButton  { menuX, menuY + 3*menuStep, 320,56, "Settings" };
    Button quitButtonMain  { menuX, menuY + 4*menuStep, 320,56, "Quit to Desktop" };


    // Pause UI buttons (declare once)
    Button resumeBtn       { (1024-260)/2.0f, 220, 260,52, "Resume" };
    Button pauseSaveBtn    { (1024-260)/2.0f, 280, 260,52, "Save..." };
    Button pauseLoadBtn    { (1024-260)/2.0f, 340, 260,52, "Load..." };
    Button pauseSettingsBtn{ (1024-260)/2.0f, 400, 260,52, "Settings" };
    Button backMenuBtn     { (1024-260)/2.0f, 460, 260,52, "Back to Menu" };
    Button quitBtn         { (1024-260)/2.0f, 520, 260,52, "Quit to Desktop" };


    // Simple text prompt for names (letters/numbers/space)
    auto ask_text = [&](const string &prompt)->string{
        string acc="";
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        while (true){
            process_events();
            clear_screen(COLOR_BLACK);
            draw_text(prompt, COLOR_YELLOW, 140, 240);
            draw_text(acc+"_", COLOR_WHITE, 140, 280);
            draw_text("(Enter=OK, Backspace=del, ESC=cancel)", COLOR_SILVER, 140, 320);
            refresh_screen(60);
            if (key_typed(RETURN_KEY)) break;
            if (key_typed(ESCAPE_KEY)) { acc=""; break; }
            for (int k=A_KEY; k<=Z_KEY; ++k) if (key_typed((key_code)k)) acc.push_back('a'+(k-A_KEY));
            for (int k=NUM_0_KEY; k<=NUM_9_KEY; ++k) if (key_typed((key_code)k)) acc.push_back('0'+(k-NUM_0_KEY));
            if (key_typed(SPACE_KEY)) acc.push_back(' ');
            if (key_typed(BACKSPACE_KEY) && !acc.empty()) acc.pop_back();
        }
        if (acc.empty()) acc="Player";
        return acc;
    };

    while (!window_close_requested("Dungeon Explorer (C++)"))
    {
        process_events();
        clear_screen(COLOR_BLACK);

        const float mx = mouse_x(), my = mouse_y();
        const bool  clickL = mouse_clicked(LEFT_BUTTON);

    // ---------------- Global hotkeys (only when not blocked by a modal/log) -------------
    const bool ui_blocked =
        (app.saveui.modal!=Modal::None) || app.logview.open || (app.mode==UIMode::Pause) || (app.mode==UIMode::Shop);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

    if (!ui_blocked && app.mode!=UIMode::Menu) {
        if (key_typed(P_KEY)) {           // open Pause
            app.mode = UIMode::Pause;
            app.pause_debounce = 6;       // ~100ms at 60fps
            refresh_screen(60);
            continue;                     // <-- CRITICAL: don't run the modal this frame
        }
        if (key_typed(J_KEY)) {           // open Shop
            app.mode = UIMode::Shop;
            app.shop_debounce = 6;
            refresh_screen(60);
            continue;
        }
        if (key_typed(H_KEY)) {           // open log viewer
            app.logview.open = true;
            app.log_debounce = 6;
            refresh_screen(60);
            continue;
        }
        if (key_typed(F5_KEY))                    { app.saveui.modal=Modal::SaveSelect; app.saveui.info.clear(); refresh_screen(60); continue; }
        if (key_typed(F9_KEY) || key_typed(L_KEY)){ app.saveui.modal=Modal::LoadSelect; app.saveui.info.clear(); refresh_screen(60); continue; }
    }

        // ---------------- Save/Load modal (blocks everything) -------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.saveui.modal!=Modal::None){
            draw_save_load_modal(app, app.saveui.modal==Modal::SaveSelect);
            refresh_screen(60);
            continue;
        }

        // Full-screen Log (H toggles inside)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.logview.open) {
            (void)show_log_modal(app);
            refresh_screen(60);
            continue;
        }

        // Shop (J toggles inside)
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.mode==UIMode::Shop) {
            (void)draw_shop_modal(app);
            refresh_screen(60);
            continue;
        }

        // ---------------- Main menu --------------------------------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.mode==UIMode::Menu)
        {
            play_music_if_needed(app, app.music_explore);

            fill_rectangle(COLOR_LIGHT_GRAY, 0, 40, 1024, 60);
            draw_text("Dungeon Explorer (C++)", COLOR_YELLOW, 430, 55);
        
            draw_button(singleButton,   point_in_button(mx,my,singleButton));
            draw_button(multiButton,    point_in_button(mx,my,multiButton));
            draw_button(loadButton,     point_in_button(mx,my,loadButton));
            draw_button(settingsButton, point_in_button(mx,my,settingsButton));
            draw_button(quitButtonMain, point_in_button(mx,my,quitButtonMain));

            // Footer text region—always below the last button
            float footerY = quitButtonMain.y + quitButtonMain.h + 26;

            draw_text("1=Single  |  2=Multi  |  L/F9=Load  |  S=Settings  |  Q=Quit",
                    COLOR_SILVER, 300, footerY);
            draw_text("P1 Class: " + class_label(app.playerOne.cls) + 
                    (app.playerOne.cls==CharacterClass::Wizard?(" ["+element_label(app.playerOne.element)+"]"):""),
                    COLOR_SILVER, 360, footerY + 28);
            draw_text("P2 Class: " + class_label(app.playerTwo.cls) + 
                    (app.playerTwo.cls==CharacterClass::Wizard?(" ["+element_label(app.playerTwo.element)+"]"):""),
                    COLOR_SILVER, 360, footerY + 48);
            draw_text("Customize: C (P1), M (P2), TAB toggles Wizard element",
                    COLOR_SILVER, 300, footerY + 70);

            // Clicks
            if (clickL && point_in_button(mx,my,singleButton))   start_singleplayer(app, ask_text("Enter your PLAYER name:"));
            if (clickL && point_in_button(mx,my,multiButton))    start_multiplayer(app, ask_text("Enter your TEAM name:"));
            if (clickL && point_in_button(mx,my,loadButton))     { app.saveui.modal=Modal::LoadSelect; app.saveui.info.clear(); }
            if (clickL && point_in_button(mx,my,settingsButton)) { app.settings_return_to = UIMode::Menu; app.mode = UIMode::Settings; }
            if (clickL && point_in_button(mx,my,quitButtonMain)) { close_window("Dungeon Explorer (C++)"); break; }  // exit loop
            
            // Keys
            if (key_typed(NUM_1_KEY)) start_singleplayer(app, ask_text("Enter your PLAYER name:"));
            if (key_typed(NUM_2_KEY)) start_multiplayer(app, ask_text("Enter your TEAM name:"));
            if (key_typed(F9_KEY) || key_typed(L_KEY)) { app.saveui.modal=Modal::LoadSelect; app.saveui.info.clear(); }
            if (key_typed(S_KEY)) { app.settings_return_to = UIMode::Menu; app.mode = UIMode::Settings; }
            if (key_typed(Q_KEY)) { close_window("Dungeon Explorer (C++)"); break; }
            if (key_typed(ESCAPE_KEY)) break;

            if (key_typed(C_KEY)){
                app.playerOne.cls=next_class(app.playerOne.cls);
                if (app.playerOne.cls!=CharacterClass::Wizard) app.playerOne.element=Element::Neutral;
                else if (app.playerOne.element==Element::Neutral) app.playerOne.element=Element::Ice;
            }
            if (key_typed(M_KEY)){
                app.playerTwo.cls=next_class(app.playerTwo.cls);
                if (app.playerTwo.cls!=CharacterClass::Wizard) app.playerTwo.element=Element::Neutral;
                else if (app.playerTwo.element==Element::Neutral) app.playerTwo.element=Element::Ice;
            }
            if (key_typed(TAB_KEY)){
                if (app.playerOne.cls==CharacterClass::Wizard)
                    app.playerOne.element=(app.playerOne.element==Element::Ice?Element::Fire:Element::Ice);
                if (app.playerTwo.cls==CharacterClass::Wizard)
                    app.playerTwo.element=(app.playerTwo.element==Element::Ice?Element::Fire:Element::Ice);
            }

            refresh_screen(60);
            continue;
        }

        // ---------------- Exploring -------------------------------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.mode==UIMode::Exploring)
        {
            play_music_if_needed(app, app.music_explore);
            enforce_boss_presence(app);

            if (key_typed(W_KEY)) try_move_player(app, app.playerOne, 0,-1);
            if (key_typed(S_KEY)) try_move_player(app, app.playerOne, 0, 1);
            if (key_typed(A_KEY)) try_move_player(app, app.playerOne,-1, 0);
            if (key_typed(D_KEY)) try_move_player(app, app.playerOne, 1, 0);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

            if (app.has_player_two){
                if (key_typed(UP_KEY))    try_move_player(app, app.playerTwo, 0,-1);
                if (key_typed(DOWN_KEY))  try_move_player(app, app.playerTwo, 0, 1);
                if (key_typed(LEFT_KEY))  try_move_player(app, app.playerTwo,-1, 0);
                if (key_typed(RIGHT_KEY)) try_move_player(app, app.playerTwo, 1, 0);
            }

            const float invY = inventory_y_for(app);
            draw_player_panel(app, 24,24,360);
            draw_inventories_exploring(app);        // <— P2 stacked below P1
            draw_grid(app, 420,24);
            draw_log_box(app, 420, 480, 560, 120);

            refresh_screen(60);
            continue;
        }

        // ---------------- Combat ----------------------------------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.mode==UIMode::Combat)
        {
            play_music_if_needed(app, app.in_boss_fight ? app.music_boss : app.music_battle);

            PlayerData &player = hero_by_index(app, app.combat_hero_index);
            DungeonCell &cell = locked_or_here(app, player);  // your combat cell-lock helper
            
            const float invYc = inventory_y_for(app);
            draw_player_panel(app, 24, 24, 360);
            draw_inventory_panel(app, player, INV_X, invYc);
            handle_inventory_input(app, const_cast<PlayerData&>(player), INV_X, invYc);
            draw_grid(app, 420,24);
            draw_log_box(app, 420, 480, 560, 120);
            draw_combat_modal(app, cell, 1024, 640);
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */

            if (app.turn==CombatTurn::PlayerTurn){
                if (key_typed(A_KEY)) player_attack(app);
                if (key_typed(R_KEY)) player_throw_poison(app);
                if (key_typed(F_KEY)) player_try_flee(app);
                if (key_typed(Q_KEY)) player_primary_skill(app);
                if (key_typed(E_KEY)) player_secondary_skill(app);

                // Inventory actions may end the turn immediately
                handle_inventory_input(app, hero_by_index(app, app.combat_hero_index), INV_X, invYc);
                if (app.turn==CombatTurn::EnemyTurn) enemy_take_turn(app);
            }

            refresh_screen(60);
            continue;
        }

        // ---------------- Pause (single-press close inside) --------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.mode==UIMode::Pause) {
            (void)show_pause_modal(app, resumeBtn, pauseSaveBtn, pauseLoadBtn, quitBtn, pauseSettingsBtn, backMenuBtn);
            refresh_screen(60);
            continue;
        }
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        
        if (app.mode == UIMode::Settings) {
            (void)show_settings_modal(app);
            refresh_screen(60);
            continue;
        }

        // ---------------- Victory (R returns to Menu) --------------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
       if (app.mode==UIMode::Victory)
        {
            play_music_if_needed(app, app.music_victory);
            save_highscore_if_best(app);

            fill_rectangle(rgba_color(0,0,0,180),0,0,1024,640);

            draw_text("YOU WIN!", COLOR_YELLOW, 460, 200);

            float x = 320.0f;
            float y = 240.0f;

            const string teamName = app.has_player_two ? app.party_name : app.playerOne.display_name;
            draw_text(cat("Team: ", teamName), COLOR_WHITE, x, y); y += 28;

            // Coins
            draw_text(cat("P1 (", app.playerOne.display_name, "): ", app.playerOne.coins, " coins"),
                    COLOR_WHITE, x, y); y += 22;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (app.has_player_two) {
                draw_text(cat("P2 (", app.playerTwo.display_name, "): ", app.playerTwo.coins, " coins"),
                        COLOR_WHITE, x, y); y += 22;
                draw_text(cat("Team Total: ", app.playerOne.coins + app.playerTwo.coins, " coins"),
                        COLOR_YELLOW, x, y); y += 26;
            }

            // Best line
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (!app.has_player_two) {
                draw_text(cat("Best (Single): ", app.highscore.best_single_name, " - ",
                            app.highscore.best_single),
                        COLOR_SILVER, x, y); y += 26;
            } else {
                draw_text(cat("Best Team: ", app.highscore.best_team_name, " - ",
                            app.highscore.best_team),
                        COLOR_SILVER, x, y); y += 26;
            }

            // Prompt placed AFTER all the above lines
            draw_text("Press R to restart.", COLOR_SILVER, x, y + 10);

            if (key_typed(R_KEY)){
                stop_music();
                app = AppState{};
                init_default_players(app);
                load_all_music(app);
                load_highscore(app.highscore);
                generate_dungeon(app);
                switch_to_music(app, app.music_explore);
                app.mode = UIMode::Menu;
            }

            refresh_screen(60);
            continue;
        }

        // ---------------- Defeat (R returns to Menu) ---------------------------
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
        if (app.mode==UIMode::Defeat)
        {
            save_highscore_if_best(app);

            fill_rectangle(rgba_color(0,0,0,180),0,0,1024,640);
            draw_text("GAME OVER", COLOR_RED, 460, 200);

            float x = 320.0f;
            float y = 240.0f;

            const string teamName = app.has_player_two ? app.party_name : app.playerOne.display_name;
            draw_text(cat("Team: ", teamName), COLOR_WHITE, x, y); y += 28;

            // Coins
            draw_text(cat("P1 (", app.playerOne.display_name, "): ", app.playerOne.coins, " coins"),
                    COLOR_WHITE, x, y); y += 22;
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (app.has_player_two) {
                draw_text(cat("P2 (", app.playerTwo.display_name, "): ", app.playerTwo.coins, " coins"),
                        COLOR_WHITE, x, y); y += 22;
                draw_text(cat("Team Total: ", app.playerOne.coins + app.playerTwo.coins, " coins"),
                        COLOR_YELLOW, x, y); y += 26;
            }

            // Best line
/**
 * Category: General Helper
 * Purpose : Utility used by a higher-level system.
 * Inputs  : See parameter names and types.
 * Output  : Returns per signature (void if none).
 */
            if (!app.has_player_two) {
                draw_text(cat("Best (Single): ", app.highscore.best_single_name, " - ",
                            app.highscore.best_single),
                        COLOR_SILVER, x, y); y += 26;
            } else {
                draw_text(cat("Best Team: ", app.highscore.best_team_name, " - ",
                            app.highscore.best_team),
                        COLOR_SILVER, x, y); y += 26;
            }

            // Prompt after all stats so it never overlaps
            draw_text("Press R to retry.", COLOR_SILVER, x, y + 10);

            if (key_typed(R_KEY)){
                stop_music();
                app = AppState{};
                init_default_players(app);
                load_all_music(app);
                load_highscore(app.highscore);
                generate_dungeon(app);
                switch_to_music(app, app.music_explore);
                app.mode = UIMode::Menu;
            }

            refresh_screen(60);
            continue;
        }

        refresh_screen(60);
    }

    return 0;
}
