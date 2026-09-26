#include "game/GameApplication.hpp"

#include <algorithm>
#include <array>

#include "game/SaveGame.hpp"
#include "game/AppFlow.hpp"
#include "game/CombatFeedback.hpp"
#include "game/CombatSystem.hpp"
#include "game/EntityPicker.hpp"
#include "game/GameDebug.hpp"
#include "game/GameSettings.hpp"
#include "systems/CharacterCombat.hpp"
#include "gameplay/GameStateManager.hpp"
#include "gameplay/IsometricCamera.hpp"
#include "gameplay/LaneBattle.hpp"
#include "gameplay/ZoneManager.hpp"
#include "render/Mesh.hpp"
#include "render/AnimationStateMachine.hpp"
#include "render/GeneratedUiAtlas.hpp"
#include "render/MobAssets.hpp"
#include "render/ParticleRenderer.hpp"
#include "render/ParticleSystem.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/SpriteSheet.hpp"
#include "render/TextRenderer.hpp"
#include "render/TownBackdrop.hpp"
#include "render/UiAssets.hpp"
#include "render/UiRenderer.hpp"
#include "render/WorldPropAssets.hpp"
#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/InventoryDrag.hpp"
#include "systems/ItemStats.hpp"
#include "systems/RunProgression.hpp"
#include "systems/SkillBar.hpp"
#include "systems/SoulProgression.hpp"
#include "systems/WeaponMastery.hpp"
#include "systems/LootEngine.hpp"
#include "systems/SlotMachineLoot.hpp"
#include "systems/Blacksmith.hpp"
#include "systems/TownHub.hpp"
#include "systems/TradeSystem.hpp"
#include "ui/LootPresentation.hpp"
#include "ui/MinimapSystem.hpp"
#include "ui/OverlayState.hpp"
#include "ui/HudConsoleLayout.hpp"
#include "ui/UiHitTest.hpp"
#include "ui/UiInteraction.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiScale.hpp"
#include "ui/UiTextLayout.hpp"
#include "Shader.hpp"

#include "engine/FrameProbe.hpp"
#include "engine/GlBindings.hpp"
#include <GLFW/glfw3.h>

#include "stb_image.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

namespace {

std::string joinPath(const std::string& root, const char* relative) {
    return root + "/" + relative;
}

glm::vec3 toGlm(const gameplay::Vec3& value) {
    return glm::vec3(value.x, value.y, value.z);
}

gameplay::Vec3 toVec3(const glm::vec3& value) {
    return gameplay::Vec3{value.x, value.y, value.z};
}

struct EntityVisual {
    glm::vec3 color;
    glm::vec3 scale;
};

EntityVisual visualFor(gameplay::EntityKind kind) {
    switch (kind) {
    case gameplay::EntityKind::PLAYER:
        return {{0.25F, 0.55F, 0.95F}, {1.0F, 1.8F, 1.0F}};
    case gameplay::EntityKind::ENEMY_MOB:
        return {{0.85F, 0.2F, 0.2F}, {1.0F, 1.4F, 1.0F}};
    case gameplay::EntityKind::ENEMY_BOSS:
        return {{0.6F, 0.1F, 0.65F}, {2.0F, 3.0F, 2.0F}};
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return {{0.95F, 0.6F, 0.15F}, {1.2F, 2.0F, 1.2F}};
    case gameplay::EntityKind::ENV_TREE:
        return {{0.2F, 0.65F, 0.25F}, {1.2F, 2.8F, 1.2F}};
    case gameplay::EntityKind::ENV_BUSH:
        return {{0.15F, 0.55F, 0.2F}, {1.0F, 1.0F, 1.0F}};
    case gameplay::EntityKind::ENV_CHEST:
        return {{0.75F, 0.55F, 0.1F}, {1.0F, 0.8F, 0.8F}};
    case gameplay::EntityKind::ENV_ROCK:
        return {{0.45F, 0.48F, 0.52F}, {1.4F, 1.0F, 1.4F}};
    case gameplay::EntityKind::ENV_HOUSE:
        return {{0.72F, 0.62F, 0.48F}, {3.0F, 4.0F, 3.0F}};
    case gameplay::EntityKind::ENV_MUSHROOM:
        return {{0.85F, 0.35F, 0.35F}, {0.6F, 0.5F, 0.6F}};
    }
    return {{0.7F, 0.7F, 0.7F}, {1.0F, 1.0F, 1.0F}};
}

const char* zoneLabel(gameplay::WorldZone zone) {
    return zone == gameplay::WorldZone::TOWN ? "Town" : "Plains";
}

glm::vec3 clampToZone(const glm::vec3& point, const gameplay::AxisAlignedBounds& bounds) {
    return glm::vec3{
        std::clamp(point.x, bounds.minX, bounds.maxX),
        point.y,
        std::clamp(point.z, bounds.minZ, bounds.maxZ)};
}

struct MenuButton {
    ui::Rect bounds{};
    const char* label{nullptr};
    int id{0};
};

enum class SettingsControlKind : std::uint8_t { Slider, Cycle };

struct SettingsControl {
    ui::Rect bounds{};
    SettingsControlKind kind{SettingsControlKind::Cycle};
    int id{0};
    float* sliderValue{nullptr};
    float sliderMin{0.0F};
    float sliderMax{1.0F};
};

constexpr int kSettingsResolution = 30;
constexpr int kSettingsVolume = 31;
constexpr int kSettingsMinimapSize = 32;
constexpr int kSettingsMinimapAnchor = 33;
constexpr int kSettingsMouseSensitivity = 34;
constexpr int kSettingsGraphicsQuality = 35;
constexpr int kSettingsDifficulty = 36;

/// Row order of the Settings panel; shared by control construction and rendering.
constexpr float kMinimapWorldRadius = 46.0F;
constexpr float kVisibleGroundMargin = 8.0F;

constexpr int kSettingsRowIds[ui::SettingsPanelLayout::kRowCount] = {
    kSettingsResolution,
    kSettingsVolume,
    kSettingsMinimapSize,
    kSettingsMinimapAnchor,
    kSettingsMouseSensitivity,
    kSettingsGraphicsQuality,
    kSettingsDifficulty,
};


struct FloatingCombatText {
    std::string text;
    glm::vec3 worldPosition{};
    float ageSeconds{0.0F};
    float lifetimeSeconds{1.6F};
    float colorR{1.0F};
    float colorG{0.4F};
    float colorB{0.32F};
    bool critical{false};
};

constexpr float kMeleeAttackRange = 2.8F;
constexpr float kMobMeleeRange = 2.35F;
constexpr float kMobAggroRadius = 16.0F;
constexpr float kMobAttackCooldownMob = 1.35F;
constexpr float kMobAttackCooldownBoss = 0.95F;
constexpr float kPlayerAttackAnimDuration = 0.65F;
constexpr float kPlayerHitReactDuration = 0.28F;
constexpr float kMoveTargetMarkerWorldHeight = 0.82F;
constexpr float kLaneCenterZ = 60.0F;
constexpr float kLaneOrthoHeight = 16.0F;
constexpr float kLaneRunSpeed = 6.2F;

bool worldToScreen(
    const glm::vec3& worldPosition,
    const glm::mat4& view,
    const glm::mat4& projection,
    int screenWidth,
    int screenHeight,
    float& outX,
    float& outY) {
    const glm::vec4 clip = projection * view * glm::vec4(worldPosition, 1.0F);
    if (clip.w <= 0.001F) {
        return false;
    }

    const glm::vec3 normalizedDevice = glm::vec3(clip) / clip.w;

    outX = (normalizedDevice.x * 0.5F + 0.5F) * static_cast<float>(screenWidth);
    outY = (1.0F - (normalizedDevice.y * 0.5F + 0.5F)) * static_cast<float>(screenHeight);
    return true;
}

struct MobScreenPlate {
    ui::Rect bar{};
    ui::Rect name{};
    bool elite{false};
    float healthRatio{1.0F};
    std::string label;
};

[[nodiscard]] const char* itemIconFrame(const systems::ItemCategory category) noexcept {
    switch (category) {
    case systems::ItemCategory::Weapon:
        return "weapon";
    case systems::ItemCategory::OffHand:
        return "offhand";
    case systems::ItemCategory::Head:
        return "head";
    case systems::ItemCategory::Shoulders:
        return "shoulders";
    case systems::ItemCategory::Chest:
        return "chest";
    case systems::ItemCategory::Hands:
        return "hands";
    case systems::ItemCategory::Waist:
        return "waist";
    case systems::ItemCategory::Legs:
        return "legs";
    case systems::ItemCategory::Feet:
        return "feet";
    case systems::ItemCategory::Amulet:
        return "amulet";
    case systems::ItemCategory::Ring:
        return "ring";
    case systems::ItemCategory::Cloak:
        return "cloak";
    case systems::ItemCategory::Charm:
        return "charm";
    case systems::ItemCategory::Relic:
        return "relic";
    case systems::ItemCategory::Consumable:
        return "potion";
    case systems::ItemCategory::Material:
        return "gem";
    case systems::ItemCategory::Misc:
        break;
    }
    return "charm";
}

[[nodiscard]] const char* skillIconFrame(const systems::SkillId id) noexcept {
    switch (id) {
    case systems::SkillId::PowerStrike:
        return "skill_power_strike";
    case systems::SkillId::Whirlwind:
        return "skill_whirlwind";
    case systems::SkillId::Heal:
        return "skill_heal";
    case systems::SkillId::Dash:
        return "skill_dash";
    case systems::SkillId::Cleave:
        return "skill_cleave";
    case systems::SkillId::Firebolt:
        return "skill_firebolt";
    case systems::SkillId::Shout:
        return "skill_shout";
    case systems::SkillId::Slam:
        return "skill_slam";
    case systems::SkillId::None:
        break;
    }
    return nullptr;
}

[[nodiscard]] const char* menuIconFrame(const int index) noexcept {
    switch (index) {
    case 0:
        return "menu_abilities";
    case 1:
        return "menu_inventory";
    case 2:
        return "menu_map";
    case 3:
        return "menu_settings";
    case 4:
        return "menu_pause";
    default:
        break;
    }
    return nullptr;
}

[[nodiscard]] const char* equipmentSlotIcon(const systems::EquipmentSlotKind slot) noexcept {
    switch (slot) {
    case systems::EquipmentSlotKind::Head:
        return "head";
    case systems::EquipmentSlotKind::Shoulders:
        return "shoulders";
    case systems::EquipmentSlotKind::Chest:
        return "chest";
    case systems::EquipmentSlotKind::Hands:
        return "hands";
    case systems::EquipmentSlotKind::Waist:
        return "waist";
    case systems::EquipmentSlotKind::Legs:
        return "legs";
    case systems::EquipmentSlotKind::Feet:
        return "feet";
    case systems::EquipmentSlotKind::Weapon:
        return "weapon";
    case systems::EquipmentSlotKind::OffHand:
        return "offhand";
    case systems::EquipmentSlotKind::Amulet:
        return "amulet";
    case systems::EquipmentSlotKind::RingLeft:
    case systems::EquipmentSlotKind::RingRight:
        return "ring";
    case systems::EquipmentSlotKind::Cloak:
        return "cloak";
    case systems::EquipmentSlotKind::Charm:
        return "charm";
    case systems::EquipmentSlotKind::Relic:
        return "relic";
    case systems::EquipmentSlotKind::Count:
        break;
    }
    return nullptr;
}

[[nodiscard]] const char* lootCeilingLabel(const gameplay::LootCeiling ceiling) noexcept {
    switch (ceiling) {
    case gameplay::LootCeiling::Magic:
        return "Magic";
    case gameplay::LootCeiling::Legendary:
        return "Legendary";
    case gameplay::LootCeiling::Unique:
        return "Unique";
    }
    return "Magic";
}

} // namespace

struct GameApplication::Impl {
    engine::Window& window;
    std::string assetsRoot;

    engine::Shader worldShader;
    engine::Shader wireShader;
    render::Mesh cubeMesh;
    render::Mesh cubeWireMesh;
    render::UiRenderer uiRenderer;
    render::TextRenderer textRenderer;
    render::UiAssets uiAssets;
    render::MobAssets mobAssets;
    render::WorldPropAssets worldPropAssets;
    render::SpriteRenderer spriteRenderer;
    render::ParticleRenderer particleRenderer;
    render::ParticleSystem particles_{};
    std::unordered_map<std::uint32_t, glm::vec2> entityLastXZ_{};
    glm::vec2 lastPlayerXZ_{0.0F};
    float spriteAnimTime_{0.0F};
    float playerAttackAnimTime_{0.0F};
    float playerHitReactTime_{0.0F};
    render::SpriteFacing playerFacing_{render::SpriteFacing::Down};
    render::AnimationStateMachine playerAnim_{};
    std::unordered_map<std::uint32_t, render::AnimationStateMachine> mobAnimations_{};
    std::unordered_map<std::uint32_t, render::SpriteFacing8> entityFacing8_{};

    struct DyingMob {
        std::uint32_t id{0};
        gameplay::EntityKind kind{gameplay::EntityKind::ENEMY_MOB};
        glm::vec3 position{0.0F};
        render::AnimationStateMachine anim{};
    };
    std::vector<DyingMob> dyingMobs_{};

    gameplay::GameStateManager stateManager;
    gameplay::ZoneManager zoneManager;
    gameplay::IsometricCamera camera;

    systems::Inventory playerInventory{6, 4};
    systems::Equipment playerEquipment{};
    systems::Inventory vendorInventory{6, 3};
    systems::TradeSystem tradeSystem;
    systems::SlotMachineLoot lootEngine;
    systems::RunProgression runProgression_;
    systems::SkillBar skillBar_{};
    systems::InventoryDrag inventoryDrag_{};
    render::GeneratedUiAtlas generatedUi_{};
    render::GeneratedUiAtlas itemIcons_{};
    gameplay::LaneBattle lane_{};
    bool laneActive_{false};
    int selectedNode_{0};
    bool nodeMapOpen_{false};
    systems::TownHub townHub_{};
    bool tavernPanelOpen_{false};
    bool healerPanelOpen_{false};
    std::string townNotice_{};
    int hoveredTownHotspot_{-1};
    render::Texture townBackdrop_{};
    std::array<render::Texture, 7> townPlates_{};
    bool townBackdropReady_{false};
    std::unordered_set<std::uint32_t> laneMobIds_{};
    std::unordered_set<std::uint32_t> eliteIds_{};
    render::Texture groundShadow_{};
    render::Texture menuBackdrop_{};
    enum class PointerCursor : std::uint8_t { Default = 0, Enemy, Loot, Slot, Count };
    GLFWcursor* pointerCursors_[static_cast<int>(PointerCursor::Count)]{};
    PointerCursor activePointer_{PointerCursor::Default};

    ui::OverlayState overlayState{6, 4};
    ui::MinimapSystem minimap;
    ui::UiInteractionRegistry uiInteraction_;
    GameSettings gameSettings{};

    int playerCurrentHealth_{0};

    AppScreen appScreen{AppScreen::MAIN_MENU};
    SettingsOrigin settingsOrigin{SettingsOrigin::MAIN_MENU};
    CharacterClass selectedClass{CharacterClass::NONE};
    int hoveredButtonId{-1};

    bool gamePaused{false};
    bool pauseSettingsOpen{false};
    std::optional<std::uint32_t> hoveredInteractableId{};
    std::uint32_t lastHoveredLogId_{0xFFFFFFFFU};
    std::optional<int> hoveredInventorySlot_{};
    std::optional<int> hoveredEquipmentSlot_{};
    std::optional<int> hoveredStatUpgradeButton_{};
    std::optional<int> hoveredTradePlayerSlot_{};
    std::optional<int> hoveredTradeVendorSlot_{};
    std::optional<int> hoveredBlacksmithService_{};
    std::optional<SaveGameSnapshot> loadPreviewSnapshot_{};

    struct SpriteDrawCommand {
        const gameplay::WorldEntitySnapshot* entity{nullptr};
        const DyingMob* dying{nullptr};
        bool isPlayer{false};
        bool isWorldProp{false};
        float sortKey{0.0F};
        std::uint32_t sortId{0};
    };

    std::vector<SpriteDrawCommand> spriteDrawCommands_{};
    std::vector<ui::MinimapBlip> minimapBlips_{};
    gameplay::GroundAabb visibleGround_{};
    float smoothedFps_{0.0F};
    char fpsLabel_[16]{"-- fps"};
    mutable std::unordered_map<std::uint32_t, std::size_t> entityIndexById_{};
    mutable std::uint32_t cachedSceneryRevision_{0};
    std::uint32_t lastCombatSyncRevision_{0xFFFFFFFFU};
    double lastBenchmarkMedianFrameMs_{0.0};
    std::uint64_t frameIndex_{0};
    std::uint64_t uiHitRegionsFrameBuilt_{0};

    struct CachedTooltip {
        ui::TooltipBoxLayout layout{};
        std::vector<std::string> lines;
        float scale{1.85F};
    };

    struct CachedItemCard {
        ui::TooltipBoxLayout layout{};
        std::vector<systems::TooltipLine> lines;
        float scale{1.7F};
        systems::ItemRarity rarity{systems::ItemRarity::Common};
    };

    std::optional<CachedTooltip> cachedItemTooltip_;
    std::optional<CachedItemCard> cachedItemCard_;
    std::optional<CachedItemCard> cachedCompareCard_;
    std::optional<CachedTooltip> cachedInteractableTooltip_;
    ui::LootPresentation lootPresentation_{};
    int hoveredHudMenu_{-1};

    struct PendingLootLabel {
        std::string name;
        float red{0.8F};
        float green{0.8F};
        float blue{0.8F};
        int rank{0};
        float intensity{0.55F};
    };
    std::vector<PendingLootLabel> pendingLootLabels_{};

    CombatSystem combatSystem;
    CombatFeedback combatFeedback_{};
    std::mt19937 combatRng_{0x5EED1234U};
    float attackCooldownSeconds_{0.0F};
    std::unordered_map<std::uint32_t, float> mobAttackCooldowns_{};
    std::vector<FloatingCombatText> floatingCombatTexts;

    glm::vec3 playerPosition{0.0F, 0.0F, 0.0F};
    glm::vec3 moveTarget{0.0F, 0.0F, 0.0F};
    bool hasMoveTarget{false};
    float playerYaw{0.0F};
    double lastFrameTime{0.0};
    std::string hudMessage;

    bool keyWasDown[512]{};
    bool mouseWasDown{false};
    static Impl* inputOwner_;

    bool useSyntheticMouse_{false};
    float syntheticMouseX_{0.0F};
    float syntheticMouseY_{0.0F};
    bool pendingSyntheticClick_{false};
    std::vector<int> pendingSyntheticKeys_;

    static void onFramebufferResizeCallback(int width, int height, void* userData) {
        static_cast<Impl*>(userData)->onFramebufferResize(width, height);
    }

    explicit Impl(engine::Window& windowRef, std::string assets)
        : window(windowRef),
          assetsRoot(std::move(assets)),
          worldShader(
              joinPath(assetsRoot, "shaders/default.vert"),
              joinPath(assetsRoot, "shaders/default.frag")),
          wireShader(
              joinPath(assetsRoot, "shaders/wire.vert"),
              joinPath(assetsRoot, "shaders/wire.frag")),
          cubeMesh(render::Mesh::createUnitCube()),
          cubeWireMesh(render::Mesh::createUnitCubeWireframe()),
          uiRenderer(
              joinPath(assetsRoot, "shaders/ui.vert"),
              joinPath(assetsRoot, "shaders/ui.frag")),
          textRenderer(
              joinPath(assetsRoot, "shaders/text.vert"),
              joinPath(assetsRoot, "shaders/text.frag")),
          spriteRenderer(
              joinPath(assetsRoot, "shaders/sprite.vert"),
              joinPath(assetsRoot, "shaders/sprite.frag")),
          particleRenderer(
              joinPath(assetsRoot, "shaders/particle.vert"),
              joinPath(assetsRoot, "shaders/particle.frag")),
          tradeSystem(playerInventory, vendorInventory) {
        inputOwner_ = this;
        glfwSetMouseButtonCallback(
            window.handle(),
            [](GLFWwindow*, int button, int action, int) {
                if (inputOwner_ != nullptr && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                    inputOwner_->pendingSyntheticClick_ = true;
                }
            });
        glfwSetKeyCallback(
            window.handle(),
            [](GLFWwindow*, int key, int, int action, int) {
                if (inputOwner_ != nullptr && action == GLFW_PRESS && key >= 0) {
                    inputOwner_->pendingSyntheticKeys_.push_back(key);
                }
            });
        logInfo("Assets root: " + assetsRoot);
        logInfo("World shader compiled.");
        logInfo("UI shader compiled.");
        if (uiAssets.load(joinPath(assetsRoot, "textures/ui"))) {
            logInfo("UI textures loaded.");
        } else {
            logInfo("Warning: UI textures failed to load; using flat-color UI fallback.");
        }
        if (mobAssets.load(joinPath(assetsRoot, "textures/mobs"))) {
            if (mobAssets.hasClassSheets()) {
                logInfo("Class sprite sheets loaded (warrior, ranger, mage).");
            } else {
                logInfo("Warning: Class sprite sheets missing; using legacy mob sprites for player.");
            }
        } else {
            logInfo("Warning: Mob textures failed to load; using cube placeholders for characters.");
        }
        if (worldPropAssets.load(joinPath(assetsRoot, "textures/world"))) {
            logInfo("World prop sprites loaded from assets.png slices.");
        } else {
            logInfo("Warning: World prop textures failed to load; using cube placeholders for scenery.");
        }
        logInfo("Mesh buffers created.");
        logInfo("Initializing gameplay systems...");
        seedStarterItems();
        tradeSystem.bindVendor("Blacksmith", 250);
        tradeSystem.setPlayerGold(zoneManager.player().gold());

        camera.setViewportSize(window.width(), window.height());
        uiRenderer.resize(window.width(), window.height());
        textRenderer.resize(window.width(), window.height());
        window.setFramebufferResizeCallback(onFramebufferResizeCallback, this);

        const gameplay::Vec3 startPos = zoneManager.player().position();
        playerPosition = toGlm(startPos);

        const std::string generatedDir = joinPath(assetsRoot, "textures/generated");
        if (generatedUi_.load(generatedDir)) {
            logInfo("Generated UI atlas loaded.");
        }
        if (itemIcons_.load(generatedDir, "items_atlas.json", "items_atlas.png")) {
            logInfo("Item icon atlas loaded.");
        } else {
            logInfo("Warning: item icon atlas missing; inventory falls back to letters.");
        }
        if (!groundShadow_.loadFromFile(generatedDir + "/shadow_blob.png", true)) {
            logInfo("Warning: ground shadow texture missing.");
        }
        if (menuBackdrop_.loadFromFile(generatedDir + "/menu_background.png", false)) {
            logInfo("Menu background loaded.");
        } else {
            logInfo("Menu background missing; using the flat backdrop.");
        }
        installPointerCursors(generatedDir);

        logInfo("Booting into Main Menu.");
        logHelp("Click Play to choose Warrior, Ranger, or Mage.");
    }

    ~Impl() {
        if (inputOwner_ == this) {
            inputOwner_ = nullptr;
        }
        for (GLFWcursor*& cursor : pointerCursors_) {
            if (cursor != nullptr) {
                glfwDestroyCursor(cursor);
                cursor = nullptr;
            }
        }
    }

    [[nodiscard]] GLFWcursor* loadCursorImage(const std::string& path) const {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr || width <= 0 || height <= 0) {
            return nullptr;
        }
        GLFWimage image{};
        image.width = width;
        image.height = height;
        image.pixels = pixels;
        GLFWcursor* cursor = glfwCreateCursor(&image, 1, 1);
        stbi_image_free(pixels);
        return cursor;
    }

    void installPointerCursors(const std::string& directory) {
        const char* files[] = {"cursor.png", "cursor_enemy.png", "cursor_loot.png", "cursor_slot.png"};
        for (int index = 0; index < static_cast<int>(PointerCursor::Count); ++index) {
            pointerCursors_[index] = loadCursorImage(directory + "/" + files[index]);
        }
        if (pointerCursors_[0] != nullptr) {
            glfwSetCursor(window.handle(), pointerCursors_[0]);
            activePointer_ = PointerCursor::Default;
        }
    }

    void applyPointerCursor(const PointerCursor kind) {
        const int index = static_cast<int>(kind);
        GLFWcursor* cursor = pointerCursors_[index] != nullptr ? pointerCursors_[index] : pointerCursors_[0];
        if (cursor == nullptr || kind == activePointer_) {
            return;
        }
        glfwSetCursor(window.handle(), cursor);
        activePointer_ = kind;
    }

    void syncGameplayPointer() {
        PointerCursor kind = PointerCursor::Default;
        const bool overSlot = inventoryDrag_.active() || hoveredInventorySlot_.has_value() ||
                              hoveredEquipmentSlot_.has_value() || hoveredTradePlayerSlot_.has_value() ||
                              hoveredTradeVendorSlot_.has_value();
        if (overSlot) {
            kind = PointerCursor::Slot;
        } else if (hoveredInteractableId.has_value()) {
            if (const gameplay::WorldEntitySnapshot* entity = findEntityById(*hoveredInteractableId)) {
                if (isAttackableEntity(entity->kind)) {
                    kind = PointerCursor::Enemy;
                } else if (entity->kind == gameplay::EntityKind::ENV_CHEST) {
                    kind = PointerCursor::Loot;
                }
            }
        }
        applyPointerCursor(kind);
    }

    void onFramebufferResize(int width, int height) {
        camera.setViewportSize(width, height);
        uiRenderer.resize(width, height);
        textRenderer.resize(width, height);
        gameSettings.resolutionWidth = width;
        gameSettings.resolutionHeight = height;
        std::ostringstream message;
        message << "Viewport resized to " << width << "x" << height;
        logInfo(message.str());
    }

    [[nodiscard]] ui::UiScale currentUiScale() const noexcept {
        return ui::UiScale(window.width(), window.height());
    }

    [[nodiscard]] static ui::ScreenAnchor screenAnchorFor(const MinimapAnchor anchor) noexcept {
        switch (anchor) {
        case MinimapAnchor::TopLeft:
            return ui::ScreenAnchor::TopLeft;
        case MinimapAnchor::BottomRight:
            return ui::ScreenAnchor::BottomRight;
        case MinimapAnchor::BottomLeft:
            return ui::ScreenAnchor::BottomLeft;
        case MinimapAnchor::TopRight:
        default:
            return ui::ScreenAnchor::TopRight;
        }
    }

    [[nodiscard]] ui::MinimapWidgetLayout minimapWidgetLayout() const {
        return ui::computeMinimapWidgetLayout(
            currentUiScale(),
            screenAnchorFor(gameSettings.minimapAnchor),
            gameSettings.minimapMarginX,
            gameSettings.minimapMarginY,
            gameSettings.minimapSize);
    }

    [[nodiscard]] ui::TextWidthMeasureFn textMeasureFn() const {
        return [this](const char* text, const float scale) {
            return textRenderer.measureTextWidth(text, scale);
        };
    }

    void drawBoundedText(
        const ui::Rect& bounds,
        const std::string& text,
        const float scale,
        const float color[4]) const {
        const std::string clipped =
            ui::truncateWithEllipsis(text, bounds.width, scale, textMeasureFn());
        const float textWidth = textRenderer.measureTextWidth(clipped.c_str(), scale);
        const float x = bounds.x + std::max(0.0F, (bounds.width - textWidth) * 0.5F);
        const float fontHeight = 8.0F * scale;
        const float y = bounds.y + std::max(0.0F, (bounds.height - fontHeight) * 0.5F);
        textRenderer.drawText(x, y, clipped.c_str(), scale, color);
    }

    void rebuildUiHitRegions() {
        ui::InGameUiVisibility visibility{};
        visibility.inventoryVisible = overlayState.inventoryOverlay().visible;
        visibility.characterVisible = overlayState.characterScreen().visible;
        visibility.trading = stateManager.currentState() == gameplay::GameState::TRADING;

        ui::buildInGameHitRegions(
            currentUiScale(),
            visibility,
            overlayState.inventoryOverlay().columns,
            overlayState.inventoryOverlay().rows,
            playerInventory.columns(),
            playerInventory.rows(),
            vendorInventory.columns(),
            vendorInventory.rows(),
            screenAnchorFor(gameSettings.minimapAnchor),
            gameSettings.minimapMarginX,
            gameSettings.minimapMarginY,
            gameSettings.minimapSize,
            uiInteraction_);
    }

    void ensureUiHitRegionsBuilt() {
        if (uiHitRegionsFrameBuilt_ == frameIndex_) {
            return;
        }
        rebuildUiHitRegions();
        uiHitRegionsFrameBuilt_ = frameIndex_;
    }

    void invalidateUiHitRegions() {
        uiHitRegionsFrameBuilt_ = 0;
    }

    void seedStarterItems() {
        playerInventory.addItem(
            {101U,
             "Traveler Blade",
             systems::ItemRarity::Common,
             12,
             systems::ItemCategory::Weapon,
             'S',
             systems::ItemStatBonuses{0, 0, 0, 0, 0.0F, 2, 0.0F}});
        playerInventory.addItem(
            {102U, "Health Tonic", systems::ItemRarity::Common, 5, systems::ItemCategory::Consumable, 'P'});
        playerInventory.addItem(
            {201U,
             "Scout Charm",
             systems::ItemRarity::Magic,
             30,
             systems::ItemCategory::Charm,
             'C',
             systems::ItemStatBonuses{0, 0, 1, 8, 0.0F, 0, 0.0F}});
        static_cast<void>(playerEquipment.equipFromInventory(playerInventory, 0));
        vendorInventory.addItem(
            {301U,
             "Forged Sword",
             systems::ItemRarity::Magic,
             80,
             systems::ItemCategory::Weapon,
             'S',
             systems::ItemStatBonuses{1, 0, 0, 0, 0.0F, 6, 0.0F}});
        vendorInventory.addItem(
            {302U,
             "Plate Vest",
             systems::ItemRarity::Magic,
             120,
             systems::ItemCategory::Chest,
             'A',
             systems::ItemStatBonuses{0, 0, 2, 30, 0.0F, 0, 0.0F}});
        vendorInventory.addItem(
            {401U,
             "Dragon Scale",
             systems::ItemRarity::Rare,
             500,
             systems::ItemCategory::Cloak,
             'D',
             systems::ItemStatBonuses{2, 0, 2, 40, 0.0F, 4, 0.0F}});
        overlayState.syncInventoryVisibility(playerInventory.usedSlots());
    }

    void setScreen(AppScreen screen) {
        if (appScreen == screen) {
            return;
        }
        appScreen = screen;
        logInfo(std::string("Screen -> ") + appScreenName(screen));

        if (screen == AppScreen::MAIN_MENU) {
            if (SaveGameIO::saveExists()) {
                logHelp("START | CONTINUE | SETTINGS | EXIT");
            } else {
                logHelp("START | SETTINGS | EXIT");
            }
        } else if (screen == AppScreen::CHARACTER_SELECT) {
            logHelp("Click a class box: Warrior (red), Ranger (green), Mage (purple).");
        } else if (screen == AppScreen::LOAD_CHARACTER) {
            logHelp("Select your saved character to continue, or Back to main menu.");
        } else if (screen == AppScreen::SETTINGS) {
            logHelp("Settings placeholder. Click Back to return.");
        } else if (screen == AppScreen::IN_GAME) {
            hudMessage = "Town is still in ruins. Fight for gold and levels, then repair the buildings.";
            logHelp(hudMessage);
        }
    }

    void applyClassStats(CharacterClass characterClass) {
        ui::CharacterScreenData& stats = overlayState.characterScreen();
        stats.level = 1;
        stats.experience = 0;
        stats.experienceToNextLevel = systems::experienceRequiredForLevel(1);
        stats.carriedSouls = 0;
        stats.statUpgradesPurchased = 0;
        systems::resetSoulGainMultiplier(stats);
        stats.unspentPoints = 0;
        stats.strength = 10;
        stats.dexterity = 10;
        stats.vitality = 10;

        switch (characterClass) {
        case CharacterClass::WARRIOR:
            stats.strength = 16;
            stats.vitality = 14;
            break;
        case CharacterClass::RANGER:
            stats.dexterity = 16;
            stats.vitality = 12;
            break;
        case CharacterClass::MAGE:
            stats.strength = 8;
            stats.dexterity = 14;
            stats.vitality = 10;
            break;
        case CharacterClass::NONE:
            break;
        }
    }

    void beginGameplay(CharacterClass characterClass) {
        selectedClass = characterClass;
        applyClassStats(characterClass);
        syncRunDifficulty();
        stateManager.transitionTo(gameplay::GameState::TOWN);
        gamePaused = false;
        pauseSettingsOpen = false;
        hasMoveTarget = false;
        combatSystem.reset();
        attackCooldownSeconds_ = 0.0F;
        floatingCombatTexts.clear();
        lootPresentation_.clear();
        pendingLootLabels_.clear();
        playerCurrentHealth_ = effectiveCharacterStats().maxHealth;
        invalidateSceneryCaches();
        setScreen(AppScreen::IN_GAME);

        std::ostringstream message;
        message << "Adventure started as " << characterClassName(characterClass) << " in Town.";
        logInfo(message.str());
    }

    [[nodiscard]] SaveGameSnapshot collectSaveSnapshot() const {
        SaveGameSnapshot snapshot{};
        snapshot.characterClass = selectedClass;

        const ui::CharacterScreenData& stats = overlayState.characterScreen();
        snapshot.character.level = stats.level;
        snapshot.character.experience = stats.experience;
        snapshot.character.experienceToNextLevel = stats.experienceToNextLevel;
        snapshot.character.carriedSouls = stats.carriedSouls;
        snapshot.character.statUpgradesPurchased = stats.statUpgradesPurchased;
        snapshot.character.soulGainMultiplier = stats.soulGainMultiplier;
        snapshot.character.strength = stats.strength;
        snapshot.character.dexterity = stats.dexterity;
        snapshot.character.vitality = stats.vitality;
        snapshot.character.unspentPoints = stats.unspentPoints;
        snapshot.character.currentHealth = playerCurrentHealth_;
        snapshot.character.gold = tradeSystem.playerGold();

        snapshot.progression.depth = runProgression_.depth();
        snapshot.progression.runSeed = runProgression_.runSeed();
        snapshot.progression.totalBossKills = runProgression_.totalBossKills();
        snapshot.progression.mobsKilledThisDepth = runProgression_.mobsKilledThisDepth();
        snapshot.progression.lifetimeMobKills = runProgression_.lifetimeMobKills();
        snapshot.progression.lootCoinPool = lootEngine.coinPool();
        snapshot.progression.lootRngSeed = lootEngine.rngSeed();
        snapshot.progression.lootPityCounter = lootEngine.pityCounter();
        snapshot.progression.difficultyTier = static_cast<int>(runProgression_.tier());
        snapshot.progression.townRepairMask = townHub_.repairMask();

        snapshot.world.activeZone = zoneManager.activeZone();
        snapshot.world.playerPosition = zoneManager.player().position();
        snapshot.world.attacksEnabled = zoneManager.player().attacksEnabled();
        snapshot.world.plainsSeed = zoneManager.plainsSeed();
        snapshot.world.plainsDepth = zoneManager.plainsDepth();
        snapshot.world.scenery = zoneManager.scenery();
        for (const MobHealthSaveEntry& entry : combatSystem.collectMobHealthEntries()) {
            snapshot.world.mobHealth.push_back(
                SaveMobHealthEntry{entry.entityId, entry.currentHp, entry.maxHp});
        }

        snapshot.inventoryColumns = playerInventory.columns();
        snapshot.inventoryRows = playerInventory.rows();
        for (int index = 0; index < playerInventory.capacity(); ++index) {
            SaveInventorySlot slot{};
            slot.index = index;
            if (playerInventory.isSlotOccupied(index)) {
                slot.item = *playerInventory.slotAt(index).item;
            }
            snapshot.inventory.push_back(std::move(slot));
        }

        for (int index = 0; index < static_cast<int>(systems::EquipmentSlotKind::Count); ++index) {
            const auto slotKind = static_cast<systems::EquipmentSlotKind>(index);
            SaveEquipmentSlot slot{};
            slot.slot = slotKind;
            if (playerEquipment.isSlotOccupied(slotKind)) {
                slot.item = *playerEquipment.itemAt(slotKind);
            }
            snapshot.equipment.push_back(std::move(slot));
        }

        return snapshot;
    }

    [[nodiscard]] bool applySaveSnapshot(const SaveGameSnapshot& snapshot) {
        if (snapshot.characterClass == CharacterClass::NONE) {
            return false;
        }

        selectedClass = snapshot.characterClass;

        ui::CharacterScreenData& stats = overlayState.characterScreen();
        stats.level = snapshot.character.level;
        stats.experience = snapshot.character.experience;
        stats.experienceToNextLevel = snapshot.character.experienceToNextLevel;
        stats.carriedSouls = snapshot.character.carriedSouls;
        if (stats.carriedSouls == 0 && snapshot.character.experience > 0) {
            stats.carriedSouls = snapshot.character.experience;
        }
        stats.statUpgradesPurchased = snapshot.character.statUpgradesPurchased;
        stats.soulGainMultiplier = snapshot.character.soulGainMultiplier;
        if (stats.soulGainMultiplier < systems::kInitialSoulGainMultiplier) {
            stats.soulGainMultiplier = systems::kInitialSoulGainMultiplier;
        }
        stats.strength = snapshot.character.strength;
        stats.dexterity = snapshot.character.dexterity;
        stats.vitality = snapshot.character.vitality;
        stats.unspentPoints = snapshot.character.unspentPoints;
        stats.visible = false;

        runProgression_.applyState(
            snapshot.progression.depth,
            snapshot.progression.runSeed,
            snapshot.progression.totalBossKills,
            snapshot.progression.mobsKilledThisDepth,
            snapshot.progression.lifetimeMobKills);
        runProgression_.setTier(systems::difficultyTierFromIndex(snapshot.progression.difficultyTier));
        lootEngine.setSeed(snapshot.progression.lootRngSeed);
        lootEngine.setCoinPool(snapshot.progression.lootCoinPool);
        lootEngine.setPityCounter(snapshot.progression.lootPityCounter);
        townHub_.applyRepairMask(snapshot.progression.townRepairMask);
        syncRunDifficulty();
        refreshManaPool();
        skillBar_.restoreMana();
        skillBar_.resetCooldowns();

        playerEquipment.clearAll();
        for (const SaveEquipmentSlot& slot : snapshot.equipment) {
            if (slot.item.has_value()) {
                playerEquipment.setSlot(slot.slot, *slot.item);
            }
        }

        std::vector<systems::InventorySlot> inventorySlots(
            static_cast<std::size_t>(playerInventory.capacity()));
        for (const SaveInventorySlot& slot : snapshot.inventory) {
            if (!playerInventory.isValidSlot(slot.index)) {
                continue;
            }
            inventorySlots[static_cast<std::size_t>(slot.index)].item = slot.item;
        }
        playerInventory.applySavedSlots(inventorySlots);
        overlayState.syncInventoryVisibility(playerInventory.usedSlots());

        zoneManager.restoreFromSnapshot(
            snapshot.world.activeZone,
            snapshot.world.playerPosition,
            snapshot.character.gold,
            snapshot.world.attacksEnabled,
            snapshot.world.plainsSeed,
            snapshot.world.plainsDepth,
            snapshot.world.scenery);
        playerPosition = toGlm(snapshot.world.playerPosition);
        tradeSystem.setPlayerGold(snapshot.character.gold);

        const int maxHealth = effectiveCharacterStats().maxHealth;
        playerCurrentHealth_ = std::clamp(snapshot.character.currentHealth, 1, maxHealth);

        combatSystem.reset();
        invalidateSceneryCaches();
        ensureCombatSynced();

        std::vector<MobHealthSaveEntry> mobHealthEntries;
        mobHealthEntries.reserve(snapshot.world.mobHealth.size());
        for (const SaveMobHealthEntry& entry : snapshot.world.mobHealth) {
            mobHealthEntries.push_back(
                MobHealthSaveEntry{entry.entityId, entry.currentHp, entry.maxHp});
        }
        combatSystem.restoreMobHealthEntries(mobHealthEntries);

        if (snapshot.world.activeZone == gameplay::WorldZone::PLAINS) {
            zoneManager.forceRespawnInTown();
            playerPosition = toGlm(zoneManager.player().position());
            stateManager.forceState(gameplay::GameState::TOWN);
            nodeMapOpen_ = true;
            hudMessage = "You wake in town. Press M and choose the road again.";
        } else {
            stateManager.forceState(gameplay::GameState::TOWN);
            restoreExploreCamera();
        }
        stats.experienceToNextLevel =
            std::max(stats.experienceToNextLevel, systems::experienceRequiredForLevel(std::max(1, stats.level)));

        overlayState.inventoryOverlay().visible = false;
        overlayState.characterScreen().visible = false;
        gamePaused = false;
        pauseSettingsOpen = false;
        hasMoveTarget = false;
        attackCooldownSeconds_ = 0.0F;
        floatingCombatTexts.clear();
        lootPresentation_.clear();
        pendingLootLabels_.clear();
        hoveredInventorySlot_.reset();
        hoveredEquipmentSlot_.reset();
        cachedItemTooltip_.reset();
        cachedInteractableTooltip_.reset();
        invalidateUiHitRegions();

        std::ostringstream message;
        message << "Loaded " << characterClassName(selectedClass) << " (depth "
                << runProgression_.depth() << ").";
        if (nodeMapOpen_) {
            message << " Choose a road.";
        }
        hudMessage = message.str();
        logInfo(message.str());
        setScreen(AppScreen::IN_GAME);
        return true;
    }

    [[nodiscard]] SaveGameResult saveGameToDisk() {
        const SaveGameSnapshot snapshot = collectSaveSnapshot();
        const SaveGameResult result = SaveGameIO::saveToFile(snapshot);
        if (result.success) {
            logInfo("Saved to " + SaveGameIO::defaultSavePath().string());
        } else {
            logInfo("Save failed: " + result.message);
        }
        return result;
    }

    [[nodiscard]] bool loadGameFromDisk() {
        SaveGameSnapshot snapshot{};
        const SaveGameResult result = SaveGameIO::loadFromFile(snapshot);
        if (!result.success) {
            logInfo("Load failed: " + result.message);
            return false;
        }
        return applySaveSnapshot(snapshot);
    }

    bool keyPressed(int key) {
        const auto pending = std::find(pendingSyntheticKeys_.begin(), pendingSyntheticKeys_.end(), key);
        if (pending != pendingSyntheticKeys_.end()) {
            pendingSyntheticKeys_.erase(pending);
            keyWasDown[key] = true;
            return true;
        }

        GLFWwindow* handle = window.handle();
        const bool down = glfwGetKey(handle, key) == GLFW_PRESS;
        const bool pressed = down && !keyWasDown[key];
        keyWasDown[key] = down;
        return pressed;
    }

    void readMousePosition(float& outX, float& outY) const {
        if (useSyntheticMouse_) {
            outX = syntheticMouseX_;
            outY = syntheticMouseY_;
            return;
        }

        GLFWwindow* handle = window.handle();
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(handle, &mouseX, &mouseY);

        int framebufferWidth = window.width();
        int framebufferHeight = window.height();
        int windowWidth = framebufferWidth;
        int windowHeight = framebufferHeight;
        glfwGetWindowSize(handle, &windowWidth, &windowHeight);
        if (windowWidth > 0 && windowHeight > 0) {
            const float scaleX =
                static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);
            const float scaleY =
                static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight);
            outX = static_cast<float>(mouseX) * scaleX;
            outY = static_cast<float>(mouseY) * scaleY;
            return;
        }

        outX = static_cast<float>(mouseX);
        outY = static_cast<float>(mouseY);
    }

    bool mouseClicked(float& outX, float& outY) {
        readMousePosition(outX, outY);

        if (pendingSyntheticClick_) {
            pendingSyntheticClick_ = false;
            mouseWasDown = true;
            return true;
        }

        GLFWwindow* handle = window.handle();
        const bool down = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool clicked = down && !mouseWasDown;
        mouseWasDown = down;
        return clicked;
    }

    void simulateMouseMove(const float screenX, const float screenY) {
        useSyntheticMouse_ = true;
        syntheticMouseX_ = screenX;
        syntheticMouseY_ = screenY;
        glfwSetCursorPos(window.handle(), static_cast<double>(screenX), static_cast<double>(screenY));
    }

    void simulateMouseClick(const float screenX, const float screenY) {
        simulateMouseMove(screenX, screenY);
        pendingSyntheticClick_ = true;
    }

    void simulateKeyPress(const int glfwKey) {
        pendingSyntheticKeys_.push_back(glfwKey);
    }

    [[nodiscard]] bool projectWorldToScreen(
        const float worldX,
        const float worldY,
        const float worldZ,
        float& screenX,
        float& screenY) const {
        const gameplay::CameraMatrices cameraMatrices =
            camera.matricesForTarget(cameraFocus());
        return worldToScreen(
            glm::vec3(worldX, worldY, worldZ),
            cameraMatrices.view,
            cameraMatrices.projection,
            window.width(),
            window.height(),
            screenX,
            screenY);
    }

    [[nodiscard]] bool tryGetNearestAttackableMobScreenPosition(float& screenX, float& screenY) {
        ensureCombatSynced();

        float nearestDistanceSquared = std::numeric_limits<float>::max();
        bool foundMob = false;
        glm::vec3 nearestMobPosition{0.0F};

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !isAttackableEntity(entity.kind) ||
                !combatSystem.isMobAlive(entity.id)) {
                continue;
            }

            glm::vec3 offset = toGlm(entity.position) - playerPosition;
            offset.y = 0.0F;
            const float distanceSquared = glm::dot(offset, offset);
            if (distanceSquared >= nearestDistanceSquared) {
                continue;
            }

            nearestDistanceSquared = distanceSquared;
            nearestMobPosition = toGlm(entity.position);
            foundMob = true;
        }

        if (!foundMob) {
            return false;
        }

        return projectWorldToScreen(
            nearestMobPosition.x,
            nearestMobPosition.y,
            nearestMobPosition.z,
            screenX,
            screenY);
    }

    [[nodiscard]] bool engageNearestMobForTest() {
        ensureCombatSynced();

        float nearestDistanceSquared = std::numeric_limits<float>::max();
        std::optional<std::uint32_t> nearestMobId{};
        glm::vec3 nearestMobPosition{0.0F};

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !isAttackableEntity(entity.kind) ||
                !combatSystem.isMobAlive(entity.id)) {
                continue;
            }

            glm::vec3 offset = toGlm(entity.position) - playerPosition;
            offset.y = 0.0F;
            const float distanceSquared = glm::dot(offset, offset);
            if (distanceSquared >= nearestDistanceSquared) {
                continue;
            }

            nearestDistanceSquared = distanceSquared;
            nearestMobId = entity.id;
            nearestMobPosition = toGlm(entity.position);
        }

        if (!nearestMobId.has_value()) {
            return false;
        }

        combatSystem.setTarget(*nearestMobId);
        attackCooldownSeconds_ = 0.0F;
        hasMoveTarget = false;

        glm::vec3 offset = playerPosition - nearestMobPosition;
        offset.y = 0.0F;
        if (glm::length(offset) < 0.05F) {
            offset = glm::vec3(1.0F, 0.0F, 0.0F);
        }
        playerPosition = nearestMobPosition + glm::normalize(offset) * 1.5F;
        playerPosition.y = 0.0F;
        zoneManager.updatePlayerPosition(toVec3(playerPosition));
        playerPosition = toGlm(zoneManager.player().position());
        return true;
    }

    void pollHover(float mouseX, float mouseY, const std::vector<MenuButton>& buttons) {
        hoveredButtonId = -1;
        for (const MenuButton& button : buttons) {
            if (button.bounds.contains(mouseX, mouseY)) {
                hoveredButtonId = button.id;
                break;
            }
        }
    }

    void drawMenuButtonLabel(const ui::Rect& bounds, const char* label, const bool hovered = false) const {
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.9F};
        const float textColor[4] = {hovered ? 1.0F : 0.93F, hovered ? 0.92F : 0.78F, hovered ? 0.62F : 0.42F, 1.0F};
        const float labelScale = currentUiScale().dim(2.2F);
        const float shadowOffset = currentUiScale().dim(2.0F);
        const ui::Rect shadowBounds{
            bounds.x + shadowOffset, bounds.y + shadowOffset, bounds.width, bounds.height};
        textRenderer.drawTextCentered(shadowBounds, label, labelScale, shadowColor);
        textRenderer.drawTextCentered(bounds, label, labelScale, textColor);
    }

    void drawPanelButtonBackground(
        const ui::Rect& bounds,
        bool hovered,
        bool enabled) const {
        drawRpgButton(bounds, hovered, enabled);
    }

    void drawPanelButtonLabel(const ui::Rect& bounds, const char* label, bool enabled) const {
        const float textColor[4] = {0.95F, 0.88F, 0.62F, enabled ? 1.0F : 0.45F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.85F};
        const float offset = currentUiScale().dim(1.0F);
        const ui::Rect shadowBounds{bounds.x + offset, bounds.y + offset, bounds.width, bounds.height};
        textRenderer.drawTextCentered(shadowBounds, label, currentUiScale().dim(1.25F), shadowColor);
        textRenderer.drawTextCentered(bounds, label, currentUiScale().dim(1.25F), textColor);
    }

    void drawPanelButton(
        const ui::Rect& bounds,
        const char* label,
        bool hovered,
        bool enabled) const {
        drawPanelButtonBackground(bounds, hovered, enabled);
        drawPanelButtonLabel(bounds, label, enabled);
    }

    void drawMenuButtonBackground(
        const MenuButton& button,
        const float baseColor[4],
        const float hoverColor[4]) const {
        const bool hovered = hoveredButtonId == button.id;
        const float* fill = hovered ? hoverColor : baseColor;
        if (hovered) {
            const float glow[4] = {0.85F, 0.58F, 0.16F, 0.35F};
            const float pad = currentUiScale().dim(5.0F);
            uiRenderer.drawFilledRect(
                button.bounds.x - pad,
                button.bounds.y - pad,
                button.bounds.width + pad * 2.0F,
                button.bounds.height + pad * 2.0F,
                glow);
        }
        uiRenderer.drawFilledRect(
            button.bounds.x, button.bounds.y, button.bounds.width, button.bounds.height, fill);
        drawRpgFrame(button.bounds, currentUiScale().dim(5.0F));
    }

    void drawButton(const MenuButton& button, const float baseColor[4], const float hoverColor[4]) {
        drawMenuButtonBackground(button, baseColor, hoverColor);
    }

    void drawTexturedBar(
        const render::Texture& frame,
        const render::Texture& fill,
        const float x,
        const float y,
        const float width,
        const float height,
        const float fillRatio) const {
        const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        uiRenderer.drawTexturedRect(frame, x, y, width, height, white);

        constexpr float kBarInsetX = 14.0F;
        constexpr float kBarInsetY = 8.0F;
        const float innerWidth = std::max(width - kBarInsetX * 2.0F, 0.0F);
        const float innerHeight = std::max(height - kBarInsetY * 2.0F, 8.0F);
        const float fillWidth = innerWidth * std::clamp(fillRatio, 0.0F, 1.0F);
        if (fillWidth <= 0.5F) {
            return;
        }

        const float fillX = x + kBarInsetX;
        const float fillY = y + (height - innerHeight) * 0.5F;
        const float u1 = std::clamp(fillRatio, 0.001F, 1.0F);
        uiRenderer.drawTexturedRectUV(fill, fillX, fillY, fillWidth, innerHeight, 0.0F, 0.0F, u1, 1.0F, white);
    }

    [[nodiscard]] static CharacterClass classFromButtonId(const int buttonId) noexcept {
        switch (buttonId) {
        case 10:
            return CharacterClass::WARRIOR;
        case 11:
            return CharacterClass::RANGER;
        case 12:
            return CharacterClass::MAGE;
        default:
            return CharacterClass::NONE;
        }
    }

    [[nodiscard]] render::SpriteFacing facingFromDelta(const glm::vec2& delta) const noexcept {
        return spriteFacingFromDelta(delta);
    }

    void advanceSpriteAnimClock() {
        static double lastClock = 0.0;
        const double now = glfwGetTime();
        if (lastClock > 0.0) {
            spriteAnimTime_ += static_cast<float>(now - lastClock);
        }
        lastClock = now;
    }

    void updatePlayerSpriteAnimation(const float deltaSeconds) {
        spriteAnimTime_ += deltaSeconds;
        if (playerAttackAnimTime_ > 0.0F) {
            playerAttackAnimTime_ = std::max(0.0F, playerAttackAnimTime_ - deltaSeconds);
        }
        if (playerHitReactTime_ > 0.0F) {
            playerHitReactTime_ = std::max(0.0F, playerHitReactTime_ - deltaSeconds);
        }

        const glm::vec2 playerXZ(playerPosition.x, playerPosition.z);
        if (hasMoveTarget) {
            const glm::vec2 moveDelta = glm::vec2(moveTarget.x, moveTarget.z) - playerXZ;
            playerAnim_.setFacingFromDelta(moveDelta);
        } else {
            playerAnim_.setFacingFromDelta(playerXZ - lastPlayerXZ_);
        }
        playerFacing_ = playerAnim_.facing4();
        playerAnim_.update(deltaSeconds, isPlayerMoving());
    }

    void updateEntityAnimations(const float deltaSeconds) {
        for (auto iterator = mobAnimations_.begin(); iterator != mobAnimations_.end();) {
            const gameplay::WorldEntitySnapshot* entity = findEntityById(iterator->first);
            if (entity == nullptr || !entity->active) {
                iterator = mobAnimations_.erase(iterator);
                continue;
            }
            const glm::vec2 currentXZ(entity->position.x, entity->position.z);
            iterator->second.update(deltaSeconds, isEntityMoving(entity->id, currentXZ));
            if (!iterator->second.isBusy()) {
                iterator = mobAnimations_.erase(iterator);
                continue;
            }
            ++iterator;
        }

        for (DyingMob& dying : dyingMobs_) {
            dying.anim.update(deltaSeconds, false);
        }
        dyingMobs_.erase(
            std::remove_if(
                dyingMobs_.begin(),
                dyingMobs_.end(),
                [](const DyingMob& dying) { return dying.anim.finished(); }),
            dyingMobs_.end());
    }

    void triggerMobHitAnimation(const std::uint32_t entityId) {
        mobAnimations_[entityId].triggerHit(0.22F);
    }

    void spawnDyingMob(const gameplay::WorldEntitySnapshot& entity) {
        DyingMob dying{};
        dying.id = entity.id;
        dying.kind = entity.kind;
        dying.position = toGlm(entity.position);
        const auto facing = entityFacing8_.find(entity.id);
        if (facing != entityFacing8_.end()) {
            dying.anim.setFacing(facing->second);
        }
        dying.anim.triggerDeath(entity.kind == gameplay::EntityKind::ENEMY_BOSS ? 0.9F : 0.55F);
        dyingMobs_.push_back(dying);
        mobAnimations_.erase(entity.id);
        entityFacing8_.erase(entity.id);
    }

    [[nodiscard]] render::SpriteClip resolvePlayerClip() const noexcept {
        if (playerAnim_.isBusy()) {
            return playerAnim_.clip();
        }
        if (isPlayerMoving()) {
            return render::SpriteClip::Walk;
        }
        return render::SpriteClip::Idle;
    }

    void drawClassSpriteUi(
        const CharacterClass characterClass,
        const float x,
        const float y,
        const float width,
        const float height,
        const render::SpriteClip clip) const {
        if (!mobAssets.hasClassSheets() || characterClass == CharacterClass::NONE) {
            return;
        }

        const render::SpriteFrameSample sample = mobAssets.sampleClassSprite(
            characterClass, clip, render::SpriteFacing::Down, spriteAnimTime_);
        if (sample.texture == nullptr) {
            return;
        }

        const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        uiRenderer.drawTexturedRectUV(
            *sample.texture,
            x,
            y,
            width,
            height,
            sample.uv.u0,
            sample.uv.v0,
            sample.uv.u1,
            sample.uv.v1,
            white);
    }

    void drawTitle(const char* title, float y, float scale) const {
        const ui::UiScale layout = currentUiScale();
        const float textColor[4] = {0.95F, 0.82F, 0.42F, 1.0F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.85F};
        const float titleWidth = layout.dim(640.0F);
        const float titleHeight = layout.dim(48.0F);
        const float shadowOffset = layout.dim(3.0F);
        ui::Rect titleBounds{
            static_cast<float>(layout.width) * 0.5F - titleWidth * 0.5F,
            layout.y(y),
            titleWidth,
            titleHeight};
        const ui::Rect shadowBounds{
            titleBounds.x + shadowOffset, titleBounds.y + shadowOffset, titleBounds.width, titleBounds.height};
        textRenderer.drawTextCentered(shadowBounds, title, layout.dim(scale), shadowColor);
        textRenderer.drawTextCentered(titleBounds, title, layout.dim(scale), textColor);
    }

    std::vector<MenuButton> buildMainMenuButtons() const {
        const ui::UiScale layout = currentUiScale();
        const float centerX = static_cast<float>(layout.width) * 0.5F;
        const float buttonW = layout.dim(280.0F);
        const float buttonH = layout.dim(56.0F);
        const float gap = layout.dim(18.0F);
        const bool hasSave = SaveGameIO::saveExists();
        const float startY =
            hasSave ? static_cast<float>(layout.height) * 0.34F
                    : static_cast<float>(layout.height) * 0.42F;

        std::vector<MenuButton> buttons{
            {{centerX - buttonW * 0.5F, startY, buttonW, buttonH}, "New Game", 1},
        };
        if (hasSave) {
            buttons.push_back(
                {{centerX - buttonW * 0.5F, startY + buttonH + gap, buttonW, buttonH},
                 "CONTINUE",
                 4});
        }
        const float settingsY = startY + (buttonH + gap) * (hasSave ? 2.0F : 1.0F);
        const float exitY = settingsY + buttonH + gap;
        buttons.push_back(
            {{centerX - buttonW * 0.5F, settingsY, buttonW, buttonH}, "Options", 2});
        buttons.push_back(
            {{centerX - buttonW * 0.5F, exitY, buttonW, buttonH}, "Exit", 3});
        return buttons;
    }

    void handleMainMenuInput() {
        std::vector<MenuButton> buttons = buildMainMenuButtons();
        float mouseX = 0.0F;
        float mouseY = 0.0F;
        const bool clicked = mouseClicked(mouseX, mouseY);
        pollHover(mouseX, mouseY, buttons);

        if (!clicked) {
            return;
        }

        for (const MenuButton& button : buttons) {
            if (!button.bounds.contains(mouseX, mouseY)) {
                continue;
            }
            if (button.id == 1) {
                logInfo("Play selected -> Character Select.");
                setScreen(AppScreen::CHARACTER_SELECT);
            } else if (button.id == 4) {
                logInfo("Continue selected -> Load Character.");
                if (!prepareLoadCharacterScreen()) {
                    logHelp("Could not read save file. Start a new game instead.");
                } else {
                    setScreen(AppScreen::LOAD_CHARACTER);
                }
            } else if (button.id == 2) {
                logInfo("Settings opened.");
                settingsOrigin = SettingsOrigin::MAIN_MENU;
                setScreen(AppScreen::SETTINGS);
            } else if (button.id == 3) {
                logInfo("Exit selected. Closing application.");
                glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
            }
            break;
        }
    }

    std::vector<MenuButton> buildClassButtons() const {
        const ui::UiScale layout = currentUiScale();
        const float panelY = static_cast<float>(layout.height) * 0.35F;
        const float boxW = layout.dim(220.0F);
        const float boxH = layout.dim(280.0F);
        const float gap = layout.dim(36.0F);
        const float totalWidth = boxW * 3.0F + gap * 2.0F;
        const float startX = static_cast<float>(layout.width) * 0.5F - totalWidth * 0.5F;

        return {
            {{startX, panelY, boxW, boxH}, "Warrior", 10},
            {{startX + boxW + gap, panelY, boxW, boxH}, "Ranger", 11},
            {{startX + (boxW + gap) * 2.0F, panelY, boxW, boxH}, "Mage", 12},
        };
    }

    MenuButton buildBackButton(const float y) const {
        const ui::UiScale layout = currentUiScale();
        const float buttonW = layout.dim(180.0F);
        const float buttonH = layout.dim(44.0F);
        const float x = static_cast<float>(layout.width) * 0.5F - buttonW * 0.5F;
        return {{x, y, buttonW, buttonH}, "BACK", 99};
    }

    void handleCharacterSelectInput() {
        std::vector<MenuButton> buttons = buildClassButtons();
        const MenuButton back = buildBackButton(static_cast<float>(window.height()) * 0.78F);
        buttons.push_back(back);

        float mouseX = 0.0F;
        float mouseY = 0.0F;
        const bool clicked = mouseClicked(mouseX, mouseY);
        pollHover(mouseX, mouseY, buttons);

        if (keyPressed(GLFW_KEY_ESCAPE)) {
            logInfo("Returning to Main Menu.");
            setScreen(AppScreen::MAIN_MENU);
            return;
        }

        if (!clicked) {
            return;
        }

        if (back.bounds.contains(mouseX, mouseY)) {
            setScreen(AppScreen::MAIN_MENU);
            return;
        }

        for (const MenuButton& button : buttons) {
            if (!button.bounds.contains(mouseX, mouseY)) {
                continue;
            }
            if (button.id == 10) {
                beginGameplay(CharacterClass::WARRIOR);
            } else if (button.id == 11) {
                beginGameplay(CharacterClass::RANGER);
            } else if (button.id == 12) {
                beginGameplay(CharacterClass::MAGE);
            }
            break;
        }
    }

    [[nodiscard]] bool prepareLoadCharacterScreen() {
        loadPreviewSnapshot_.reset();
        SaveGameSnapshot snapshot{};
        const SaveGameResult result = SaveGameIO::loadFromFile(snapshot);
        if (!result.success || snapshot.characterClass == CharacterClass::NONE) {
            return false;
        }
        loadPreviewSnapshot_ = std::move(snapshot);
        return true;
    }

    std::vector<MenuButton> buildLoadCharacterButtons() const {
        const ui::UiScale layout = currentUiScale();
        const float boxW = layout.dim(280.0F);
        const float boxH = layout.dim(320.0F);
        const float x = static_cast<float>(layout.width) * 0.5F - boxW * 0.5F;
        const float y = static_cast<float>(layout.height) * 0.32F;
        const char* label = "Saved Hero";
        if (loadPreviewSnapshot_.has_value()) {
            label = characterClassName(loadPreviewSnapshot_->characterClass);
        }
        return {
            {{x, y, boxW, boxH}, label, 40},
            buildBackButton(static_cast<float>(layout.height) * 0.78F),
        };
    }

    void handleLoadCharacterInput() {
        std::vector<MenuButton> buttons = buildLoadCharacterButtons();
        float mouseX = 0.0F;
        float mouseY = 0.0F;
        const bool clicked = mouseClicked(mouseX, mouseY);
        pollHover(mouseX, mouseY, buttons);

        if (keyPressed(GLFW_KEY_ESCAPE)) {
            logInfo("Returning to Main Menu.");
            loadPreviewSnapshot_.reset();
            setScreen(AppScreen::MAIN_MENU);
            return;
        }

        if (!clicked) {
            return;
        }

        for (const MenuButton& button : buttons) {
            if (!button.bounds.contains(mouseX, mouseY)) {
                continue;
            }
            if (button.id == 99) {
                loadPreviewSnapshot_.reset();
                setScreen(AppScreen::MAIN_MENU);
            } else if (button.id == 40 && loadPreviewSnapshot_.has_value()) {
                logInfo("Loading saved character.");
                SaveGameSnapshot snapshot = std::move(*loadPreviewSnapshot_);
                loadPreviewSnapshot_.reset();
                if (!applySaveSnapshot(snapshot)) {
                    logHelp("Could not load save. Start a new game instead.");
                    setScreen(AppScreen::MAIN_MENU);
                }
            }
            break;
        }
    }

    void returnToMainMenuFromPause() {
        closeTransientOverlays();
        if (stateManager.currentState() == gameplay::GameState::TRADING) {
            stateManager.leaveTrading();
        }
        gamePaused = false;
        pauseSettingsOpen = false;
        combatSystem.clearTarget();
        hasMoveTarget = false;
        setScreen(AppScreen::MAIN_MENU);
    }

    void applyGameSettings() {
        window.setWindowSize(gameSettings.resolutionWidth, gameSettings.resolutionHeight);
        invalidateUiHitRegions();
        std::ostringstream message;
        message << "Resolution set to " << gameSettings.resolutionWidth << "x" << gameSettings.resolutionHeight;
        logInfo(message.str());
    }

    void syncPlayerHealth() {
        const int maxHealth = effectiveCharacterStats().maxHealth;
        if (playerCurrentHealth_ <= 0) {
            if (zoneManager.activeZone() == gameplay::WorldZone::PLAINS) {
                handlePlayerDeath();
                return;
            }
            playerCurrentHealth_ = maxHealth;
        }
        playerCurrentHealth_ = std::min(playerCurrentHealth_, maxHealth);
    }

    void handlePlayerDeath() {
        ui::CharacterScreenData& stats = overlayState.characterScreen();
        const int lostSouls = stats.carriedSouls;
        stats.carriedSouls = 0;
        systems::resetSoulGainMultiplier(stats);
        endLane();

        zoneManager.forceRespawnInTown();
        playerPosition = toGlm(zoneManager.player().position());
        stateManager.forceState(gameplay::GameState::TOWN);
        combatSystem.clearTarget();
        hasMoveTarget = false;
        attackCooldownSeconds_ = 0.0F;
        mobAttackCooldowns_.clear();
        floatingCombatTexts.clear();
        lootPresentation_.clear();
        pendingLootLabels_.clear();
        particles_.clear();
        combatFeedback_.reset();
        combatFeedback_.addFlash(glm::vec3(0.6F, 0.02F, 0.04F), 0.75F, 1.2F);
        mobAnimations_.clear();
        dyingMobs_.clear();
        playerAnim_.reset();
        skillBar_.resetCooldowns();
        skillBar_.restoreMana();

        playerCurrentHealth_ = effectiveCharacterStats().maxHealth;

        std::ostringstream message;
        message << "YOU DIED. Lost " << lostSouls
                << " souls. Your pack is safe — sell or buy in town, then press M.";
        hudMessage = message.str();
        logInfo(hudMessage);
    }

    void syncPlayerGoldFromTrade() {
        zoneManager.player().setGold(tradeSystem.playerGold());
    }

    [[nodiscard]] systems::BlacksmithUnlockState blacksmithUnlockState() const noexcept {
        return {
            runProgression_.lifetimeMobKills(),
            runProgression_.totalBossKills(),
            runProgression_.depth()};
    }

    void tryBlacksmithService(const systems::BlacksmithServiceKind service) {
        const systems::BlacksmithUnlockState unlocks = blacksmithUnlockState();
        if (!systems::isBlacksmithServiceUnlocked(service, unlocks)) {
            hudMessage = systems::blacksmithUnlockHint(service);
            logInfo(hudMessage);
            return;
        }

        int playerGold = tradeSystem.playerGold();
        ui::CharacterScreenData& stats = overlayState.characterScreen();
        systems::BlacksmithResult result{false, "Unknown service"};

        switch (service) {
        case systems::BlacksmithServiceKind::TemperWeapon:
            result = systems::temperEquippedWeapon(playerEquipment, playerGold);
            break;
        case systems::BlacksmithServiceKind::ReinforceGear:
            result = systems::reinforceEquippedGear(playerEquipment, playerGold);
            break;
        case systems::BlacksmithServiceKind::ReforgeBackpack: {
            std::optional<int> targetSlot = hoveredTradePlayerSlot_;
            if (!targetSlot.has_value()) {
                for (int index = 0; index < playerInventory.capacity(); ++index) {
                    if (playerInventory.isSlotOccupied(index)) {
                        targetSlot = index;
                        break;
                    }
                }
            }
            if (!targetSlot.has_value() || !playerInventory.isSlotOccupied(*targetSlot)) {
                result = {false, "Select a backpack item to reforge"};
                break;
            }
            const std::uint32_t seed = runProgression_.runSeed() ^
                                       static_cast<std::uint32_t>(*targetSlot + 1) * 0x9E3779B9U;
            systems::ItemMetadata item = *playerInventory.slotAt(*targetSlot).item;
            result = systems::reforgeBackpackItem(item, seed, playerGold);
            if (result.success) {
                playerInventory.discardAt(*targetSlot);
                playerInventory.addItemAt(item, *targetSlot);
            }
            break;
        }
        case systems::BlacksmithServiceKind::MasterworkEquipped:
            result = systems::masterworkEquippedWeapon(playerEquipment, playerGold);
            break;
        case systems::BlacksmithServiceKind::SoulInfusion:
            result = systems::soulInfuseEquippedWeapon(
                playerEquipment, playerGold, stats.carriedSouls);
            break;
        case systems::BlacksmithServiceKind::Count:
            break;
        }

        tradeSystem.setPlayerGold(playerGold);
        syncPlayerGoldFromTrade();
        hudMessage = result.message;
        if (result.success) {
            syncPlayerHealth();
        }
        logInfo(hudMessage);
    }

    void trySoulStatUpgrade(const systems::SoulStatKind stat) {
        ui::CharacterScreenData& stats = overlayState.characterScreen();
        const bool inTown = zoneManager.activeZone() == gameplay::WorldZone::TOWN;
        const systems::SoulUpgradeResult result =
            systems::tryPurchaseStatUpgrade(stats, stat, inTown);
        hudMessage = result.message;
        if (!result.success) {
            logInfo(hudMessage);
            return;
        }

        if (stat == systems::SoulStatKind::Vitality) {
            playerCurrentHealth_ = effectiveCharacterStats().maxHealth;
        } else {
            syncPlayerHealth();
        }

        logInfo(hudMessage);
    }

    void updateMobMeleeThreats(const float deltaSeconds) {
        if (gamePaused || zoneManager.activeZone() != gameplay::WorldZone::PLAINS) {
            return;
        }
        if (stateManager.isPausedForUi()) {
            return;
        }

        const systems::DifficultyModifiers modifiers = runProgression_.modifiers();

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !isAttackableEntity(entity.kind)) {
                continue;
            }
            if (!combatSystem.isMobAlive(entity.id)) {
                continue;
            }

            glm::vec3 toPlayer = playerPosition - toGlm(entity.position);
            toPlayer.y = 0.0F;
            const float distanceSquared = glm::dot(toPlayer, toPlayer);
            constexpr float kMobAggroRadiusSq = kMobAggroRadius * kMobAggroRadius;
            constexpr float kMobMeleeRangeSq = kMobMeleeRange * kMobMeleeRange;
            if (distanceSquared > kMobAggroRadiusSq) {
                continue;
            }

            if (distanceSquared > kMobMeleeRangeSq) {
                continue;
            }

            float& cooldown = mobAttackCooldowns_[entity.id];
            cooldown -= deltaSeconds;
            if (cooldown > 0.0F) {
                continue;
            }

            int damage = 0;
            if (laneActive_) {
                const float tierDamage =
                    systems::difficultyTierModifiers(runProgression_.tier()).damageMultiplier;
                float scale =
                    (0.48F + static_cast<float>(lane_.node().enemyLevel) * 0.055F) * tierDamage;
                if (eliteIds_.count(entity.id) != 0U) {
                    scale *= 1.42F;
                }
                damage = systems::mobMeleeDamage(entity.kind, 1.0F, scale);
            } else {
                damage = systems::mobMeleeDamage(
                    entity.kind, modifiers.mobHpMultiplier, modifiers.mobDamageMultiplier);
            }
            playerCurrentHealth_ -= damage;
            playerHitReactTime_ = kPlayerHitReactDuration;
            playerAnim_.triggerHit(kPlayerHitReactDuration);
            combatFeedback_.addTrauma(0.2F);
            combatFeedback_.addFlash(glm::vec3(0.75F, 0.05F, 0.05F), 0.22F, 0.25F);
            spawnFloatingCombatText(
                playerPosition,
                std::to_string(damage),
                0.95F,
                0.25F,
                0.22F,
                1.4F,
                1.6F);

            cooldown = entity.kind == gameplay::EntityKind::ENEMY_BOSS ? kMobAttackCooldownBoss
                                                                       : kMobAttackCooldownMob;

            if (playerCurrentHealth_ <= 0) {
                handlePlayerDeath();
                return;
            }
        }
    }

    [[nodiscard]] ui::SettingsPanelLayout settingsPanelLayout() const {
        return ui::computeSettingsPanelLayout(currentUiScale());
    }

    [[nodiscard]] std::vector<SettingsControl> buildSettingsControls() {
        const ui::SettingsPanelLayout layout = settingsPanelLayout();
        const int* rowIds = kSettingsRowIds;

        std::vector<SettingsControl> controls;
        controls.reserve(ui::SettingsPanelLayout::kRowCount);
        for (int index = 0; index < ui::SettingsPanelLayout::kRowCount; ++index) {
            const ui::SettingsRowLayout& row = layout.rows[index];
            SettingsControl control{};
            control.bounds = row.control;
            control.id = rowIds[index];
            control.kind = row.kind == ui::SettingsRowKind::Slider ? SettingsControlKind::Slider
                                                                   : SettingsControlKind::Cycle;

            switch (control.id) {
            case kSettingsVolume:
                control.sliderValue = &gameSettings.masterVolume;
                control.sliderMin = 0.0F;
                control.sliderMax = 1.0F;
                break;
            case kSettingsMinimapSize:
                control.sliderValue = &gameSettings.minimapSize;
                control.sliderMin = 120.0F;
                control.sliderMax = 280.0F;
                break;
            case kSettingsMouseSensitivity:
                control.sliderValue = &gameSettings.mouseSensitivity;
                control.sliderMin = 0.5F;
                control.sliderMax = 2.0F;
                break;
            default:
                break;
            }

            controls.push_back(control);
        }

        return controls;
    }

    [[nodiscard]] std::string settingsControlValueLabel(const int controlId) const {
        std::ostringstream value;
        switch (controlId) {
        case kSettingsResolution:
            value << gameSettings.resolutionWidth << " x " << gameSettings.resolutionHeight;
            break;
        case kSettingsMinimapAnchor:
            value << minimapAnchorLabel(gameSettings.minimapAnchor);
            break;
        case kSettingsGraphicsQuality:
            value << graphicsQualityLabel(gameSettings.graphicsQuality);
            break;
        case kSettingsDifficulty:
            value << systems::difficultyTierLabel(runProgression_.tier());
            if (!runProgression_.isTierUnlocked(nextDifficultyTier())) {
                value << " *";
            }
            break;
        case kSettingsVolume:
            value << static_cast<int>(gameSettings.masterVolume * 100.0F) << '%';
            break;
        case kSettingsMinimapSize:
            value << static_cast<int>(gameSettings.minimapSize) << " px";
            break;
        case kSettingsMouseSensitivity:
            value << gameSettings.mouseSensitivity;
            break;
        default:
            break;
        }
        return value.str();
    }

    [[nodiscard]] const char* settingsRowLabel(const int rowIndex) const noexcept {
        static constexpr const char* kLabels[ui::SettingsPanelLayout::kRowCount] = {
            "Resolution",
            "Master Volume",
            "Minimap Size",
            "Minimap Position",
            "Mouse Sensitivity",
            "Graphics Quality",
            "Difficulty",
        };
        if (rowIndex < 0 || rowIndex >= ui::SettingsPanelLayout::kRowCount) {
            return "";
        }
        return kLabels[rowIndex];
    }

    void setSliderFromMouse(const SettingsControl& control, const float mouseX) {
        if (control.sliderValue == nullptr || control.bounds.width <= 0.0F) {
            return;
        }

        const float normalized =
            std::clamp((mouseX - control.bounds.x) / control.bounds.width, 0.0F, 1.0F);
        *control.sliderValue = control.sliderMin + (control.sliderMax - control.sliderMin) * normalized;
    }

    void activateSettingsControl(const SettingsControl& control) {
        switch (control.id) {
        case kSettingsResolution:
            cycleResolution(gameSettings);
            applyGameSettings();
            break;
        case kSettingsVolume:
            break;
        case kSettingsMinimapSize:
            break;
        case kSettingsMinimapAnchor:
            cycleMinimapAnchor(gameSettings);
            break;
        case kSettingsMouseSensitivity:
            break;
        case kSettingsGraphicsQuality:
            cycleGraphicsQuality(gameSettings);
            logInfo(std::string("Graphics quality -> ") + graphicsQualityLabel(gameSettings.graphicsQuality));
            break;
        case kSettingsDifficulty:
            cycleDifficultyTier();
            break;
        default:
            break;
        }
    }

    [[nodiscard]] systems::DifficultyTier nextDifficultyTier() const noexcept {
        const int next = (static_cast<int>(runProgression_.tier()) + 1) % systems::kDifficultyTierCount;
        return systems::difficultyTierFromIndex(next);
    }

    void cycleDifficultyTier() {
        systems::DifficultyTier candidate = nextDifficultyTier();
        // Skip locked tiers (Nightmare needs 1 boss kill, Hell needs 3) and wrap back to Normal.
        for (int attempt = 0; attempt < systems::kDifficultyTierCount; ++attempt) {
            if (runProgression_.isTierUnlocked(candidate)) {
                break;
            }
            hudMessage = std::string(systems::difficultyTierLabel(candidate)) + " locked: defeat more bosses";
            logInfo(hudMessage);
            candidate = systems::difficultyTierFromIndex(
                (static_cast<int>(candidate) + 1) % systems::kDifficultyTierCount);
        }
        if (candidate == runProgression_.tier()) {
            return;
        }
        runProgression_.setTier(candidate);
        syncRunDifficulty();
        if (zoneManager.activeZone() == gameplay::WorldZone::PLAINS) {
            refreshPlainsRun();
        }
        hudMessage = std::string("Difficulty -> ") + systems::difficultyTierLabel(candidate);
        logInfo(hudMessage);
    }

    void handleSettingsInput() {
        const MenuButton back = buildBackButton(static_cast<float>(window.height()) * 0.78F);
        const std::vector<SettingsControl> controls = buildSettingsControls();
        float mouseX = 0.0F;
        float mouseY = 0.0F;
        const bool clicked = mouseClicked(mouseX, mouseY);

        std::vector<MenuButton> hoverTargets;
        hoverTargets.push_back(back);
        for (const SettingsControl& control : controls) {
            if (control.kind == SettingsControlKind::Cycle) {
                hoverTargets.push_back({control.bounds, "Adjust", control.id});
            }
        }
        pollHover(mouseX, mouseY, hoverTargets);

        if (clicked) {
            if (back.bounds.contains(mouseX, mouseY)) {
                if (settingsOrigin == SettingsOrigin::PAUSE_MENU) {
                    logInfo("Returning to pause menu.");
                    pauseSettingsOpen = false;
                    appScreen = AppScreen::IN_GAME;
                } else {
                    logInfo("Returning to Main Menu.");
                    setScreen(AppScreen::MAIN_MENU);
                }
                return;
            }

            for (const SettingsControl& control : controls) {
                if (!control.bounds.contains(mouseX, mouseY)) {
                    continue;
                }
                if (control.kind == SettingsControlKind::Slider) {
                    setSliderFromMouse(control, mouseX);
                    if (control.id == kSettingsVolume) {
                        std::ostringstream message;
                        message << "Master volume " << static_cast<int>(gameSettings.masterVolume * 100.0F) << "%";
                        logInfo(message.str());
                    }
                } else {
                    activateSettingsControl(control);
                }
                break;
            }
        }

        if (keyPressed(GLFW_KEY_ESCAPE)) {
            if (settingsOrigin == SettingsOrigin::PAUSE_MENU) {
                pauseSettingsOpen = false;
                appScreen = AppScreen::IN_GAME;
            } else {
                setScreen(AppScreen::MAIN_MENU);
            }
        }
    }

    [[nodiscard]] ui::Rect minimapFrameRect() const { return minimapWidgetLayout().frame; }

    [[nodiscard]] ui::Rect playerStatusHudRect() const {
        return ui::computeHudChromeLayout(currentUiScale()).statusHud;
    }

    [[nodiscard]] render::SpriteFacing spriteFacingFromDelta(const glm::vec2& delta) const noexcept {
        constexpr float kEpsilonSq = 0.0004F;
        if (glm::dot(delta, delta) <= kEpsilonSq) {
            return playerFacing_;
        }

        if (std::abs(delta.x) > std::abs(delta.y)) {
            return delta.x >= 0.0F ? render::SpriteFacing::Right : render::SpriteFacing::Left;
        }
        return delta.y >= 0.0F ? render::SpriteFacing::Down : render::SpriteFacing::Up;
    }

    void drawSegmentedBar(
        float x,
        float y,
        float width,
        float height,
        int segmentCount,
        float fillRatio,
        const float fillColor[4],
        const float emptyColor[4]) const {
        if (segmentCount <= 0 || width <= 0.0F) {
            return;
        }

        const float gap = 2.0F;
        const float segmentWidth =
            (width - gap * static_cast<float>(segmentCount - 1)) / static_cast<float>(segmentCount);
        const int filledSegments =
            std::clamp(static_cast<int>(std::round(fillRatio * static_cast<float>(segmentCount))), 0, segmentCount);

        for (int segment = 0; segment < segmentCount; ++segment) {
            const float segmentX = x + static_cast<float>(segment) * (segmentWidth + gap);
            const float* color = segment < filledSegments ? fillColor : emptyColor;
            uiRenderer.drawFilledRect(segmentX, y, segmentWidth, height, color);
        }
    }

    [[nodiscard]] static const char* entityDisplayName(const gameplay::EntityKind kind) noexcept {
        switch (kind) {
        case gameplay::EntityKind::ENEMY_MOB:
            return "Plains Mob";
        case gameplay::EntityKind::ENEMY_BOSS:
            return "World Boss";
        case gameplay::EntityKind::NPC_BLACKSMITH:
            return "Blacksmith";
        case gameplay::EntityKind::PLAYER:
            return "Player";
        case gameplay::EntityKind::ENV_TREE:
            return "Tree";
        case gameplay::EntityKind::ENV_BUSH:
            return "Bush";
        case gameplay::EntityKind::ENV_CHEST:
            return "Chest";
        case gameplay::EntityKind::ENV_ROCK:
            return "Rock";
        case gameplay::EntityKind::ENV_HOUSE:
            return "House";
        case gameplay::EntityKind::ENV_MUSHROOM:
            return "Mushroom";
        }
        return "Unknown";
    }

    [[nodiscard]] bool worldReadoutsHidden() const noexcept {
        return gamePaused || nodeMapOpen_ || overlayState.inventoryOverlay().visible ||
            overlayState.characterScreen().visible ||
            stateManager.currentState() == gameplay::GameState::TRADING ||
            stateManager.currentState() == gameplay::GameState::CHARACTER_MENU;
    }

    [[nodiscard]] bool screenRectHitsChrome(const ui::Rect& rect) const {
        const float width = static_cast<float>(window.width());
        const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(currentUiScale());
        const ui::Rect topBand{0.0F, 0.0F, width, 34.0F};
        if (rectsOverlap(rect, topBand) || rectsOverlap(rect, console.panel) ||
            rectsOverlap(rect, console.messageStrip) || rectsOverlap(rect, minimapWidgetLayout().frame)) {
            return true;
        }
        return false;
    }

    void collectMobPlates(std::vector<MobScreenPlate>& plates) const {
        plates.clear();
        if (worldReadoutsHidden()) {
            return;
        }

        const gameplay::CameraMatrices cameraMatrices = camera.matricesForTarget(cameraFocus());
        const glm::vec3 cameraUp = glm::normalize(
            glm::vec3(cameraMatrices.view[0][1], cameraMatrices.view[1][1], cameraMatrices.view[2][1]));

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !isAttackableEntity(entity.kind) || !combatSystem.isMobAlive(entity.id)) {
                continue;
            }
            if (!isInsideVisibleGround(toGlm(entity.position))) {
                continue;
            }

            const std::optional<game::MobHealthSnapshot> health = combatSystem.mobHealth(entity.id);
            if (!health.has_value() || health->maxHp <= 0) {
                continue;
            }

            const float height = mobAssets.spriteWorldHeight(entity.kind);
            const glm::vec3 head = toGlm(entity.position) + glm::vec3(0.0F, height * 0.5F, 0.0F) + cameraUp * (height * 0.5F);
            float screenX = 0.0F;
            float screenY = 0.0F;
            if (!worldToScreen(
                    head,
                    cameraMatrices.view,
                    cameraMatrices.projection,
                    window.width(),
                    window.height(),
                    screenX,
                    screenY)) {
                continue;
            }

            MobScreenPlate plate{};
            constexpr float kBarWidth = 84.0F;
            constexpr float kBarHeight = 10.0F;
            plate.bar = ui::Rect{screenX - kBarWidth * 0.5F, screenY - kBarHeight - 8.0F, kBarWidth, kBarHeight};
            const bool elite = eliteIds_.count(entity.id) != 0U;
            plate.elite = elite;
            plate.healthRatio =
                static_cast<float>(health->currentHp) / static_cast<float>(std::max(health->maxHp, 1));
            plate.label = elite ? std::string("Elite ") + entityDisplayName(entity.kind)
                                : std::string(entityDisplayName(entity.kind));
            const float nameScale = 1.15F;
            const float nameWidth = std::max(textRenderer.measureTextWidth(plate.label.c_str(), nameScale), kBarWidth);
            plate.name = ui::Rect{screenX - nameWidth * 0.5F, plate.bar.y - 16.0F, nameWidth, 14.0F};
            if (screenRectHitsChrome(plate.bar)) {
                continue;
            }
            plates.push_back(std::move(plate));
        }
    }

    void renderMobHealthBars() const {
        std::vector<MobScreenPlate> plates;
        collectMobPlates(plates);
        for (const MobScreenPlate& plate : plates) {
            const float fill[4] = {
                plate.elite ? 0.92F : 0.78F,
                plate.elite ? 0.55F : 0.12F,
                plate.elite ? 0.12F : 0.1F,
                1.0F};
            drawVitalBar(plate.bar, plate.healthRatio, fill);
        }
    }

    void renderTargetMobHud() const {
        if (laneActive_ || !combatSystem.hasTarget()) {
            return;
        }

        const std::uint32_t targetId = *combatSystem.targetId();
        const gameplay::WorldEntitySnapshot* target = findEntityById(targetId);
        if (target == nullptr || !target->active) {
            return;
        }

        const std::optional<game::MobHealthSnapshot> health = combatSystem.mobHealth(targetId);
        if (!health.has_value()) {
            return;
        }

        const ui::UiScale layout = currentUiScale();
        const float barWidth = layout.dim(360.0F);
        const float barHeight = layout.dim(24.0F);
        const float barX = static_cast<float>(layout.width) * 0.5F - barWidth * 0.5F;
        const float barY = layout.y(8.0F);
        const float healthRatio = static_cast<float>(health->currentHp) / static_cast<float>(std::max(health->maxHp, 1));

        const float fillColor[4] = {0.82F, 0.18F, 0.14F, 1.0F};
        drawVitalBar({barX, barY, barWidth, barHeight}, healthRatio, fillColor);
    }

    void renderTargetMobHudLabel() const {
        if (laneActive_ || !combatSystem.hasTarget()) {
            return;
        }

        const std::uint32_t targetId = *combatSystem.targetId();
        const gameplay::WorldEntitySnapshot* target = findEntityById(targetId);
        if (target == nullptr || !target->active) {
            return;
        }

        const std::optional<game::MobHealthSnapshot> health = combatSystem.mobHealth(targetId);
        if (!health.has_value()) {
            return;
        }

        const ui::UiScale layout = currentUiScale();
        const float barWidth = layout.dim(360.0F);
        const float barHeight = layout.dim(24.0F);
        const float barX = static_cast<float>(layout.width) * 0.5F - barWidth * 0.5F;
        const float barY = layout.y(8.0F);

        const float textColor[4] = {1.0F, 0.95F, 0.9F, 1.0F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.9F};
        std::ostringstream label;
        label << (eliteIds_.count(targetId) != 0U ? "Elite " : "") << entityDisplayName(target->kind) << "  "
              << health->currentHp << " / " << health->maxHp << " HP";
        const ui::Rect labelRect{barX, barY, barWidth, barHeight};
        textRenderer.drawTextCentered(labelRect, label.str().c_str(), 1.5F, shadowColor);
        textRenderer.drawTextCentered(labelRect, label.str().c_str(), 1.5F, textColor);
    }

    void renderMobNameplates() const {
        if (!showMobNameplates()) {
            return;
        }

        std::vector<MobScreenPlate> plates;
        collectMobPlates(plates);
        const float textColor[4] = {0.96F, 0.93F, 0.84F, 1.0F};
        const float eliteColor[4] = {1.0F, 0.84F, 0.28F, 1.0F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.9F};
        for (const MobScreenPlate& plate : plates) {
            const float* color = plate.elite ? eliteColor : textColor;
            textRenderer.drawTextCentered(plate.name, plate.label.c_str(), 1.15F, shadowColor);
            textRenderer.drawTextCentered(plate.name, plate.label.c_str(), 1.15F, color);
        }
    }

    [[nodiscard]] bool isMouseOverInGameUi(float mouseX, float mouseY) const {
        return uiInteraction_.blocksWorldInput(mouseX, mouseY);
    }

    std::vector<MenuButton> buildPauseMenuButtons() const {
        const ui::UiScale layout = currentUiScale();
        const float centerX = static_cast<float>(layout.width) * 0.5F;
        const float startY = static_cast<float>(layout.height) * 0.4F;
        const float buttonW = layout.dim(320.0F);
        const float buttonH = layout.dim(52.0F);
        const float gap = layout.dim(16.0F);
        return {
            {{centerX - buttonW * 0.5F, startY, buttonW, buttonH}, "Save and Exit", 20},
            {{centerX - buttonW * 0.5F, startY + buttonH + gap, buttonW, buttonH}, "Options", 21},
            {{centerX - buttonW * 0.5F, startY + (buttonH + gap) * 2.0F, buttonW, buttonH}, "Resume", 22},
        };
    }

    void handlePauseMenuInput() {
        if (pauseSettingsOpen) {
            handleSettingsInput();
            return;
        }

        std::vector<MenuButton> buttons = buildPauseMenuButtons();
        float mouseX = 0.0F;
        float mouseY = 0.0F;
        const bool clicked = mouseClicked(mouseX, mouseY);
        pollHover(mouseX, mouseY, buttons);

        if (keyPressed(GLFW_KEY_ESCAPE)) {
            gamePaused = false;
            logInfo("Game resumed.");
            return;
        }

        if (!clicked) {
            return;
        }

        for (const MenuButton& button : buttons) {
            if (!button.bounds.contains(mouseX, mouseY)) {
                continue;
            }
            if (button.id == 20) {
                logInfo("Save and Exit selected.");
                const SaveGameResult saveResult = saveGameToDisk();
                if (saveResult.success) {
                    logHelp("Progress saved. Returning to main menu.");
                    returnToMainMenuFromPause();
                } else {
                    logHelp("Save failed: " + saveResult.message);
                }
            } else if (button.id == 21) {
                logInfo("Opening settings from pause menu.");
                settingsOrigin = SettingsOrigin::PAUSE_MENU;
                pauseSettingsOpen = true;
            } else if (button.id == 22) {
                gamePaused = false;
                logInfo("Game resumed.");
            }
            break;
        }
    }

    [[nodiscard]] systems::EffectiveCharacterStats effectiveCharacterStats() const {
        return systems::computeEffectiveStats(overlayState.characterScreen(), playerEquipment);
    }

    [[nodiscard]] systems::CombatStatInput buildCombatStats() const {
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const ui::CharacterScreenData& stats = overlayState.characterScreen();
        return systems::CombatStatInput{stats.level, effective.strength, effective.dexterity};
    }

    [[nodiscard]] ui::InventoryPaperDollLayout buildInventoryPaperDollLayout() const {
        return ui::computeInventoryPaperDollLayout(
            currentUiScale(),
            overlayState.inventoryOverlay().columns,
            overlayState.inventoryOverlay().rows);
    }

    void drawPortraitInRect(const ui::Rect& portrait) const {
        if (mobAssets.hasClassSheets() && selectedClass != CharacterClass::NONE) {
            drawClassSpriteUi(
                selectedClass,
                portrait.x,
                portrait.y,
                portrait.width,
                portrait.height,
                render::SpriteClip::Idle);
            return;
        }

        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            int portraitFrame = 0;
            if (selectedClass == CharacterClass::RANGER) {
                portraitFrame = 1;
            } else if (selectedClass == CharacterClass::MAGE) {
                portraitFrame = 2;
            }
            constexpr int kPortraitFrameCount = 9;
            const float frameU0 = static_cast<float>(portraitFrame) / static_cast<float>(kPortraitFrameCount);
            const float frameU1 =
                static_cast<float>(portraitFrame + 1) / static_cast<float>(kPortraitFrameCount);
            uiRenderer.drawTexturedRectUV(
                uiAssets.portraitSheet(),
                portrait.x,
                portrait.y,
                portrait.width,
                portrait.height,
                frameU0,
                0.0F,
                frameU1,
                1.0F,
                white);
            return;
        }

        float fill[4] = {0.45F, 0.48F, 0.55F, 1.0F};
        if (selectedClass == CharacterClass::WARRIOR) {
            fill[0] = 0.85F;
            fill[1] = 0.35F;
            fill[2] = 0.25F;
        } else if (selectedClass == CharacterClass::RANGER) {
            fill[0] = 0.3F;
            fill[1] = 0.8F;
            fill[2] = 0.35F;
        } else if (selectedClass == CharacterClass::MAGE) {
            fill[0] = 0.5F;
            fill[1] = 0.35F;
            fill[2] = 0.95F;
        }
        uiRenderer.drawFilledRect(portrait.x, portrait.y, portrait.width, portrait.height, fill);
    }

    void drawResourceBars(const ui::Rect& hpBar, const ui::Rect& xpBar) const {
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const int maxHealth = std::max(effective.maxHealth, 1);
        const float healthRatio =
            static_cast<float>(playerCurrentHealth_) / static_cast<float>(maxHealth);
        const int nextSoulUpgradeCost = systems::soulUpgradeCost(base.statUpgradesPurchased);
        const float soulRatio = nextSoulUpgradeCost > 0
                                    ? std::min(
                                          1.0F,
                                          static_cast<float>(base.carriedSouls) /
                                              static_cast<float>(nextSoulUpgradeCost))
                                    : 0.0F;

        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            const float heartSize = currentUiScale().dim(18.0F);
            uiRenderer.drawTexturedRect(
                uiAssets.heartRed(),
                hpBar.x - heartSize - 2.0F,
                hpBar.y + (hpBar.height - heartSize) * 0.5F,
                heartSize,
                heartSize,
                white);
            drawTexturedBar(
                uiAssets.barFrame(), uiAssets.hpBarFill(), hpBar.x, hpBar.y, hpBar.width, hpBar.height, healthRatio);
            uiRenderer.drawTexturedRect(
                uiAssets.heartYellow(),
                xpBar.x - heartSize - 2.0F,
                xpBar.y + (xpBar.height - heartSize) * 0.5F,
                heartSize,
                heartSize,
                white);
            drawTexturedBar(
                uiAssets.barFrame(), uiAssets.hpBarFill(), xpBar.x, xpBar.y, xpBar.width, xpBar.height, soulRatio);
            return;
        }

        const float hpFill[4] = {0.82F, 0.15F, 0.12F, 1.0F};
        const float hpEmpty[4] = {0.14F, 0.1F, 0.1F, 0.95F};
        const float xpFill[4] = {0.95F, 0.82F, 0.18F, 1.0F};
        const float xpEmpty[4] = {0.18F, 0.16F, 0.1F, 0.95F};
        drawSegmentedBar(hpBar.x, hpBar.y, hpBar.width, hpBar.height, 12, healthRatio, hpFill, hpEmpty);
        drawSegmentedBar(xpBar.x, xpBar.y, xpBar.width, xpBar.height, 12, soulRatio, xpFill, xpEmpty);
    }

    void drawResourceBarLabels(const ui::Rect& hpBar, const ui::Rect& xpBar) const {
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const int maxHealth = std::max(effective.maxHealth, 1);
        const float labelScale = currentUiScale().dim(1.25F);
        const float labelColor[4] = {0.82F, 0.86F, 0.92F, 1.0F};

        std::ostringstream hpLine;
        hpLine << playerCurrentHealth_ << " / " << maxHealth;
        const std::string hpText = hpLine.str();
        textRenderer.drawText(hpBar.x, hpBar.y - labelScale * 6.0F, hpText.c_str(), labelScale, labelColor);

        std::ostringstream soulLine;
        soulLine << "Souls " << base.carriedSouls;
        if (zoneManager.activeZone() == gameplay::WorldZone::PLAINS) {
            soulLine << " (lost on death)";
        }
        const std::string soulText = soulLine.str();
        textRenderer.drawText(xpBar.x, xpBar.y - labelScale * 6.0F, soulText.c_str(), labelScale, labelColor);
    }

    [[nodiscard]] std::vector<std::string> buildCharacterStatLines(const bool compact) const {
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const systems::ItemStatBonuses gear = systems::sumEquipmentBonuses(playerEquipment);

        std::vector<std::string> lines;
        std::ostringstream line;

        auto appendStat = [&](const char* name, int baseValue, int bonus, int total) {
            line.str("");
            line.clear();
            if (compact) {
                line << name << " " << baseValue;
                if (bonus > 0) {
                    line << " +" << bonus;
                }
                line << "  " << total;
            } else {
                line << name << ": " << baseValue;
                if (bonus > 0) {
                    line << " (+" << bonus << ")";
                }
                line << "  -> " << total;
            }
            lines.push_back(line.str());
        };

        appendStat("Strength", base.strength, gear.strength, effective.strength);
        appendStat("Dexterity", base.dexterity, gear.dexterity, effective.dexterity);
        appendStat("Vitality", base.vitality, gear.vitality, effective.vitality);

        line.str("");
        line.clear();
        line << (compact ? "Health " : "Max Health: ") << effective.maxHealth;
        lines.push_back(line.str());

        line.str("");
        line.clear();
        line << (compact ? "Damage " : "Damage: ") << effective.damage;
        lines.push_back(line.str());

        line.str("");
        line.clear();
        line << (compact ? "Atk Spd " : "Attack Speed: ") << effective.attacksPerSecond;
        lines.push_back(line.str());

        line.str("");
        line.clear();
        if (compact) {
            line << "Light " << effective.lightRadius;
        } else {
            line << "Light Radius: " << systems::kBaseLightRadius;
            if (gear.lightRadius > 0.001F) {
                line << " (+" << gear.lightRadius << ")";
            }
            line << "  -> " << effective.lightRadius;
        }
        lines.push_back(line.str());

        if (!compact) {
            lines.push_back("Bonuses from equipped gear only");
        }
        return lines;
    }

    void drawMultilineText(
        float x,
        float y,
        const std::vector<std::string>& lines,
        float scale,
        const float color[4],
        float lineHeight) const {
        float cursorY = y;
        for (const std::string& line : lines) {
            textRenderer.drawText(x, cursorY, line.c_str(), scale, color);
            cursorY += lineHeight;
        }
    }

    void drawTooltipBackground(const ui::TooltipBoxLayout& layout, const float* borderColor = nullptr) const {
        const ui::Rect& box = layout.box;
        const float gold[4] = {0.86F, 0.64F, 0.24F, 1.0F};
        const float* border = borderColor != nullptr ? borderColor : gold;
        drawRpgPanel(box);
        const float accent = std::max(3.0F, currentUiScale().dim(4.0F));
        const float inset = currentUiScale().dim(8.0F);
        if (box.width > inset * 2.0F) {
            uiRenderer.drawFilledRect(box.x + inset, box.y + inset, box.width - inset * 2.0F, accent, border);
        }
    }

    [[nodiscard]] ui::TooltipBoxLayout computeItemTooltipLayout(
        float anchorX,
        float anchorY,
        const std::vector<std::string>& lines,
        float textScale) const {
        return ui::computeTooltipBoxLayout(
            currentUiScale(),
            anchorX,
            anchorY,
            lines,
            textScale,
            textMeasureFn(),
            window.width(),
            window.height());
    }

    void drawTextTooltip(
        float anchorX,
        float anchorY,
        const std::vector<std::string>& lines,
        float scale) const {
        if (lines.empty()) {
            return;
        }

        const ui::TooltipBoxLayout layout = computeItemTooltipLayout(anchorX, anchorY, lines, scale);
        drawTooltipBackground(layout);
        drawTextTooltipTextOnly(layout, lines, scale);
    }

    void drawTextTooltipTextOnly(
        const ui::TooltipBoxLayout& layout,
        const std::vector<std::string>& lines,
        float scale) const {
        if (lines.empty()) {
            return;
        }

        const float textColor[4] = {0.93F, 0.86F, 0.68F, 1.0F};
        const float lineWidth = layout.box.width - layout.contentInsetX * 2.0F;
        float cursorY = layout.box.y + layout.contentInsetY;
        for (const std::string& line : lines) {
            const ui::Rect row{
                layout.box.x + layout.contentInsetX, cursorY, lineWidth, layout.lineHeight};
            drawBoundedText(row, line, scale, textColor);
            cursorY += layout.lineHeight + layout.lineGap;
        }
    }

    void updateInventoryHover(float mouseX, float mouseY) {
        hoveredInventorySlot_.reset();
        hoveredEquipmentSlot_.reset();
        hoveredStatUpgradeButton_.reset();
        hoveredTradePlayerSlot_.reset();
        hoveredTradeVendorSlot_.reset();
        hoveredBlacksmithService_.reset();
        hoveredHudMenu_ = -1;

        const ui::HitRegion* region = uiInteraction_.hitTest(mouseX, mouseY);
        if (region == nullptr) {
            return;
        }

        if (region->kind == ui::WidgetKind::HudMenuButton) {
            hoveredHudMenu_ = region->slotIndex;
            return;
        }

        if (region->kind == ui::WidgetKind::StatUpgradeButton) {
            hoveredStatUpgradeButton_ = region->slotIndex;
            return;
        }

        if (region->kind == ui::WidgetKind::TradePlayerSellSlot) {
            hoveredTradePlayerSlot_ = region->slotIndex;
            return;
        }

        if (region->kind == ui::WidgetKind::TradeVendorBuySlot) {
            hoveredTradeVendorSlot_ = region->slotIndex;
            return;
        }

        if (region->kind == ui::WidgetKind::BlacksmithServiceButton) {
            hoveredBlacksmithService_ = region->slotIndex;
            return;
        }

        if (region->kind == ui::WidgetKind::EquipmentSlot) {
            hoveredEquipmentSlot_ = region->slotIndex;
            return;
        }

        if (region->kind == ui::WidgetKind::InventorySlot) {
            hoveredInventorySlot_ = region->slotIndex;
        }
    }

    void handleTradeUiClick(float mouseX, float mouseY) {
        const ui::HitRegion* region = uiInteraction_.hitTest(mouseX, mouseY);
        if (region == nullptr) {
            return;
        }

        if (region->kind == ui::WidgetKind::TradePlayerSellSlot && region->slotIndex >= 0) {
            if (!playerInventory.isSlotOccupied(region->slotIndex)) {
                return;
            }

            const systems::ItemMetadata& item = *playerInventory.slotAt(region->slotIndex).item;
            const int sellValue = systems::blacksmithSellPrice(item);
            const systems::TradeResult result =
                tradeSystem.sellPlayerSlot(region->slotIndex, sellValue);
            syncPlayerGoldFromTrade();
            hudMessage = result.success
                             ? ("Sold " + item.name + " for " + std::to_string(sellValue) + "g")
                             : result.message;
            logInfo(hudMessage);
            return;
        }

        if (region->kind == ui::WidgetKind::TradeVendorBuySlot && region->slotIndex >= 0) {
            if (!vendorInventory.isSlotOccupied(region->slotIndex)) {
                return;
            }

            const systems::ItemMetadata& item = *vendorInventory.slotAt(region->slotIndex).item;
            const int buyCost = systems::blacksmithBuyPrice(item);
            const systems::TradeResult result =
                tradeSystem.buyVendorSlot(region->slotIndex, buyCost);
            syncPlayerGoldFromTrade();
            hudMessage = result.success ? ("Bought " + item.name) : result.message;
            logInfo(hudMessage);
            return;
        }

        if (region->kind == ui::WidgetKind::BlacksmithServiceButton && region->slotIndex >= 0 &&
            region->slotIndex < static_cast<int>(systems::BlacksmithServiceKind::Count)) {
            tryBlacksmithService(static_cast<systems::BlacksmithServiceKind>(region->slotIndex));
        }
    }

    void applyInventoryDrag(const float mouseX, const float mouseY) {
        const ui::HitRegion* region = uiInteraction_.hitTest(mouseX, mouseY);
        systems::DragOrigin target = systems::DragOrigin::None;
        int targetIndex = -1;
        bool targetOccupied = false;
        if (region != nullptr && region->kind == ui::WidgetKind::InventorySlot && region->slotIndex >= 0) {
            target = systems::DragOrigin::Inventory;
            targetIndex = region->slotIndex;
            targetOccupied = playerInventory.isSlotOccupied(targetIndex);
        } else if (region != nullptr && region->kind == ui::WidgetKind::EquipmentSlot && region->slotIndex >= 0) {
            target = systems::DragOrigin::Equipment;
            targetIndex = region->slotIndex;
            targetOccupied = playerEquipment.isSlotOccupied(static_cast<systems::EquipmentSlotKind>(targetIndex));
        }

        const systems::DragAction action = inventoryDrag_.release(target, targetIndex, targetOccupied);
        inventoryDrag_.cancel();

        switch (action.kind) {
        case systems::DragActionKind::Move:
        case systems::DragActionKind::Swap:
            if (playerInventory.exchangeSlots(action.from, action.to)) {
                hudMessage = action.kind == systems::DragActionKind::Swap ? "Swapped items" : "Moved item";
                logInfo(hudMessage);
            }
            break;
        case systems::DragActionKind::EquipFromInventory: {
            const systems::EquipmentActionResult result =
                playerEquipment.equipFromInventory(playerInventory, action.from);
            hudMessage = result.success ? "Equipped item" : result.message;
            if (result.success) {
                syncPlayerHealth();
            }
            logInfo(hudMessage);
            break;
        }
        case systems::DragActionKind::UnequipToInventory: {
            const systems::EquipmentActionResult result = playerEquipment.unequipToIndex(
                playerInventory, static_cast<systems::EquipmentSlotKind>(action.from), action.to);
            hudMessage = result.message;
            if (result.success) {
                syncPlayerHealth();
            }
            logInfo(hudMessage);
            break;
        }
        case systems::DragActionKind::Unequip: {
            const systems::EquipmentActionResult result = playerEquipment.unequipToInventory(
                playerInventory, static_cast<systems::EquipmentSlotKind>(action.from));
            hudMessage = result.message;
            if (result.success) {
                syncPlayerHealth();
            }
            logInfo(hudMessage);
            break;
        }
        case systems::DragActionKind::None:
            break;
        }
    }

    void handleEquipmentUiClick(float mouseX, float mouseY) {
        if (stateManager.currentState() == gameplay::GameState::TRADING) {
            handleTradeUiClick(mouseX, mouseY);
            return;
        }

        const ui::HitRegion* region = uiInteraction_.hitTest(mouseX, mouseY);
        if (region == nullptr) {
            return;
        }

        if (region->kind == ui::WidgetKind::StatUpgradeButton && region->slotIndex >= 0 &&
            region->slotIndex <= 2) {
            switch (region->slotIndex) {
            case 0:
                trySoulStatUpgrade(systems::SoulStatKind::Strength);
                break;
            case 1:
                trySoulStatUpgrade(systems::SoulStatKind::Dexterity);
                break;
            case 2:
                trySoulStatUpgrade(systems::SoulStatKind::Vitality);
                break;
            default:
                break;
            }
            return;
        }

        if (region->kind == ui::WidgetKind::EquipmentSlot && region->slotIndex >= 0 &&
            region->slotIndex < static_cast<int>(systems::EquipmentSlotKind::Count)) {
            const auto slot = static_cast<systems::EquipmentSlotKind>(region->slotIndex);
            if (!playerEquipment.isSlotOccupied(slot)) {
                return;
            }

            const std::string itemName = playerEquipment.itemAt(slot)->name;
            const systems::EquipmentActionResult result =
                playerEquipment.unequipToInventory(playerInventory, slot);
            if (result.success) {
                syncPlayerHealth();
                hudMessage = "Unequipped " + itemName;
            } else {
                hudMessage = result.message;
            }
            logInfo(hudMessage);
            return;
        }

        if (region->kind == ui::WidgetKind::InventorySlot && region->slotIndex >= 0) {
            if (!playerInventory.isSlotOccupied(region->slotIndex)) {
                return;
            }

            const systems::ItemMetadata& item = *playerInventory.slotAt(region->slotIndex).item;
            systems::ItemMetadata resolved = item;
            systems::applyItemDefinition(resolved);
            if (!systems::Equipment::isEquippableCategory(resolved.category)) {
                hudMessage = "That item cannot be equipped";
                logInfo(hudMessage);
                return;
            }

            const systems::EquipmentActionResult result =
                playerEquipment.equipFromInventory(playerInventory, region->slotIndex);
            hudMessage = result.success ? ("Equipped " + resolved.name) : result.message;
            if (result.success) {
                syncPlayerHealth();
            }
            logInfo(hudMessage);
        }
    }

    [[nodiscard]] bool slotLetterOverlapsTooltip(const ui::Rect& slot) const {
        const float centerX = slot.x + slot.width * 0.5F;
        const float centerY = slot.y + slot.height * 0.5F;
        const auto hits = [&](const ui::Rect& box) {
            return box.width > 0.0F && box.contains(centerX, centerY);
        };
        if (cachedItemTooltip_.has_value() && hits(cachedItemTooltip_->layout.box)) {
            return true;
        }
        if (cachedItemCard_.has_value() && hits(cachedItemCard_->layout.box)) {
            return true;
        }
        return cachedCompareCard_.has_value() && hits(cachedCompareCard_->layout.box);
    }

    void drawEquipmentSlot(
        const ui::Rect& slot,
        const systems::EquipmentSlotKind equipmentSlot,
        bool hovered) const {
        if (playerEquipment.isSlotOccupied(equipmentSlot)) {
            const systems::ItemMetadata& item = *playerEquipment.itemAt(equipmentSlot);
            float glow[4]{};
            float unused[4]{};
            rarityColors(item.rarity, glow, unused);
            glow[3] = hovered ? 0.95F : 0.65F;
            const float glowPad = 2.0F;
            uiRenderer.drawOutlineRect(
                slot.x - glowPad,
                slot.y - glowPad,
                slot.width + glowPad * 2.0F,
                slot.height + glowPad * 2.0F,
                glow);
        }

        drawItemWell(slot, hovered);

        if (!playerEquipment.isSlotOccupied(equipmentSlot)) {
            const char* ghost = equipmentSlotIcon(equipmentSlot);
            if (ghost != nullptr && itemIcons_.isLoaded()) {
                const render::UiFrameUv uv = itemIcons_.uvFor(ghost);
                if (uv.valid) {
                    const float tint[4] = {1.0F, 1.0F, 1.0F, 0.42F};
                    const float pad = 7.0F;
                    uiRenderer.drawTexturedRectUV(
                        itemIcons_.texture(),
                        slot.x + pad,
                        slot.y + pad,
                        std::max(1.0F, slot.width - pad * 2.0F),
                        std::max(1.0F, slot.height - pad * 2.0F),
                        uv.u0,
                        uv.v0,
                        uv.u1,
                        uv.v1,
                        tint);
                }
            }
            return;
        }

        const systems::ItemMetadata& item = *playerEquipment.itemAt(equipmentSlot);
        float iconFill[4]{};
        float iconBorder[4]{};
        rarityColors(item.rarity, iconFill, iconBorder);

        const float iconPad = 4.0F;
        if (!hasItemIcon(item)) {
            uiRenderer.drawFilledRect(
                slot.x + iconPad,
                slot.y + iconPad,
                slot.width - iconPad * 2.0F,
                slot.height - iconPad * 2.0F,
                iconFill);
        }
        uiRenderer.drawOutlineRect(
            slot.x + iconPad,
            slot.y + iconPad,
            slot.width - iconPad * 2.0F,
            slot.height - iconPad * 2.0F,
            iconBorder);
        drawItemIcon(slot, item);
    }

    void drawEquipmentSlotLetter(
        const ui::Rect& slot,
        const systems::EquipmentSlotKind equipmentSlot) const {
        if (slotLetterOverlapsTooltip(slot)) {
            return;
        }

        const ui::UiScale layoutScale = currentUiScale();
        const float letterScale = layoutScale.dim(2.2F);
        const float letterWidthOffset = layoutScale.dim(10.0F);

        if (playerEquipment.isSlotOccupied(equipmentSlot)) {
            const systems::ItemMetadata& item = *playerEquipment.itemAt(equipmentSlot);
            if (hasItemIcon(item)) {
                return;
            }
            const char iconLetter[2] = {item.iconLetter, '\0'};
            const float iconTextColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            const float letterWidth = textRenderer.measureTextWidth(iconLetter, letterScale);
            textRenderer.drawText(
                slot.x + slot.width * 0.5F - letterWidth * 0.5F,
                slot.y + slot.height * 0.5F - letterWidthOffset,
                iconLetter,
                letterScale,
                iconTextColor);
            return;
        }

        const char* ghost = equipmentSlotIcon(equipmentSlot);
        if (ghost != nullptr && itemIcons_.isLoaded() && itemIcons_.uvFor(ghost).valid) {
            return;
        }
        const char slotLetter[2] = {systems::Equipment::slotAbbreviation(equipmentSlot), '\0'};
        const float ghostColor[4] = {0.55F, 0.58F, 0.65F, 0.75F};
        const float letterWidth = textRenderer.measureTextWidth(slotLetter, letterScale);
        textRenderer.drawText(
            slot.x + slot.width * 0.5F - letterWidth * 0.5F,
            slot.y + slot.height * 0.5F - letterWidthOffset,
            slotLetter,
            letterScale,
            ghostColor);
    }

    void grantSouls(const int amount) {
        if (amount <= 0) {
            return;
        }

        ui::CharacterScreenData& stats = overlayState.characterScreen();
        stats.carriedSouls += amount;
    }

    void grantWeaponMasteryFromCombat(const int damageDealt, const int mobXpReward, const bool killed) {
        if (!playerEquipment.isSlotOccupied(systems::EquipmentSlotKind::Weapon)) {
            return;
        }

        int xpGain = 0;
        if (damageDealt > 0) {
            xpGain += systems::weaponMasteryXpForHit(damageDealt);
        }
        if (killed) {
            xpGain += systems::weaponMasteryXpForKill(mobXpReward);
        }
        if (xpGain <= 0) {
            return;
        }

        systems::WeaponMasteryResult masteryResult{};
        const bool updated = playerEquipment.modifySlot(
            systems::EquipmentSlotKind::Weapon,
            [&](systems::ItemMetadata& item) {
                masteryResult = systems::grantWeaponMasteryXp(item, xpGain);
            });
        if (!updated || masteryResult.xpGained <= 0) {
            return;
        }

        if (masteryResult.leveledUp) {
            std::ostringstream message;
            message << "Weapon mastery level " << masteryResult.newLevel << "!";
            hudMessage = message.str();
            logInfo(hudMessage);

            const std::optional<systems::ItemMetadata>& weapon =
                playerEquipment.itemAt(systems::EquipmentSlotKind::Weapon);
            if (weapon.has_value()) {
                spawnFloatingCombatText(
                    playerPosition,
                    weapon->name + " Lv " + std::to_string(masteryResult.newLevel),
                    0.95F,
                    0.82F,
                    0.25F,
                    1.8F,
                    1.4F);
            }
        }
    }

    void spawnFloatingCombatText(
        const glm::vec3& worldPosition,
        const std::string& text,
        const float colorR,
        const float colorG,
        const float colorB,
        const float lifetimeSeconds,
        const float verticalOffset) {
        FloatingCombatText floating{};
        floating.text = text;
        floating.worldPosition = worldPosition + glm::vec3(0.0F, verticalOffset, 0.0F);
        floating.ageSeconds = 0.0F;
        floating.lifetimeSeconds = lifetimeSeconds;
        floating.colorR = colorR;
        floating.colorG = colorG;
        floating.colorB = colorB;

        const int spreadIndex = static_cast<int>(floatingCombatTexts.size() % 7);
        floating.worldPosition.x += static_cast<float>(spreadIndex - 3) * 0.45F;

        floatingCombatTexts.push_back(std::move(floating));
    }

    void spawnDamageNumber(const glm::vec3& worldPosition, const int damage) {
        spawnFloatingCombatText(
            worldPosition,
            std::to_string(damage),
            1.0F,
            0.38F,
            0.28F,
            1.5F,
            2.2F);
    }

    void spawnCriticalDamageNumber(const glm::vec3& worldPosition, const int damage) {
        spawnFloatingCombatText(
            worldPosition,
            std::to_string(damage) + "!",
            1.0F,
            0.85F,
            0.2F,
            1.8F,
            2.6F);
        floatingCombatTexts.back().critical = true;
    }

    void spawnSoulsNumber(const glm::vec3& worldPosition, const int souls) {
        spawnFloatingCombatText(
            worldPosition,
            "+" + std::to_string(souls) + " Souls",
            0.95F,
            0.82F,
            0.18F,
            2.2F,
            2.8F);
    }

    void syncCombatWithScenery() {
        combatSystem.syncScenery(zoneManager.scenery());
    }

    void ensureSceneryCaches() const {
        if (cachedSceneryRevision_ == zoneManager.sceneryRevision()) {
            return;
        }

        cachedSceneryRevision_ = zoneManager.sceneryRevision();
        entityIndexById_.clear();
        const std::vector<gameplay::WorldEntitySnapshot>& scenery = zoneManager.scenery();
        entityIndexById_.reserve(scenery.size());
        for (std::size_t index = 0; index < scenery.size(); ++index) {
            entityIndexById_[scenery[index].id] = index;
        }
    }

    void ensureCombatSynced() {
        ensureSceneryCaches();
        if (lastCombatSyncRevision_ == zoneManager.sceneryRevision()) {
            return;
        }
        syncCombatWithScenery();
        lastCombatSyncRevision_ = zoneManager.sceneryRevision();
    }

    void invalidateSceneryCaches() {
        cachedSceneryRevision_ = 0;
        lastCombatSyncRevision_ = 0xFFFFFFFFU;
    }

    void refreshVisibleGround() {
        const gameplay::CameraMatrices matrices = camera.matricesForTarget(cameraFocus());
        visibleGround_ = gameplay::visibleGroundAabb(
            matrices, window.width(), window.height(), playerPosition, kVisibleGroundMargin);
    }

    [[nodiscard]] bool isInsideVisibleGround(const glm::vec3& worldPosition) const noexcept {
        return visibleGround_.contains(worldPosition.x, worldPosition.z);
    }

    void noteFrameDelta(const float deltaSeconds) noexcept {
        if (deltaSeconds <= 0.0001F) {
            return;
        }
        const float instant = 1.0F / deltaSeconds;
        smoothedFps_ = smoothedFps_ <= 0.0F ? instant : smoothedFps_ * 0.9F + instant * 0.1F;
        std::snprintf(fpsLabel_, sizeof(fpsLabel_), "%.0f fps", smoothedFps_);
    }

    [[nodiscard]] static std::optional<ui::MinimapBlipKind> minimapBlipKind(
        const gameplay::EntityKind kind) noexcept {
        switch (kind) {
        case gameplay::EntityKind::ENEMY_MOB:
            return ui::MinimapBlipKind::Hostile;
        case gameplay::EntityKind::ENEMY_BOSS:
            return ui::MinimapBlipKind::Boss;
        case gameplay::EntityKind::NPC_BLACKSMITH:
            return ui::MinimapBlipKind::Ally;
        case gameplay::EntityKind::ENV_CHEST:
            return ui::MinimapBlipKind::Loot;
        case gameplay::EntityKind::ENV_HOUSE:
            return ui::MinimapBlipKind::Landmark;
        case gameplay::EntityKind::PLAYER:
        case gameplay::EntityKind::ENV_TREE:
        case gameplay::EntityKind::ENV_BUSH:
        case gameplay::EntityKind::ENV_ROCK:
        case gameplay::EntityKind::ENV_MUSHROOM:
            break;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool showMobNameplates() const noexcept {
        return gameSettings.graphicsQuality > 0;
    }

    void syncRunDifficulty() {
        if (laneActive_) {
            applyLanePresentation();
            return;
        }
        const systems::DifficultyModifiers modifiers = runProgression_.modifiers();
        combatSystem.setDifficultyModifiers(modifiers);
        lootEngine.setZoneDepth(runProgression_.depth());
        lootEngine.setLootTierBonus(modifiers.lootTierBonus);
        lootEngine.setLootCeiling(systems::LootCeiling::Unique);
    }

    void refreshPlainsRun() {
        zoneManager.respawnPlainsContent(runProgression_.runSeed(), runProgression_.depth());
        combatSystem.reset();
        mobAnimations_.clear();
        entityFacing8_.clear();
        invalidateSceneryCaches();
        syncRunDifficulty();
        ensureCombatSynced();
    }

    void onPlainsZoneEntered() {
        invalidateSceneryCaches();
        beginLane(selectedNode_);
    }

    void restoreExploreCamera() {
        camera.setFollowOffset(gameplay::kIsometricEyeOffset);
        camera.setOrthoHeight(gameplay::kIsometricOrthoHeight);
        lootEngine.setLootCeiling(systems::LootCeiling::Unique);
    }

    void applyLanePresentation() {
        camera.setFollowOffset(glm::vec3(0.0F, 30.0F, 16.0F));
        camera.setOrthoHeight(kLaneOrthoHeight);
        const gameplay::BattleNode& node = lane_.node();
        systems::LootCeiling ceiling = systems::LootCeiling::Magic;
        if (node.ceiling == gameplay::LootCeiling::Legendary) {
            ceiling = systems::LootCeiling::Legendary;
        } else if (node.ceiling == gameplay::LootCeiling::Unique) {
            ceiling = systems::LootCeiling::Unique;
        }
        lootEngine.setLootCeiling(ceiling);
        lootEngine.setZoneDepth(std::max(1, node.enemyLevel));
        systems::DifficultyModifiers mods{};
        mods.mobHpMultiplier = systems::difficultyTierModifiers(runProgression_.tier()).hpMultiplier;
        mods.mobXpMultiplier = 1.0F;
        mods.mobDamageMultiplier = systems::difficultyTierModifiers(runProgression_.tier()).damageMultiplier;
        combatSystem.setDifficultyModifiers(mods);
    }

    void clearPlainsActors() {
        std::vector<std::uint32_t> retire;
        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active) {
                continue;
            }
            const bool foe = isAttackableEntity(entity.kind) || entity.kind == gameplay::EntityKind::ENV_CHEST;
            const bool onRoad = std::abs(entity.position.z - kLaneCenterZ) < 8.0F;
            if (foe || onRoad) {
                retire.push_back(entity.id);
            }
        }
        for (const std::uint32_t id : retire) {
            static_cast<void>(zoneManager.deactivateEntity(id));
        }
        laneMobIds_.clear();
        eliteIds_.clear();
        combatSystem.reset();
        invalidateSceneryCaches();
    }

    void beginLane(const int nodeIndex) {
        selectedNode_ = std::clamp(nodeIndex, 0, gameplay::LaneBattle::kNodeCount - 1);
        clearPlainsActors();
        const std::uint32_t seed = runProgression_.runSeed() ^ static_cast<std::uint32_t>(selectedNode_ * 97 + 3);
        lane_.start(selectedNode_, seed);
        laneActive_ = true;
        nodeMapOpen_ = false;
        tavernPanelOpen_ = false;
        healerPanelOpen_ = false;
        playerPosition = glm::vec3(0.0F, 0.0F, kLaneCenterZ);
        zoneManager.updatePlayerPosition(toVec3(playerPosition));
        playerPosition = toGlm(zoneManager.player().position());
        playerPosition.z = kLaneCenterZ;
        hasMoveTarget = false;
        combatSystem.clearTarget();
        applyLanePresentation();
        lane_.update(0.0F, 0, 999.0F);
        if (lane_.wantsSpawn()) {
            spawnLaneGroup(lane_.consumeSpawn());
        }
        hudMessage = std::string(lane_.node().name) + " — the hero runs right. Packs march in from the right.";
        logInfo(hudMessage);
    }

    void endLane() {
        if (!laneActive_) {
            restoreExploreCamera();
            return;
        }
        laneActive_ = false;
        laneMobIds_.clear();
        eliteIds_.clear();
        restoreExploreCamera();
    }

    void returnToTownFromLane(const char* message) {
        endLane();
        zoneManager.forceRespawnInTown();
        playerPosition = toGlm(zoneManager.player().position());
        stateManager.forceState(gameplay::GameState::TOWN);
        combatSystem.clearTarget();
        hasMoveTarget = false;
        attackCooldownSeconds_ = 0.0F;
        mobAttackCooldowns_.clear();
        nodeMapOpen_ = true;
        hudMessage = message;
        logInfo(message);
    }

    [[nodiscard]] int livingLaneMobs(float& nearestGap) const {
        nearestGap = 999.0F;
        int living = 0;
        for (const std::uint32_t id : laneMobIds_) {
            const gameplay::WorldEntitySnapshot* entity = findEntityById(id);
            if (entity == nullptr || !entity->active || !combatSystem.isMobAlive(id)) {
                continue;
            }
            ++living;
            nearestGap = std::min(nearestGap, entity->position.x - playerPosition.x);
        }
        return living;
    }

    void spawnLaneGroup(const gameplay::LaneSpawnRequest& request) {
        if (request.count <= 0) {
            return;
        }
        const float originX = playerPosition.x + gameplay::LaneBattle::kSpawnLead;
        int elitesLeft = request.eliteCount;
        for (int index = 0; index < request.count; ++index) {
            const bool boss = request.boss && index == 0;
            const bool elite = !boss && elitesLeft > 0;
            if (elite) {
                --elitesLeft;
            }
            const float z = kLaneCenterZ +
                (static_cast<float>(index) - static_cast<float>(request.count - 1) * 0.5F) * 1.55F;
            const gameplay::EntityKind kind =
                boss ? gameplay::EntityKind::ENEMY_BOSS : gameplay::EntityKind::ENEMY_MOB;
            const std::uint32_t id = zoneManager.spawnEntity(kind, originX, z, elite ? 1 : 0);
            laneMobIds_.insert(id);
            if (elite) {
                eliteIds_.insert(id);
            }
            ensureCombatSynced();
            float scale = 0.85F + static_cast<float>(request.enemyLevel) * 0.12F;
            if (elite) {
                scale *= 1.7F;
            }
            if (boss) {
                scale = 0.75F + static_cast<float>(request.enemyLevel) * 0.08F;
            }
            combatSystem.multiplyMobHealth(id, scale);
        }
        invalidateSceneryCaches();
    }

    void retargetLaneFight() {
        float best = std::numeric_limits<float>::max();
        std::optional<std::uint32_t> closest;
        for (const std::uint32_t id : laneMobIds_) {
            const gameplay::WorldEntitySnapshot* entity = findEntityById(id);
            if (entity == nullptr || !entity->active || !combatSystem.isMobAlive(id)) {
                continue;
            }
            const float dx = entity->position.x - playerPosition.x;
            const float dz = entity->position.z - playerPosition.z;
            const float distance = dx * dx + dz * dz;
            if (distance < best) {
                best = distance;
                closest = id;
            }
        }
        if (closest.has_value()) {
            ensureCombatSynced();
            combatSystem.setTarget(*closest);
        }
    }

    void prepareLaneMotion() {
        if (!laneActive_ || zoneManager.activeZone() != gameplay::WorldZone::PLAINS) {
            return;
        }
        if (lane_.phase() == gameplay::LanePhase::Running) {
            combatSystem.clearTarget();
            hasMoveTarget = true;
            moveTarget = glm::vec3(playerPosition.x + 10.0F, 0.0F, kLaneCenterZ);
            return;
        }
        if (lane_.phase() == gameplay::LanePhase::Fighting) {
            if (!combatSystem.hasTarget() || !combatSystem.isMobAlive(*combatSystem.targetId())) {
                retargetLaneFight();
            }
        }
    }

    void advanceLane(const float deltaSeconds) {
        if (!laneActive_) {
            return;
        }
        if (zoneManager.activeZone() != gameplay::WorldZone::PLAINS) {
            endLane();
            return;
        }

        playerPosition.z = kLaneCenterZ;
        if (lane_.phase() == gameplay::LanePhase::Running) {
            for (const std::uint32_t id : laneMobIds_) {
                const gameplay::WorldEntitySnapshot* entity = findEntityById(id);
                if (entity == nullptr || !entity->active || !combatSystem.isMobAlive(id)) {
                    continue;
                }
                if (entity->position.x - playerPosition.x > gameplay::LaneBattle::kEngageGap) {
                    const float pullZ = (kLaneCenterZ - entity->position.z) * std::min(1.0F, deltaSeconds * 3.0F);
                    zoneManager.nudgeEntity(id, -gameplay::LaneBattle::kMobMarchSpeed * deltaSeconds, pullZ);
                }
            }
        }

        if (playerPosition.x > 7.0F) {
            const float shift = playerPosition.x;
            playerPosition.x = 0.0F;
            moveTarget.x -= shift;
            zoneManager.shiftSceneryX(-shift);
        }
        zoneManager.updatePlayerPosition(toVec3(playerPosition));
        playerPosition = toGlm(zoneManager.player().position());
        playerPosition.z = kLaneCenterZ;

        float nearestGap = 999.0F;
        const int living = livingLaneMobs(nearestGap);
        lane_.update(deltaSeconds, living, nearestGap);
        if (lane_.wantsSpawn()) {
            spawnLaneGroup(lane_.consumeSpawn());
        }
        if (lane_.consumeVictory()) {
            runProgression_.noteNodeCleared(lane_.nodeIndex());
            returnToTownFromLane("Road cleared. Wear the upgrades, sell the rest, then pick the next node.");
        }
    }

    [[nodiscard]] ui::Rect campaignNodeRect(const int index) const {
        const float width = static_cast<float>(window.width());
        const float height = static_cast<float>(window.height());
        const float panelWidth = std::min(760.0F, width * 0.78F);
        const float panelHeight = std::min(540.0F, height * 0.68F);
        const float panelX = (width - panelWidth) * 0.5F;
        const float panelY = height * 0.14F;
        const int column = index % 2;
        const int row = index / 2;
        const float gap = 12.0F;
        const float buttonWidth = (panelWidth - 56.0F - gap) * 0.5F;
        const float buttonHeight = (panelHeight - 96.0F) / 4.0F - 8.0F;
        return ui::Rect{
            panelX + 24.0F + static_cast<float>(column) * (buttonWidth + gap),
            panelY + 72.0F + static_cast<float>(row) * (buttonHeight + 8.0F),
            buttonWidth,
            buttonHeight};
    }

    void pickCampaignNode(const double mouseX, const double mouseY) {
        if (zoneManager.activeZone() != gameplay::WorldZone::TOWN) {
            return;
        }
        for (int index = 0; index < gameplay::LaneBattle::kNodeCount; ++index) {
            if (!campaignNodeRect(index).contains(static_cast<float>(mouseX), static_cast<float>(mouseY))) {
                continue;
            }
            if (!gameplay::LaneBattle::nodeUnlocked(index, runProgression_.depth())) {
                hudMessage = "Clear the previous road before this one.";
                logInfo(hudMessage);
                return;
            }
            selectedNode_ = index;
            nodeMapOpen_ = false;
            playerPosition = glm::vec3(0.0F, 0.0F, 40.0F);
            const gameplay::ZoneTransitionResult transition =
                zoneManager.updatePlayerPosition(toVec3(playerPosition));
            playerPosition = toGlm(zoneManager.player().position());
            if (transition.transitioned && transition.toZone == gameplay::WorldZone::PLAINS) {
                closeTransientOverlays();
                stateManager.transitionTo(gameplay::GameState::PLAINS);
                onPlainsZoneEntered();
            }
            return;
        }
    }

    void renderCampaignMap() const {
        if (!nodeMapOpen_ || zoneManager.activeZone() != gameplay::WorldZone::TOWN) {
            return;
        }
        const float width = static_cast<float>(window.width());
        const float height = static_cast<float>(window.height());
        const float panelWidth = std::min(760.0F, width * 0.78F);
        const float panelHeight = std::min(540.0F, height * 0.68F);
        const ui::Rect panel{(width - panelWidth) * 0.5F, height * 0.14F, panelWidth, panelHeight};
        drawStonePlaque(panel);
        const float shade[4] = {0.02F, 0.015F, 0.02F, 0.35F};
        uiRenderer.drawFilledRect(0.0F, 0.0F, width, panel.y, shade);

        for (int index = 0; index < gameplay::LaneBattle::kNodeCount; ++index) {
            const bool unlocked = gameplay::LaneBattle::nodeUnlocked(index, runProgression_.depth());
            const ui::Rect button = campaignNodeRect(index);
            drawRpgButton(button, unlocked && index == selectedNode_, unlocked);
        }
    }

    void renderCampaignMapText() const {
        if (!nodeMapOpen_ || zoneManager.activeZone() != gameplay::WorldZone::TOWN) {
            return;
        }
        const float width = static_cast<float>(window.width());
        const float height = static_cast<float>(window.height());
        const float panelWidth = std::min(760.0F, width * 0.78F);
        const ui::Rect title{(width - panelWidth) * 0.5F, height * 0.14F + 8.0F, panelWidth, 28.0F};
        const float titleColor[4] = {0.95F, 0.86F, 0.55F, 1.0F};
        textRenderer.drawTextCentered(title, "Campaign roads — earlier paths stay open to farm", 1.7F, titleColor);
        const float openColor[4] = {0.94F, 0.9F, 0.78F, 1.0F};
        const float lockedColor[4] = {0.45F, 0.45F, 0.5F, 1.0F};
        const float subColor[4] = {0.75F, 0.68F, 0.48F, 1.0F};
        for (int index = 0; index < gameplay::LaneBattle::kNodeCount; ++index) {
            const gameplay::BattleNode& node = gameplay::LaneBattle::nodeAt(index);
            const bool unlocked = gameplay::LaneBattle::nodeUnlocked(index, runProgression_.depth());
            const ui::Rect button = campaignNodeRect(index);
            const ui::Rect nameRect{button.x + 8.0F, button.y + 6.0F, button.width - 16.0F, button.height * 0.48F};
            const ui::Rect subRect{button.x + 8.0F, button.y + button.height * 0.48F, button.width - 16.0F, button.height * 0.4F};
            textRenderer.drawTextCentered(nameRect, unlocked ? node.name : "Locked", 1.45F, unlocked ? openColor : lockedColor);
            std::ostringstream detail;
            if (unlocked) {
                detail << "Lv " << node.enemyLevel << "  " << lootCeilingLabel(node.ceiling);
                if (node.bossFinale) {
                    detail << "  Boss";
                }
            } else {
                detail << "Clear road " << index;
            }
            const std::string detailText = detail.str();
            textRenderer.drawTextCentered(subRect, detailText.c_str(), 1.2F, subColor);
        }
    }

    void syncPlayerGold(const int gold) {
        zoneManager.player().setGold(gold);
        tradeSystem.setPlayerGold(zoneManager.player().gold());
    }

    void openTownBlacksmith() {
        tavernPanelOpen_ = false;
        healerPanelOpen_ = false;
        nodeMapOpen_ = false;
        tradeSystem.setPlayerGold(zoneManager.player().gold());
        stateManager.enterTrading();
        hudMessage = "Blacksmith — click items to sell, forge services below";
        townNotice_ = hudMessage;
        logInfo(hudMessage);
    }

    [[nodiscard]] static std::vector<std::uint8_t> flipTownImage(const render::TownPixelBuffer& image) {
        std::vector<std::uint8_t> upright(image.rgba.size());
        const int stride = image.width * 4;
        for (int y = 0; y < image.height; ++y) {
            const int src = y * stride;
            const int dst = (image.height - 1 - y) * stride;
            std::copy(image.rgba.begin() + src, image.rgba.begin() + src + stride, upright.begin() + dst);
        }
        return upright;
    }

    void uploadTownPlate(const int index, const render::TownPlateKind kind, const bool restored) {
        const render::TownPixelBuffer image = render::paintTownPlate(
            kind, restored, kind == render::TownPlateKind::Road ? 320 : 160, kind == render::TownPlateKind::Road ? 72 : 210);
        const std::vector<std::uint8_t> upright = flipTownImage(image);
        static_cast<void>(townPlates_[static_cast<std::size_t>(index)].uploadRgba(
            image.width, image.height, upright.data(), true));
    }

    [[nodiscard]] bool loadTownTextures() {
        const std::string dir = joinPath(assetsRoot, "textures/town");
        if (!townBackdrop_.loadFromFile(dir + "/backdrop.png", true)) {
            return false;
        }
        const char* files[7] = {
            "forge_ruined.png",
            "forge_repaired.png",
            "chapel_ruined.png",
            "chapel_repaired.png",
            "tavern_ruined.png",
            "tavern_repaired.png",
            "road.png"};
        for (int index = 0; index < 7; ++index) {
            if (!townPlates_[static_cast<std::size_t>(index)].loadFromFile(dir + "/" + files[index], true)) {
                return false;
            }
        }
        return true;
    }

    void ensureTownBackdrop() {
        if (townBackdropReady_) {
            return;
        }
        if (loadTownTextures()) {
            townBackdropReady_ = true;
            logInfo("Town art loaded from textures/town.");
            return;
        }
        const render::TownPixelBuffer image = render::paintTownBackdrop(480, 270);
        const std::vector<std::uint8_t> upright = flipTownImage(image);
        townBackdropReady_ = townBackdrop_.uploadRgba(image.width, image.height, upright.data(), true);
        uploadTownPlate(0, render::TownPlateKind::Forge, false);
        uploadTownPlate(1, render::TownPlateKind::Forge, true);
        uploadTownPlate(2, render::TownPlateKind::Chapel, false);
        uploadTownPlate(3, render::TownPlateKind::Chapel, true);
        uploadTownPlate(4, render::TownPlateKind::Tavern, false);
        uploadTownPlate(5, render::TownPlateKind::Tavern, true);
        uploadTownPlate(6, render::TownPlateKind::Road, true);
        logInfo("Town art files missing; using the painted fallback.");
    }

    void spinTavern() {
        int gold = tradeSystem.playerGold();
        const systems::TavernGambleResult spin = lootEngine.gambleTavern(gold);
        syncPlayerGold(gold);
        if (spin.item.has_value()) {
            if (!playerInventory.addItem(*spin.item).success) {
                townNotice_ = "Bag full — " + spin.item->name + " was lost.";
            } else {
                townNotice_ = spin.message;
            }
        } else {
            townNotice_ = spin.message;
        }
        hudMessage = townNotice_;
        logInfo(hudMessage);
    }

    void restAtChapel() {
        int gold = tradeSystem.playerGold();
        const int cost = systems::healerTitheGold();
        if (gold < cost) {
            townNotice_ = "The chapel asks " + std::to_string(cost) + " gold.";
            hudMessage = townNotice_;
            return;
        }
        gold -= cost;
        syncPlayerGold(gold);
        playerCurrentHealth_ = std::max(1, effectiveCharacterStats().maxHealth);
        skillBar_.restoreMana();
        townNotice_ = "You rest. Health and mana restored.";
        hudMessage = townNotice_;
        logInfo(hudMessage);
    }

    void handleTownClick(const float mouseX, const float mouseY) {
        const ui::TownSceneLayout layout = ui::computeTownSceneLayout(currentUiScale());
        if (tavernPanelOpen_ || healerPanelOpen_) {
            if (layout.serviceClose.contains(mouseX, mouseY)) {
                tavernPanelOpen_ = false;
                healerPanelOpen_ = false;
                return;
            }
            if (layout.serviceAction.contains(mouseX, mouseY)) {
                if (tavernPanelOpen_) {
                    spinTavern();
                } else {
                    restAtChapel();
                }
                return;
            }
            if (layout.servicePanel.contains(mouseX, mouseY)) {
                return;
            }
            tavernPanelOpen_ = false;
            healerPanelOpen_ = false;
        }

        if (layout.road.contains(mouseX, mouseY)) {
            overlayState.showInventoryOverlay(false);
            overlayState.showCharacterScreen(false);
            if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                stateManager.closeCharacterMenu();
            }
            tavernPanelOpen_ = false;
            healerPanelOpen_ = false;
            nodeMapOpen_ = true;
            townNotice_ = "Choose a road. Gold and levels repair the town.";
            hudMessage = townNotice_;
            return;
        }

        const ui::Rect hotspots[] = {layout.blacksmith, layout.tavern, layout.healer};
        const systems::TownBuilding buildings[] = {
            systems::TownBuilding::Blacksmith,
            systems::TownBuilding::Tavern,
            systems::TownBuilding::Healer};
        for (int index = 0; index < 3; ++index) {
            if (!hotspots[index].contains(mouseX, mouseY)) {
                continue;
            }
            const systems::TownBuilding building = buildings[index];
            if (!townHub_.isRepaired(building)) {
                int gold = tradeSystem.playerGold();
                const systems::TownRepairResult result =
                    townHub_.tryRepair(building, gold, overlayState.characterScreen().level);
                syncPlayerGold(gold);
                townNotice_ = systems::TownHub::repairMessage(building, result);
                hudMessage = townNotice_;
                logInfo(hudMessage);
                return;
            }
            if (building == systems::TownBuilding::Blacksmith) {
                openTownBlacksmith();
                return;
            }
            if (building == systems::TownBuilding::Tavern) {
                healerPanelOpen_ = false;
                tavernPanelOpen_ = true;
                townNotice_ = "Tavern — 25 gold a spin. Mythical prizes are 1 in 10,000.";
                hudMessage = townNotice_;
                return;
            }
            tavernPanelOpen_ = false;
            healerPanelOpen_ = true;
            townNotice_ = "Chapel — rest for a small tithe.";
            hudMessage = townNotice_;
            return;
        }
    }

    void updateTownHover(const float mouseX, const float mouseY) {
        hoveredTownHotspot_ = -1;
        if (laneActive_ || zoneManager.allowsFreeMovement() || nodeMapOpen_ || stateManager.isPausedForUi()) {
            return;
        }
        const ui::TownSceneLayout layout = ui::computeTownSceneLayout(currentUiScale());
        if ((tavernPanelOpen_ || healerPanelOpen_) && layout.servicePanel.contains(mouseX, mouseY)) {
            return;
        }
        hoveredTownHotspot_ = ui::townHotspotIndexAt(layout, mouseX, mouseY);
    }

    void drawTownHoverOutline(const ui::Rect& rect) const {
        const float pad = 4.0F;
        const float gold[4] = {1.0F, 0.84F, 0.32F, 1.0F};
        uiRenderer.drawOutlineRect(
            rect.x - pad, rect.y - pad, rect.width + pad * 2.0F, rect.height + pad * 2.0F, gold, 3.0F);
    }

    void drawReadableCentered(const ui::Rect& bounds, const char* text, float scale, const float color[4]) const {
        if (text == nullptr || text[0] == '\0') {
            return;
        }
        const float measured = textRenderer.measureTextWidth(text, scale);
        const float limit = std::max(8.0F, bounds.width - 12.0F);
        if (measured > limit && measured > 1.0F) {
            scale *= limit / measured;
        }
        const float shadow[4] = {0.02F, 0.01F, 0.0F, 0.95F};
        const float kick = std::max(1.25F, scale * 0.5F);
        const float offsets[4][2] = {{-kick, 0.0F}, {kick, 0.0F}, {0.0F, -kick}, {0.0F, kick}};
        for (const auto& offset : offsets) {
            const ui::Rect shifted{bounds.x + offset[0], bounds.y + offset[1], bounds.width, bounds.height};
            textRenderer.drawTextCentered(shifted, text, scale, shadow);
        }
        textRenderer.drawTextCentered(bounds, text, scale, color);
    }

    void renderTownScene() {
        if (laneActive_ || zoneManager.allowsFreeMovement()) {
            return;
        }

        ensureTownBackdrop();
        const float width = static_cast<float>(window.width());
        const float height = static_cast<float>(window.height());
        if (townBackdrop_.isValid()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawTexturedRect(townBackdrop_, 0.0F, 0.0F, width, height, white);
        } else {
            const float sky[4] = {0.07F, 0.06F, 0.1F, 1.0F};
            uiRenderer.drawFilledRect(0.0F, 0.0F, width, height, sky);
        }

        const ui::TownSceneLayout layout = ui::computeTownSceneLayout(currentUiScale());
        if (!nodeMapOpen_) {
            const float plate[4] = {0.07F, 0.04F, 0.02F, 0.92F};
            uiRenderer.drawFilledRect(layout.notice.x, layout.notice.y, layout.notice.width, layout.notice.height, plate);
            drawRpgFrame(layout.notice, 3.0F);
        }
        const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        const auto paintBuilding = [&](const ui::Rect& rect, const systems::TownBuilding building, const int ruinedPlate, const int openPlate, const int hotspotIndex) {
            const bool repaired = townHub_.isRepaired(building);
            const ui::Rect art = ui::townBuildingArtRect(rect);
            if (repaired) {
                const float glow[4] = {1.0F, 0.72F, 0.28F, 0.22F};
                uiRenderer.drawFilledCircle(
                    art.x + art.width * 0.5F, art.y + art.height * 0.55F, std::min(art.width, art.height) * 0.18F, glow, 22);
            }
            const render::Texture& plate = townPlates_[static_cast<std::size_t>(repaired ? openPlate : ruinedPlate)];
            if (plate.isValid()) {
                uiRenderer.drawTexturedRect(plate, art.x, art.y, art.width, art.height, white);
            }
            if (hoveredTownHotspot_ == hotspotIndex) {
                drawTownHoverOutline(art);
            }
            const ui::Rect caption = ui::townBuildingCaptionRect(rect);
            const float banner[4] = {0.08F, 0.045F, 0.02F, 0.94F};
            uiRenderer.drawFilledRect(caption.x, caption.y, caption.width, caption.height, banner);
            drawRpgFrame(caption, 4.0F);
        };

        paintBuilding(layout.blacksmith, systems::TownBuilding::Blacksmith, 0, 1, 0);
        paintBuilding(layout.healer, systems::TownBuilding::Healer, 2, 3, 2);
        paintBuilding(layout.tavern, systems::TownBuilding::Tavern, 4, 5, 1);

        if (townPlates_[6].isValid()) {
            uiRenderer.drawTexturedRect(
                townPlates_[6], layout.road.x, layout.road.y, layout.road.width, layout.road.height, white);
        }
        if (hoveredTownHotspot_ == 3) {
            drawTownHoverOutline(layout.road);
        }

        if (tavernPanelOpen_ || healerPanelOpen_) {
            drawRpgPanel(layout.servicePanel);
            drawRpgButton(layout.serviceAction, false, true);
            drawRpgButton(layout.serviceClose, false, true);
        }
    }

    void renderTownSceneText() const {
        if (laneActive_ || zoneManager.allowsFreeMovement() || nodeMapOpen_) {
            return;
        }

        const ui::TownSceneLayout layout = ui::computeTownSceneLayout(currentUiScale());
        const float title[4] = {1.0F, 0.95F, 0.78F, 1.0F};
        const float sub[4] = {0.98F, 0.9F, 0.68F, 1.0F};
        const auto labelBuilding = [&](const ui::Rect& rect, const systems::TownBuilding building) {
            const systems::TownBuildingDefinition definition = systems::townBuildingDefinition(building);
            const bool repaired = townHub_.isRepaired(building);
            const ui::Rect caption = ui::townBuildingCaptionRect(rect);
            const ui::Rect nameRect{caption.x + 8.0F, caption.y + 3.0F, caption.width - 16.0F, caption.height * 0.48F};
            const ui::Rect subRect{
                caption.x + 8.0F, nameRect.y + nameRect.height, caption.width - 16.0F, caption.height * 0.46F};
            drawReadableCentered(nameRect, repaired ? definition.name : definition.ruinedName, 2.35F, title);
            std::string detail = repaired ? definition.serviceHint
                                          : ("Repair " + std::to_string(definition.repairGold) + "g  Lv " +
                                             std::to_string(definition.requiredLevel));
            drawReadableCentered(subRect, detail.c_str(), 1.9F, sub);
        };
        labelBuilding(layout.blacksmith, systems::TownBuilding::Blacksmith);
        labelBuilding(layout.healer, systems::TownBuilding::Healer);
        labelBuilding(layout.tavern, systems::TownBuilding::Tavern);
        drawReadableCentered(layout.road, "The Road", 2.15F, title);

        if (tavernPanelOpen_ || healerPanelOpen_) {
            const float body[4] = {0.98F, 0.92F, 0.78F, 1.0F};
            const char* titleText = tavernPanelOpen_ ? "Tavern gamble" : "Chapel";
            drawReadableCentered(layout.serviceTitle, titleText, 2.15F, title);
            const char* bodyText = tavernPanelOpen_
                ? "Pay 25 gold. Most spins pay coin or common scraps. Mythical is 1 in 10,000."
                : "Pay a small tithe to restore health and mana.";
            drawReadableCentered(layout.serviceBody, bodyText, 1.7F, body);
            drawReadableCentered(layout.serviceAction, tavernPanelOpen_ ? "Spin" : "Rest", 1.9F, title);
            drawReadableCentered(layout.serviceClose, "Close", 1.75F, sub);
        }
    }

    void renderLaneBanner() const {
        const float width = static_cast<float>(window.width());
        if (laneActive_) {
            const ui::Rect banner{width * 0.5F - 280.0F, 4.0F, 560.0F, 28.0F};
            const float color[4] = {1.0F, 0.95F, 0.78F, 1.0F};
            drawReadableCentered(banner, lane_.status(), 2.0F, color);
            return;
        }
        if (zoneManager.activeZone() == gameplay::WorldZone::TOWN && !nodeMapOpen_ && !laneActive_) {
            const ui::TownSceneLayout layout = ui::computeTownSceneLayout(currentUiScale());
            const float color[4] = {1.0F, 0.96F, 0.8F, 1.0F};
            const char* line = townNotice_.empty()
                ? "Click a ruin to repair it with gold. The road leaves town."
                : townNotice_.c_str();
            drawReadableCentered(layout.notice, line, 2.2F, color);
        }
    }

    [[nodiscard]] bool hasItemIcon(const systems::ItemMetadata& item) const noexcept {
        return itemIcons_.isLoaded() && itemIcons_.uvFor(itemIconFrame(item.category)).valid;
    }

    void drawItemIcon(const ui::Rect& slot, const systems::ItemMetadata& item) const {
        if (!hasItemIcon(item)) {
            return;
        }
        const render::UiFrameUv uv = itemIcons_.uvFor(itemIconFrame(item.category));
        const float tint[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        const float pad = 3.0F;
        uiRenderer.drawTexturedRectUV(
            itemIcons_.texture(),
            slot.x + pad,
            slot.y + pad,
            std::max(1.0F, slot.width - pad * 2.0F),
            std::max(1.0F, slot.height - pad * 2.0F),
            uv.u0,
            uv.v0,
            uv.u1,
            uv.v1,
            tint);
    }

    [[nodiscard]] static float lootIntensityForRank(const int rank) noexcept {
        switch (rank) {
        case 5:
            return 2.8F;
        case 4:
            return 2.4F;
        case 3:
            return 2.2F;
        case 2:
            return 1.45F;
        case 1:
            return 1.0F;
        default:
            return 0.55F;
        }
    }

    void noteLootLabel(const std::string& name, const systems::ItemRarity rarity) {
        const systems::RarityColor tint = systems::rarityColor(rarity);
        PendingLootLabel label{};
        label.name = name;
        label.red = tint.red;
        label.green = tint.green;
        label.blue = tint.blue;
        label.rank = systems::rarityRank(rarity);
        label.intensity = lootIntensityForRank(label.rank);
        pendingLootLabels_.push_back(std::move(label));
    }

    void flushLootBeacon(const glm::vec3& worldPosition) {
        if (pendingLootLabels_.empty()) {
            return;
        }

        float intensity = 0.45F;
        std::vector<ui::LootLabel> labels;
        labels.reserve(pendingLootLabels_.size());
        for (const PendingLootLabel& pending : pendingLootLabels_) {
            intensity = std::max(intensity, pending.intensity);
            labels.push_back(ui::LootLabel{pending.name, pending.red, pending.green, pending.blue, pending.rank});
        }
        const int pile = static_cast<int>(lootPresentation_.beacons().size());
        const float angle = static_cast<float>(pile) * 0.9F;
        const float radius = 1.15F + static_cast<float>(pile % 5) * 0.55F;
        const glm::vec3 piled =
            worldPosition + glm::vec3(std::cos(angle) * radius, 0.0F, std::sin(angle) * radius);
        lootPresentation_.spawn(piled.x, piled.y, piled.z, std::move(labels), intensity);
        if (intensity >= 1.6F && !lootPresentation_.beacons().empty()) {
            const ui::LootLabel& brightest = lootPresentation_.beacons().back().labels.front();
            particles_.spawnSpellFlash(
                piled, glm::vec4(brightest.red, brightest.green, brightest.blue, 1.0F));
        }
        pendingLootLabels_.clear();
    }

    void awardLootPrize(const systems::LootPrize& prize, const glm::vec3& worldPosition, std::ostringstream& summary) {
        if (prize.kind == systems::LootPrizeKind::Gold) {
            zoneManager.player().addGold(prize.goldAmount);
            tradeSystem.setPlayerGold(zoneManager.player().gold());
            spawnFloatingCombatText(
                worldPosition, "+" + std::to_string(prize.goldAmount) + " gold", 1.0F, 0.86F, 0.28F, 1.8F, 2.2F);
            summary << "+" << prize.goldAmount << "g ";
            return;
        }
        if (!prize.item.has_value()) {
            return;
        }

        const systems::RarityColor tint = systems::rarityColor(prize.item->rarity);
        const systems::InventoryAddResult added = playerInventory.addItem(*prize.item);
        if (!added.success) {
            summary << "[bag full: " << prize.item->name << " lost] ";
            logInfo("Inventory full — " + prize.item->name + " lost!");
            noteLootLabel(prize.item->name + " (full)", prize.item->rarity);
            return;
        }

        noteLootLabel(prize.item->name, prize.item->rarity);
        const float pillarIntensity = lootIntensityForRank(systems::rarityRank(prize.item->rarity));
        particles_.spawnLootPillar(worldPosition, glm::vec4(tint.red, tint.green, tint.blue, 1.0F), pillarIntensity);
        summary << prize.item->name << " ";
    }

    /// One coin-in / reel-spin cycle. Returns true when anything was paid out.
    bool processLootDrop(const systems::EntityTier tier, const glm::vec3& worldPosition) {
        const systems::LootSpinResult spin = lootEngine.spin(tier);
        if (!spin.spun || spin.tier == systems::LootReelTier::Nothing) {
            return false;
        }

        std::ostringstream summary;
        if (spin.jackpot) {
            // Jackpot feedback: light pillar, screen flash, camera kick, log + audio placeholder.
            combatFeedback_.addFlash(glm::vec3(1.0F, 0.92F, 0.6F), 0.55F, 0.6F);
            combatFeedback_.addTrauma(0.5F);
            combatFeedback_.addHitStop(0.12F, 0.1F);
            particles_.spawnLootPillar(worldPosition, glm::vec4(1.0F, 0.95F, 0.7F, 1.0F), 3.0F);
            spawnFloatingCombatText(worldPosition, "JACKPOT!", 1.0F, 0.9F, 0.3F, 2.6F, 3.4F);
            floatingCombatTexts.back().critical = true;
            logInfo("[audio] play jackpot_fanfare.wav");
            summary << "JACKPOT! ";
        } else if (spin.tier == systems::LootReelTier::Medium) {
            logInfo("[audio] play loot_chime.wav");
        }

        for (const systems::LootPrize& prize : spin.prizes) {
            awardLootPrize(prize, worldPosition, summary);
        }
        flushLootBeacon(worldPosition);

        hudMessage = summary.str();
        std::ostringstream logLine;
        logLine << "Loot [" << systems::lootReelTierLabel(spin.tier) << "] coins " << spin.coinPoolBefore << "->"
                << spin.coinPoolAfter << " pity " << spin.pityCounterAfter << ": " << hudMessage;
        logInfo(logLine.str());
        return true;
    }

    void tryInteractWithProp(const gameplay::WorldEntitySnapshot& entity) {
        if (!entity.active) {
            return;
        }

        const glm::vec3 worldPosition = toGlm(entity.position);
        if (entity.kind == gameplay::EntityKind::ENV_CHEST) {
            lootEngine.insertCoins(systems::ActionType::CHEST_OPEN);
            processLootDrop(systems::EntityTier::Standard, worldPosition);
            if (!zoneManager.deactivateEntity(entity.id)) {
                logInfo("Warning: failed to remove opened chest from world.");
            }
            return;
        }

        if (entity.kind == gameplay::EntityKind::ENV_ROCK) {
            lootEngine.insertCoins(systems::ActionType::ROCK_CLICK);
            processLootDrop(systems::EntityTier::Minor, worldPosition);
            if (!zoneManager.deactivateEntity(entity.id)) {
                logInfo("Warning: failed to remove mined rock from world.");
            }
        }
    }

    void spawnLootNumber(const glm::vec3& worldPosition, const std::string& itemName) {
        spawnFloatingCombatText(
            worldPosition,
            itemName,
            0.95F,
            0.85F,
            0.35F,
            2.4F,
            2.4F);
    }

    void updateFloatingCombatTexts(float deltaSeconds) {
        for (FloatingCombatText& floating : floatingCombatTexts) {
            floating.ageSeconds += deltaSeconds;
            floating.worldPosition.y += deltaSeconds * 1.4F;
        }

        floatingCombatTexts.erase(
            std::remove_if(
                floatingCombatTexts.begin(),
                floatingCombatTexts.end(),
                [](const FloatingCombatText& floating) {
                    return floating.ageSeconds >= floating.lifetimeSeconds;
                }),
            floatingCombatTexts.end());
    }

    void updateCombat(float deltaSeconds) {
        ensureCombatSynced();
        updateFloatingCombatTexts(deltaSeconds);

        if (!combatSystem.hasTarget() || gamePaused) {
            attackCooldownSeconds_ = 0.0F;
            return;
        }

        const std::uint32_t targetId = *combatSystem.targetId();
        const gameplay::WorldEntitySnapshot* target = findEntityById(targetId);
        if (target == nullptr || !target->active || !combatSystem.isMobAlive(targetId)) {
            combatSystem.clearTarget();
            return;
        }

        if (!zoneManager.player().attacksEnabled()) {
            combatSystem.clearTarget();
            return;
        }

        const glm::vec3 targetPosition = toGlm(target->position);
        glm::vec3 toTarget = targetPosition - playerPosition;
        toTarget.y = 0.0F;
        const float distance = glm::length(toTarget);

        if (distance > kMeleeAttackRange) {
            moveTarget = targetPosition;
            hasMoveTarget = true;
            return;
        }

        hasMoveTarget = false;
        if (distance > 0.05F) {
            playerYaw = std::atan2(toTarget.x, toTarget.z);
        }

        attackCooldownSeconds_ -= deltaSeconds;
        if (attackCooldownSeconds_ > 0.0F) {
            return;
        }

        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        attackCooldownSeconds_ = 1.0F / std::max(effective.attacksPerSecond, 0.1F);
        playerAttackAnimTime_ = kPlayerAttackAnimDuration;
        playerAnim_.triggerAttack(std::min(kPlayerAttackAnimDuration, attackCooldownSeconds_ * 0.85F));

        const bool critical = rollCriticalHit(effective);
        const int damage = critical ? systems::applyCriticalDamage(effective.damage) : effective.damage;
        applyPlayerHitToMob(targetId, *target, damage, critical, nullptr);
    }

    [[nodiscard]] bool rollCriticalHit(const systems::EffectiveCharacterStats& effective) {
        std::uniform_real_distribution<float> unit(0.0F, 1.0F);
        return systems::isCriticalRoll(unit(combatRng_), systems::computeCriticalChance(effective.dexterity));
    }

    /// Applies `damage` to a mob and runs every downstream effect (numbers, particles, shake,
    /// souls, loot, depth progression). Returns true when the mob died. Shared by melee and skills.
    bool applyPlayerHitToMob(
        const std::uint32_t targetId,
        const gameplay::WorldEntitySnapshot& target,
        const int damage,
        const bool critical,
        const char* skillName) {
        const glm::vec3 targetPosition = toGlm(target.position);
        const std::optional<DamageResult> result = combatSystem.applyDamage(targetId, damage);
        if (!result.has_value()) {
            return false;
        }

        if (critical) {
            spawnCriticalDamageNumber(targetPosition, result->damageDealt);
            combatFeedback_.addTrauma(0.42F);
            combatFeedback_.addHitStop(0.08F, 0.12F);
        } else {
            spawnDamageNumber(targetPosition, result->damageDealt);
            combatFeedback_.addTrauma(skillName != nullptr ? 0.22F : 0.12F);
        }
        particles_.spawnHitSparks(targetPosition, critical);
        triggerMobHitAnimation(targetId);
        grantWeaponMasteryFromCombat(result->damageDealt, 0, false);

        std::ostringstream hitMessage;
        if (skillName != nullptr) {
            hitMessage << skillName << ' ';
        }
        hitMessage << (eliteIds_.count(targetId) != 0U ? "Elite " : "")
                   << (critical ? "CRIT " : "Hit ") << "for " << result->damageDealt << " ("
                   << result->remainingHp << " HP left)";
        logInfo(hitMessage.str());

        if (!result->killed) {
            return false;
        }

        grantWeaponMasteryFromCombat(0, result->xpReward, true);
        spawnDyingMob(target);

        if (!zoneManager.deactivateEntity(targetId)) {
            logInfo("Warning: failed to remove defeated mob from world.");
        }
        if (combatSystem.hasTarget() && *combatSystem.targetId() == targetId) {
            combatSystem.clearTarget();
        }

        const bool isBoss = target.kind == gameplay::EntityKind::ENEMY_BOSS;
        const bool isElite = eliteIds_.count(targetId) != 0U;
        laneMobIds_.erase(targetId);
        eliteIds_.erase(targetId);
        particles_.spawnDeathBurst(
            targetPosition,
            isBoss ? glm::vec4(0.85F, 0.35F, 1.0F, 0.9F) : glm::vec4(0.7F, 0.08F, 0.1F, 0.85F));
        combatFeedback_.addTrauma(isBoss ? 0.8F : 0.25F);
        combatFeedback_.addHitStop(isBoss ? 0.2F : 0.06F, isBoss ? 0.08F : 0.2F);

        ui::CharacterScreenData& stats = overlayState.characterScreen();
        const float gainMultiplier = stats.soulGainMultiplier;
        const int soulsAwarded =
            systems::scaleSoulReward(result->xpReward, gainMultiplier);
        grantSouls(soulsAwarded);
        spawnSoulsNumber(targetPosition, soulsAwarded);
        runProgression_.onMobKill();
        const systems::CombatKillReward killReward =
            systems::combatKillReward(isBoss, isElite, runProgression_.depth());
        const int goldBounty = killReward.gold;
        zoneManager.player().addGold(goldBounty);
        tradeSystem.setPlayerGold(zoneManager.player().gold());
        spawnFloatingCombatText(
            targetPosition, "+" + std::to_string(goldBounty) + "g", 0.95F, 0.82F, 0.28F, 1.5F, 2.0F);

        const int experienceAward = killReward.experience;
        const systems::ExperienceGrant experienceGrant =
            systems::grantCombatExperience(stats, experienceAward);
        if (experienceGrant.levelsGained > 0) {
            playerCurrentHealth_ = effectiveCharacterStats().maxHealth;
        }

        if (isBoss) {
            systems::registerBossSoulGain(stats);
            lootEngine.insertCoins(systems::ActionType::BOSS_KILL);
            processLootDrop(systems::EntityTier::Boss, targetPosition);
            combatFeedback_.addFlash(glm::vec3(1.0F, 0.9F, 0.6F), 0.45F, 0.5F);
            if (!laneActive_) {
                runProgression_.onBossDefeated();
                refreshPlainsRun();
            }
            std::ostringstream depthMessage;
            depthMessage << "Boss slain! +" << soulsAwarded << " souls";
            if (experienceGrant.levelsGained > 0) {
                depthMessage << " | Level " << stats.level;
            }
            hudMessage = depthMessage.str();
            logInfo(hudMessage);
            return true;
        }

        systems::registerMobSoulGain(stats);
        lootEngine.insertCoins(systems::ActionType::MOB_KILL);
        processLootDrop(isElite ? systems::EntityTier::Elite : systems::EntityTier::Standard, targetPosition);

        std::ostringstream killMessage;
        killMessage << (isElite ? "Elite defeated (+" : "Mob defeated (+") << soulsAwarded << " souls, +"
                    << experienceAward << " xp)";
        if (experienceGrant.levelsGained > 0) {
            killMessage << " Level " << stats.level << "!";
        }
        hudMessage = killMessage.str();
        logInfo(hudMessage);
        return true;
    }

    void refreshManaPool() {
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        skillBar_.setMaxMana(systems::computeMaxMana(base.level, selectedClass == CharacterClass::MAGE));
        skillBar_.setManaRegenPerSecond(2.5F + static_cast<float>(base.level) * 0.35F);
    }

    [[nodiscard]] glm::vec3 playerForwardDirection() const noexcept {
        return glm::vec3(std::sin(playerYaw), 0.0F, std::cos(playerYaw));
    }

    void triggerSkillAnimation(const systems::SkillAnimation animation) {
        const float duration = kPlayerAttackAnimDuration * 0.7F;
        switch (animation) {
        case systems::SkillAnimation::Attack2:
            playerAnim_.triggerAttack2(duration);
            break;
        case systems::SkillAnimation::Cast:
            playerAnim_.triggerCast(duration);
            break;
        case systems::SkillAnimation::Attack:
            playerAnim_.triggerAttack(duration);
            break;
        case systems::SkillAnimation::None:
            break;
        }
    }

    void castSkillSlot(const int slotIndex) {
        if (!zoneManager.allowsFreeMovement()) {
            hudMessage = "Skills are sealed inside Town";
            logInfo(hudMessage);
            return;
        }

        const systems::SkillCastResult cast = skillBar_.tryCast(slotIndex, combatSystem.hasTarget());
        const systems::SkillDefinition& skill = systems::skillDefinition(cast.skill);
        if (!cast.success) {
            hudMessage = std::string(skill.name) + ": " + (cast.failureReason != nullptr ? cast.failureReason : "failed");
            logInfo(hudMessage);
            return;
        }

        const glm::vec4 skillColor(skill.colorR, skill.colorG, skill.colorB, 1.0F);
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        playerAttackAnimTime_ = kPlayerAttackAnimDuration;
        triggerSkillAnimation(skill.animation);
        logInfo(std::string("[audio] play ") + skill.name);

        switch (cast.skill) {
        case systems::SkillId::PowerStrike: {
            const std::uint32_t targetId = *combatSystem.targetId();
            const gameplay::WorldEntitySnapshot* target = findEntityById(targetId);
            if (target == nullptr || !target->active || !combatSystem.isMobAlive(targetId)) {
                skillBar_.setMana(skillBar_.mana() + cast.manaSpent);
                hudMessage = "Power Strike: target lost";
                return;
            }
            const glm::vec3 delta = toGlm(target->position) - playerPosition;
            if (glm::dot(delta, delta) > (kMeleeAttackRange * 1.6F) * (kMeleeAttackRange * 1.6F)) {
                skillBar_.setMana(skillBar_.mana() + cast.manaSpent);
                hudMessage = "Power Strike: too far";
                return;
            }
            particles_.spawnSpellFlash(playerPosition, skillColor);
            const int damage = systems::applyCriticalDamage(
                static_cast<int>(static_cast<float>(effective.damage) * skill.damageMultiplier));
            applyPlayerHitToMob(targetId, *target, damage, true, skill.name);
            break;
        }
        case systems::SkillId::Whirlwind: {
            particles_.spawnSpellFlash(playerPosition, skillColor);
            particles_.spawnSpellFlash(playerPosition + playerForwardDirection() * 1.5F, skillColor);
            combatFeedback_.addTrauma(0.3F);
            const float radiusSq = skill.areaRadius * skill.areaRadius;
            const int damage = static_cast<int>(static_cast<float>(effective.damage) * skill.damageMultiplier);
            std::vector<std::uint32_t> victims;
            for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
                if (!entity.active || !isAttackableEntity(entity.kind) || !combatSystem.isMobAlive(entity.id)) {
                    continue;
                }
                const glm::vec3 delta = toGlm(entity.position) - playerPosition;
                if (glm::dot(delta, delta) <= radiusSq) {
                    victims.push_back(entity.id);
                }
            }
            int hits = 0;
            for (const std::uint32_t victimId : victims) {
                const gameplay::WorldEntitySnapshot* victim = findEntityById(victimId);
                if (victim == nullptr || !victim->active) {
                    continue;
                }
                const gameplay::WorldEntitySnapshot snapshot = *victim;
                applyPlayerHitToMob(victimId, snapshot, damage, rollCriticalHit(effective), skill.name);
                ++hits;
            }
            if (hits == 0) {
                hudMessage = "Whirlwind hit nothing";
                logInfo(hudMessage);
            }
            break;
        }
        case systems::SkillId::Heal: {
            const int maxHealth = std::max(1, effective.maxHealth);
            const int amount = std::max(1, maxHealth * skill.healPercent / 100);
            const int before = playerCurrentHealth_;
            playerCurrentHealth_ = std::min(maxHealth, playerCurrentHealth_ + amount);
            particles_.spawnSpellFlash(playerPosition, skillColor);
            particles_.spawnLootPillar(playerPosition, skillColor, 0.8F);
            spawnFloatingCombatText(
                playerPosition, "+" + std::to_string(playerCurrentHealth_ - before), 0.4F, 1.0F, 0.5F, 1.4F, 2.4F);
            hudMessage = "Healed";
            break;
        }
        case systems::SkillId::Cleave:
        case systems::SkillId::Slam: {
            particles_.spawnSpellFlash(playerPosition, skillColor);
            combatFeedback_.addTrauma(cast.skill == systems::SkillId::Slam ? 0.35F : 0.22F);
            const glm::vec3 forward = playerForwardDirection();
            const float radiusSq = skill.areaRadius * skill.areaRadius;
            const int damage = static_cast<int>(static_cast<float>(effective.damage) * skill.damageMultiplier);
            std::vector<std::uint32_t> victims;
            for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
                if (!entity.active || !isAttackableEntity(entity.kind) || !combatSystem.isMobAlive(entity.id)) {
                    continue;
                }
                glm::vec3 delta = toGlm(entity.position) - playerPosition;
                delta.y = 0.0F;
                if (glm::dot(delta, delta) > radiusSq) {
                    continue;
                }
                if (cast.skill == systems::SkillId::Cleave && glm::dot(delta, forward) <= 0.25F) {
                    continue;
                }
                victims.push_back(entity.id);
            }
            int hits = 0;
            for (const std::uint32_t victimId : victims) {
                const gameplay::WorldEntitySnapshot* victim = findEntityById(victimId);
                if (victim == nullptr || !victim->active) {
                    continue;
                }
                const gameplay::WorldEntitySnapshot snapshot = *victim;
                applyPlayerHitToMob(victimId, snapshot, damage, rollCriticalHit(effective), skill.name);
                ++hits;
            }
            if (hits == 0) {
                hudMessage = std::string(skill.name) + " hit nothing";
                logInfo(hudMessage);
            }
            break;
        }
        case systems::SkillId::Firebolt: {
            const std::uint32_t targetId = *combatSystem.targetId();
            const gameplay::WorldEntitySnapshot* target = findEntityById(targetId);
            if (target == nullptr || !target->active || !combatSystem.isMobAlive(targetId)) {
                skillBar_.setMana(skillBar_.mana() + cast.manaSpent);
                hudMessage = "Firebolt: target lost";
                return;
            }
            const glm::vec3 delta = toGlm(target->position) - playerPosition;
            if (glm::dot(delta, delta) > (kMeleeAttackRange * 2.4F) * (kMeleeAttackRange * 2.4F)) {
                skillBar_.setMana(skillBar_.mana() + cast.manaSpent);
                hudMessage = "Firebolt: too far";
                return;
            }
            particles_.spawnSpellFlash(toGlm(target->position), skillColor);
            const int base = static_cast<int>(static_cast<float>(effective.damage) * skill.damageMultiplier);
            const bool critical = rollCriticalHit(effective);
            const int damage = critical ? systems::applyCriticalDamage(base) : base;
            applyPlayerHitToMob(targetId, *target, damage, critical, skill.name);
            break;
        }
        case systems::SkillId::Shout: {
            const int maxHealth = std::max(1, effective.maxHealth);
            const int amount = std::max(1, maxHealth * skill.healPercent / 100);
            const int before = playerCurrentHealth_;
            playerCurrentHealth_ = std::min(maxHealth, playerCurrentHealth_ + amount);
            particles_.spawnSpellFlash(playerPosition, skillColor);
            combatFeedback_.addTrauma(0.18F);
            spawnFloatingCombatText(
                playerPosition, "+" + std::to_string(playerCurrentHealth_ - before), 0.95F, 0.8F, 0.3F, 1.4F, 2.4F);
            hudMessage = "Shout";
            break;
        }
        case systems::SkillId::Dash: {
            glm::vec3 direction = playerForwardDirection();
            if (hasMoveTarget) {
                glm::vec3 toTarget = moveTarget - playerPosition;
                toTarget.y = 0.0F;
                if (glm::dot(toTarget, toTarget) > 0.01F) {
                    direction = glm::normalize(toTarget);
                }
            }
            const gameplay::AxisAlignedBounds bounds =
                zoneManager.activeZone() == gameplay::WorldZone::TOWN ? zoneManager.townBounds()
                                                                      : zoneManager.plainsBounds();
            glm::vec3 destination = clampToZone(playerPosition + direction * skill.dashDistance, bounds);
            if (laneActive_) {
                destination.z = kLaneCenterZ;
            }
            particles_.spawnSpellFlash(playerPosition, skillColor);
            playerPosition = destination;
            zoneManager.updatePlayerPosition(toVec3(playerPosition));
            playerPosition = toGlm(zoneManager.player().position());
            hasMoveTarget = false;
            combatFeedback_.addTrauma(0.15F);
            particles_.spawnSpellFlash(playerPosition, skillColor);
            break;
        }
        case systems::SkillId::None:
        default:
            break;
        }
    }

    void useBeltPotion() {
        for (int index = 0; index < playerInventory.capacity(); ++index) {
            const systems::InventorySlot& slot = playerInventory.slotAt(index);
            if (!slot.item.has_value() || slot.item->category != systems::ItemCategory::Consumable) {
                continue;
            }
            const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
            const int maxHealth = std::max(1, effective.maxHealth);
            const int heal = std::max(10, maxHealth * 40 / 100);
            const int before = playerCurrentHealth_;
            playerCurrentHealth_ = std::min(maxHealth, playerCurrentHealth_ + heal);
            skillBar_.setMana(std::min(skillBar_.maxMana(), skillBar_.mana() + skillBar_.maxMana() / 4));
            playerInventory.discardAt(index);
            particles_.spawnSpellFlash(playerPosition, glm::vec4(0.9F, 0.25F, 0.3F, 1.0F));
            spawnFloatingCombatText(
                playerPosition, "+" + std::to_string(playerCurrentHealth_ - before), 0.95F, 0.35F, 0.4F, 1.4F, 2.4F);
            hudMessage = "Quaffed " + slot.item->name;
            logInfo(hudMessage);
            return;
        }
        hudMessage = "No potions in belt";
        logInfo(hudMessage);
    }

    void updateMovementTowardTarget(float deltaSeconds) {
        if (!hasMoveTarget || gamePaused) {
            return;
        }

        glm::vec3 toTarget = moveTarget - playerPosition;
        toTarget.y = 0.0F;
        const float distance = glm::length(toTarget);
        if (distance < 0.25F) {
            hasMoveTarget = false;
            return;
        }

        const glm::vec3 direction = toTarget / distance;
        playerYaw = std::atan2(direction.x, direction.z);
        const float moveSpeed = laneActive_ && lane_.phase() == gameplay::LanePhase::Running ? kLaneRunSpeed : 12.0F;
        playerPosition += direction * std::min(distance, moveSpeed * deltaSeconds);

        const gameplay::ZoneTransitionResult transition =
            zoneManager.updatePlayerPosition(toVec3(playerPosition));
        playerPosition = toGlm(zoneManager.player().position());

        if (transition.transitioned) {
            std::ostringstream message;
            message << "Entered " << zoneLabel(transition.toZone);
            hudMessage = message.str();
            logInfo(hudMessage);
            if (transition.toZone == gameplay::WorldZone::PLAINS) {
                closeTransientOverlays();
                stateManager.transitionTo(gameplay::GameState::PLAINS);
                onPlainsZoneEntered();
            } else {
                closeTransientOverlays();
                stateManager.transitionTo(gameplay::GameState::TOWN);
                combatSystem.clearTarget();
                if (laneActive_) {
                    endLane();
                }
            }
        }

        tradeSystem.setPlayerGold(zoneManager.player().gold());
    }

    void activateHudMenu(const int index) {
        if (index <= 2 && stateManager.currentState() == gameplay::GameState::TRADING) {
            hudMessage = "Close the blacksmith first.";
            return;
        }

        switch (index) {
        case 0:
            if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                stateManager.closeCharacterMenu();
                overlayState.showCharacterScreen(false);
            } else {
                overlayState.showInventoryOverlay(false);
                nodeMapOpen_ = false;
                stateManager.openCharacterMenu();
                overlayState.showCharacterScreen(true);
            }
            break;
        case 1:
            nodeMapOpen_ = false;
            if (overlayState.inventoryOverlay().visible) {
                overlayState.showInventoryOverlay(false);
            } else {
                if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                    stateManager.closeCharacterMenu();
                    overlayState.showCharacterScreen(false);
                }
                overlayState.showInventoryOverlay(true);
            }
            break;
        case 2:
            if (zoneManager.activeZone() != gameplay::WorldZone::TOWN) {
                hudMessage = "The campaign map is in town.";
                break;
            }
            overlayState.showInventoryOverlay(false);
            overlayState.showCharacterScreen(false);
            if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                stateManager.closeCharacterMenu();
            }
            nodeMapOpen_ = !nodeMapOpen_;
            hudMessage = nodeMapOpen_ ? "Choose a road. Earlier roads stay open." : "Campaign map closed.";
            break;
        case 3:
            gamePaused = true;
            pauseSettingsOpen = true;
            break;
        case 4:
            gamePaused = true;
            pauseSettingsOpen = false;
            break;
        default:
            break;
        }
    }

    void closeTransientOverlays() {
        if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
            stateManager.closeCharacterMenu();
        }
        overlayState.showCharacterScreen(false);
        overlayState.showInventoryOverlay(false);
    }

    void handleInGameInput(float deltaSeconds) {
        ensureUiHitRegionsBuilt();
        if (gamePaused) {
            handlePauseMenuInput();
            return;
        }

        if (keyPressed(GLFW_KEY_ESCAPE)) {
            if (tavernPanelOpen_ || healerPanelOpen_) {
                tavernPanelOpen_ = false;
                healerPanelOpen_ = false;
                return;
            }
            if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                stateManager.closeCharacterMenu();
                overlayState.showCharacterScreen(false);
                return;
            }
            if (stateManager.currentState() == gameplay::GameState::TRADING) {
                stateManager.leaveTrading();
                return;
            }
            if (overlayState.inventoryOverlay().visible) {
                overlayState.showInventoryOverlay(false);
                return;
            }
            if (overlayState.characterScreen().visible) {
                overlayState.showCharacterScreen(false);
                return;
            }
            if (nodeMapOpen_) {
                nodeMapOpen_ = false;
                return;
            }

            gamePaused = true;
            pauseSettingsOpen = false;
            logInfo("Game paused.");
            logHelp("Pause menu: Save and Exit (main menu) | Settings | Resume (Esc)");
            return;
        }

        if (keyPressed(GLFW_KEY_C)) {
            if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                stateManager.closeCharacterMenu();
                overlayState.showCharacterScreen(false);
            } else {
                overlayState.showInventoryOverlay(false);
                stateManager.openCharacterMenu();
                overlayState.showCharacterScreen(true);
            }
        }

        if (keyPressed(GLFW_KEY_I)) {
            nodeMapOpen_ = false;
            if (overlayState.inventoryOverlay().visible) {
                overlayState.showInventoryOverlay(false);
            } else {
                if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                    stateManager.closeCharacterMenu();
                    overlayState.showCharacterScreen(false);
                }
                overlayState.showInventoryOverlay(true);
                logInfo("Inventory opened.");
            }
        }

        if (keyPressed(GLFW_KEY_M) && zoneManager.activeZone() == gameplay::WorldZone::TOWN &&
            !stateManager.isPausedForUi()) {
            overlayState.showInventoryOverlay(false);
            overlayState.showCharacterScreen(false);
            nodeMapOpen_ = !nodeMapOpen_;
            hudMessage = nodeMapOpen_ ? "Choose a road. Earlier roads stay open." : "Campaign map closed.";
        }

        if (!stateManager.isPausedForUi()) {
            constexpr int kSkillKeys[ui::HudConsoleLayout::kSkillSlotCount] = {
                GLFW_KEY_1,
                GLFW_KEY_2,
                GLFW_KEY_3,
                GLFW_KEY_4,
                GLFW_KEY_5,
                GLFW_KEY_6,
                GLFW_KEY_7,
                GLFW_KEY_8};
            for (int slot = 0; slot < ui::HudConsoleLayout::kSkillSlotCount; ++slot) {
                if (keyPressed(kSkillKeys[slot])) {
                    castSkillSlot(slot);
                }
            }
            if (keyPressed(GLFW_KEY_Q)) {
                useBeltPotion();
            }
        }

        if (keyPressed(GLFW_KEY_E) && zoneManager.activeZone() == gameplay::WorldZone::TOWN && !laneActive_) {
            if (townHub_.isRepaired(systems::TownBuilding::Blacksmith)) {
                openTownBlacksmith();
            } else {
                hudMessage = "The forge is in ruins. Click it and spend gold to repair.";
                logInfo(hudMessage);
            }
        }

        float mouseX = 0.0F;
        float mouseY = 0.0F;
        readMousePosition(mouseX, mouseY);
        const bool syntheticClick = pendingSyntheticClick_;
        const bool buttonDown = syntheticClick ||
            glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool pressed = syntheticClick || (buttonDown && !mouseWasDown);
        const bool released = syntheticClick || (!buttonDown && mouseWasDown);
        if (syntheticClick) {
            pendingSyntheticClick_ = false;
            mouseWasDown = false;
        } else {
            mouseWasDown = buttonDown;
        }

        if (pressed && isMouseOverInGameUi(mouseX, mouseY)) {
            const ui::HitRegion* menuRegion = uiInteraction_.hitTest(mouseX, mouseY);
            if (menuRegion != nullptr && menuRegion->kind == ui::WidgetKind::HudMenuButton) {
                activateHudMenu(menuRegion->slotIndex);
            } else if (stateManager.currentState() == gameplay::GameState::TRADING) {
                handleEquipmentUiClick(mouseX, mouseY);
            } else {
                const ui::HitRegion* region = menuRegion;
                if (region != nullptr && region->kind == ui::WidgetKind::StatUpgradeButton) {
                    handleEquipmentUiClick(mouseX, mouseY);
                } else if (region != nullptr && region->kind == ui::WidgetKind::InventorySlot &&
                           playerInventory.isSlotOccupied(region->slotIndex)) {
                    inventoryDrag_.begin(systems::DragOrigin::Inventory, region->slotIndex);
                } else if (region != nullptr && region->kind == ui::WidgetKind::EquipmentSlot &&
                           region->slotIndex >= 0 &&
                           playerEquipment.isSlotOccupied(
                               static_cast<systems::EquipmentSlotKind>(region->slotIndex))) {
                    inventoryDrag_.begin(systems::DragOrigin::Equipment, region->slotIndex);
                }
            }
        }
        if (released && inventoryDrag_.active()) {
            applyInventoryDrag(mouseX, mouseY);
        }

        if (stateManager.isPausedForUi()) {
            return;
        }

        if (nodeMapOpen_ && pressed) {
            pickCampaignNode(mouseX, mouseY);
            return;
        }

        if (!laneActive_ && !zoneManager.allowsFreeMovement()) {
            hasMoveTarget = false;
            if (pressed && !isMouseOverInGameUi(mouseX, mouseY)) {
                handleTownClick(mouseX, mouseY);
            }
            return;
        }

        const bool managingGear =
            laneActive_ && (overlayState.inventoryOverlay().visible || overlayState.characterScreen().visible);
        if (managingGear) {
            hasMoveTarget = false;
            return;
        }

        if (!laneActive_ && pressed && !isMouseOverInGameUi(mouseX, mouseY)) {
            const gameplay::CameraMatrices cameraMatrices =
                camera.matricesForTarget(cameraFocus());
            const ScreenRay ray = buildScreenRay(
                mouseX, mouseY, window.width(), window.height(), cameraMatrices);

            const std::optional<std::uint32_t> pickedId =
                pickInteractableEntity(ray, zoneManager.scenery());
            if (pickedId.has_value()) {
                const gameplay::WorldEntitySnapshot* picked = findEntityById(*pickedId);
                if (picked != nullptr && picked->active) {
                    if (picked->kind == gameplay::EntityKind::ENV_CHEST ||
                        picked->kind == gameplay::EntityKind::ENV_ROCK) {
                        tryInteractWithProp(*picked);
                        return;
                    }

                    if (zoneManager.player().attacksEnabled() && isAttackableEntity(picked->kind) &&
                        combatSystem.isMobAlive(*pickedId)) {
                        ensureCombatSynced();
                        combatSystem.setTarget(*pickedId);
                        attackCooldownSeconds_ = 0.0F;
                        moveTarget = toGlm(picked->position);
                        hasMoveTarget = true;
                        std::ostringstream message;
                        message << "Attacking " << interactableHint(picked->kind);
                        logInfo(message.str());
                        updateMovementTowardTarget(deltaSeconds);
                        updateCombat(deltaSeconds);
                        return;
                    }
                }
            }

            combatSystem.clearTarget();
            attackCooldownSeconds_ = 0.0F;

            glm::vec3 groundPoint{};
            if (gameplay::screenPointToGround(
                    mouseX,
                    mouseY,
                    window.width(),
                    window.height(),
                    cameraMatrices,
                    groundPoint)) {
                const gameplay::AxisAlignedBounds bounds =
                    zoneManager.activeZone() == gameplay::WorldZone::TOWN ? zoneManager.townBounds()
                                                                          : zoneManager.plainsBounds();
                moveTarget = clampToZone(groundPoint, bounds);
                hasMoveTarget = true;
                std::ostringstream message;
                message << "Move target set (" << moveTarget.x << ", " << moveTarget.z << ")";
                logInfo(message.str());
            }
        }

        if (laneActive_) {
            prepareLaneMotion();
        }
        updateMovementTowardTarget(deltaSeconds);
        updateCombat(deltaSeconds);
        if (laneActive_) {
            advanceLane(deltaSeconds);
        }
    }

    void drawStonePlaque(const ui::Rect& bounds) const {
        drawRpgPanel(bounds);
    }

    void drawMenuAtmosphere() const {
        const float width = static_cast<float>(window.width());
        const float height = static_cast<float>(window.height());
        constexpr int kVignetteSteps = 8;
        const float band = currentUiScale().dim(150.0F);
        const float slice = band / static_cast<float>(kVignetteSteps);
        for (int step = 0; step < kVignetteSteps; ++step) {
            const float fade = 1.0F - static_cast<float>(step) / static_cast<float>(kVignetteSteps);
            const float vignette[4] = {0.0F, 0.0F, 0.0F, 0.42F * fade * fade};
            const float offset = static_cast<float>(step) * slice;
            uiRenderer.drawFilledRect(0.0F, offset, width, slice, vignette);
            uiRenderer.drawFilledRect(0.0F, height - offset - slice, width, slice, vignette);
            uiRenderer.drawFilledRect(offset, 0.0F, slice * 0.65F, height, vignette);
            uiRenderer.drawFilledRect(width - offset - slice * 0.65F, 0.0F, slice * 0.65F, height, vignette);
        }

        const float time = spriteAnimTime_;
        for (int index = 0; index < 24; ++index) {
            const float seed = static_cast<float>(index) * 1.37F;
            const float travel = std::fmod(time * (18.0F + seed) + seed * 53.0F, height + 30.0F);
            const float x = std::fmod(seed * 113.0F + std::sin(time * 0.4F + seed) * 28.0F, width);
            const float y = height - travel;
            const float radius = currentUiScale().dim(1.4F + std::fmod(seed, 2.0F));
            const float ember[4] = {0.9F, 0.42F + std::fmod(seed, 0.2F), 0.1F, 0.4F};
            uiRenderer.drawFilledCircle(x, y, radius, ember, 8);
        }
    }

    void renderMenuBackdrop() {
        advanceSpriteAnimClock();
        applyPointerCursor(PointerCursor::Default);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.03F, 0.025F, 0.028F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        uiRenderer.beginFrame();
        const float width = static_cast<float>(window.width());
        const float height = static_cast<float>(window.height());
        if (menuBackdrop_.isValid()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawTexturedRect(menuBackdrop_, 0.0F, 0.0F, width, height, white);
            const float veil[4] = {0.0F, 0.0F, 0.0F, 0.28F};
            uiRenderer.drawFilledRect(0.0F, 0.0F, width, height, veil);
        } else {
            const float backdrop[4] = {0.04F, 0.03F, 0.035F, 1.0F};
            uiRenderer.drawFilledRect(0.0F, 0.0F, width, height, backdrop);
        }
        drawMenuAtmosphere();
    }

    void renderMainMenu() {
        renderMenuBackdrop();

        const std::vector<MenuButton> buttons = buildMainMenuButtons();
        const float stone[4] = {0.12F, 0.09F, 0.07F, 0.96F};
        const float stoneHover[4] = {0.24F, 0.16F, 0.08F, 1.0F};
        if (!buttons.empty()) {
            const ui::Rect& first = buttons.front().bounds;
            const ui::Rect& last = buttons.back().bounds;
            const float pad = currentUiScale().dim(28.0F);
            drawStonePlaque(
                {first.x - pad,
                 first.y - pad,
                 first.width + pad * 2.0F,
                 (last.y + last.height) - first.y + pad * 2.0F});
        }

        for (const MenuButton& button : buttons) {
            drawMenuButtonBackground(button, stone, stoneHover);
        }

        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        drawTitle("Game Engine RPG", 72.0F, 3.2F);
        for (const MenuButton& button : buttons) {
            drawMenuButtonLabel(button.bounds, button.label, hoveredButtonId == button.id);
        }
        textRenderer.endOverlay();
        glEnable(GL_DEPTH_TEST);
    }

    void renderCharacterSelect() {
        renderMenuBackdrop();

        const std::vector<MenuButton> classes = buildClassButtons();
        const float warriorBase[4] = {0.7F, 0.22F, 0.18F, 1.0F};
        const float warriorHover[4] = {0.9F, 0.32F, 0.25F, 1.0F};
        const float rangerBase[4] = {0.2F, 0.62F, 0.28F, 1.0F};
        const float rangerHover[4] = {0.3F, 0.82F, 0.38F, 1.0F};
        const float mageBase[4] = {0.42F, 0.25F, 0.75F, 1.0F};
        const float mageHover[4] = {0.55F, 0.35F, 0.95F, 1.0F};

        for (const MenuButton& button : classes) {
            if (button.id == 10) {
                drawMenuButtonBackground(button, warriorBase, warriorHover);
            } else if (button.id == 11) {
                drawMenuButtonBackground(button, rangerBase, rangerHover);
            } else {
                drawMenuButtonBackground(button, mageBase, mageHover);
            }

            const CharacterClass previewClass = classFromButtonId(button.id);
            if (mobAssets.hasClassSheets()) {
                const float spriteW = button.bounds.width * 0.72F;
                const float spriteH = button.bounds.height * 0.62F;
                const float spriteX = button.bounds.x + (button.bounds.width - spriteW) * 0.5F;
                const float spriteY = button.bounds.y + currentUiScale().dim(18.0F);
                drawClassSpriteUi(
                    previewClass, spriteX, spriteY, spriteW, spriteH, render::SpriteClip::Idle);
            }
        }

        const ui::UiScale layout = currentUiScale();
        const MenuButton back = buildBackButton(static_cast<float>(layout.height) * 0.78F);
        const float backBase[4] = {0.25F, 0.27F, 0.32F, 1.0F};
        const float backHover[4] = {0.38F, 0.4F, 0.48F, 1.0F};
        drawMenuButtonBackground(back, backBase, backHover);

        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        drawTitle("Select Your Class", 52.0F, 2.8F);
        for (const MenuButton& button : classes) {
            ui::Rect labelBounds{
                button.bounds.x,
                button.bounds.y + button.bounds.height - currentUiScale().dim(52.0F),
                button.bounds.width,
                currentUiScale().dim(36.0F)};
            drawMenuButtonLabel(labelBounds, button.label);
        }
        drawMenuButtonLabel(back.bounds, back.label);
        textRenderer.endOverlay();
        glEnable(GL_DEPTH_TEST);
    }

    void renderLoadCharacterScreen() {
        if (!loadPreviewSnapshot_.has_value()) {
            return;
        }

        const SaveGameSnapshot& snapshot = *loadPreviewSnapshot_;
        renderMenuBackdrop();

        const std::vector<MenuButton> buttons = buildLoadCharacterButtons();
        const MenuButton& card = buttons.front();

        const float warriorBase[4] = {0.7F, 0.22F, 0.18F, 1.0F};
        const float warriorHover[4] = {0.9F, 0.32F, 0.25F, 1.0F};
        const float rangerBase[4] = {0.2F, 0.62F, 0.28F, 1.0F};
        const float rangerHover[4] = {0.3F, 0.82F, 0.38F, 1.0F};
        const float mageBase[4] = {0.42F, 0.25F, 0.75F, 1.0F};
        const float mageHover[4] = {0.55F, 0.35F, 0.95F, 1.0F};

        switch (snapshot.characterClass) {
        case CharacterClass::WARRIOR:
            drawMenuButtonBackground(card, warriorBase, warriorHover);
            break;
        case CharacterClass::RANGER:
            drawMenuButtonBackground(card, rangerBase, rangerHover);
            break;
        case CharacterClass::MAGE:
            drawMenuButtonBackground(card, mageBase, mageHover);
            break;
        case CharacterClass::NONE:
            break;
        }

        if (mobAssets.hasClassSheets()) {
            const float spriteW = card.bounds.width * 0.72F;
            const float spriteH = card.bounds.height * 0.52F;
            const float spriteX = card.bounds.x + (card.bounds.width - spriteW) * 0.5F;
            const float spriteY = card.bounds.y + currentUiScale().dim(24.0F);
            drawClassSpriteUi(
                snapshot.characterClass, spriteX, spriteY, spriteW, spriteH, render::SpriteClip::Idle);
        }

        const MenuButton& back = buttons.back();
        const float backBase[4] = {0.25F, 0.27F, 0.32F, 1.0F};
        const float backHover[4] = {0.38F, 0.4F, 0.48F, 1.0F};
        drawMenuButtonBackground(back, backBase, backHover);

        uiRenderer.endFrame();

        const ui::UiScale layout = currentUiScale();
        const float detailColor[4] = {0.92F, 0.95F, 1.0F, 1.0F};
        const float hintColor[4] = {0.78F, 0.82F, 0.9F, 1.0F};

        textRenderer.beginOverlay();
        drawTitle("Load Character", 52.0F, 2.8F);

        ui::Rect classLabelBounds{
            card.bounds.x,
            card.bounds.y + card.bounds.height - layout.dim(118.0F),
            card.bounds.width,
            layout.dim(32.0F)};
        drawMenuButtonLabel(classLabelBounds, characterClassName(snapshot.characterClass));

        std::ostringstream levelLine;
        levelLine << "Level " << snapshot.character.level;
        const std::string levelText = levelLine.str();
        ui::Rect levelBounds{
            card.bounds.x,
            classLabelBounds.y + layout.dim(34.0F),
            card.bounds.width,
            layout.dim(24.0F)};
        textRenderer.drawTextCentered(levelBounds, levelText.c_str(), layout.dim(1.6F), detailColor);

        std::ostringstream zoneLine;
        zoneLine << zoneLabel(snapshot.world.activeZone) << "  |  Depth "
                 << snapshot.progression.depth;
        const std::string zoneText = zoneLine.str();
        ui::Rect zoneBounds{
            card.bounds.x,
            levelBounds.y + layout.dim(26.0F),
            card.bounds.width,
            layout.dim(22.0F)};
        textRenderer.drawTextCentered(zoneBounds, zoneText.c_str(), layout.dim(1.35F), hintColor);

        std::ostringstream goldLine;
        goldLine << snapshot.character.gold << " gold";
        const std::string goldText = goldLine.str();
        ui::Rect goldBounds{
            card.bounds.x,
            zoneBounds.y + layout.dim(24.0F),
            card.bounds.width,
            layout.dim(22.0F)};
        textRenderer.drawTextCentered(goldBounds, goldText.c_str(), layout.dim(1.35F), hintColor);

        ui::Rect clickHintBounds{
            card.bounds.x,
            card.bounds.y + layout.dim(8.0F),
            card.bounds.width,
            layout.dim(20.0F)};
        textRenderer.drawTextCentered(clickHintBounds, "Click to continue", layout.dim(1.2F), hintColor);

        drawMenuButtonLabel(back.bounds, back.label);
        textRenderer.endOverlay();
        glEnable(GL_DEPTH_TEST);
    }

    void renderSettingsSliderRow(
        const ui::Rect& track,
        float value,
        float minValue,
        float maxValue,
        bool hovered) const {
        const ui::UiScale layout = currentUiScale();
        const float normalized = (value - minValue) / std::max(maxValue - minValue, 0.001F);
        const float fillRatio = std::clamp(normalized, 0.0F, 1.0F);

        if (uiAssets.isLoaded()) {
            const float tint[4] = {1.0F, 1.0F, 1.0F, hovered ? 1.0F : 0.9F};
            uiRenderer.drawTexturedRect(
                uiAssets.settingsBarTrack(), track.x, track.y, track.width, track.height, tint);

            const float fillWidth = track.width * fillRatio;
            if (fillWidth > 1.0F) {
                uiRenderer.drawTexturedRectUV(
                    uiAssets.settingsBarFill(),
                    track.x,
                    track.y,
                    fillWidth,
                    track.height,
                    0.0F,
                    0.0F,
                    fillRatio,
                    1.0F,
                    tint);
            }

            const float knobSize = layout.dim(22.0F);
            const float knobX = track.x + fillWidth - knobSize * 0.5F;
            uiRenderer.drawTexturedRect(
                uiAssets.settingsBarKnob(),
                knobX,
                track.y + (track.height - knobSize) * 0.5F,
                knobSize,
                knobSize,
                tint);
            return;
        }

        const float trackBg[4] = {0.14F, 0.16F, 0.22F, 1.0F};
        const float trackBorder[4] = {0.35F, 0.42F, 0.58F, hovered ? 1.0F : 0.7F};
        const float fill[4] = {0.35F, 0.55F, 0.9F, 1.0F};
        const float knob[4] = {0.92F, 0.95F, 1.0F, 1.0F};

        uiRenderer.drawFilledRect(track.x, track.y, track.width, track.height, trackBg);
        uiRenderer.drawOutlineRect(track.x, track.y, track.width, track.height, trackBorder);

        const float fillWidth = track.width * fillRatio;
        if (fillWidth > 0.0F) {
            uiRenderer.drawFilledRect(track.x, track.y, fillWidth, track.height, fill);
        }

        const float knobWidth = layout.dim(8.0F);
        const float knobX = track.x + fillWidth - knobWidth * 0.5F;
        uiRenderer.drawFilledRect(
            knobX, track.y - layout.dim(2.0F), knobWidth, track.height + layout.dim(4.0F), knob);
    }

    void renderSettingsPanelBackground() {
        const ui::SettingsPanelLayout layout = settingsPanelLayout();
        const ui::Rect& panel = layout.panel;

        drawRpgPanel(panel);
        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawTexturedRect(
                uiAssets.settingsCross(),
                layout.closeButton.x,
                layout.closeButton.y,
                layout.closeButton.width,
                layout.closeButton.height,
                white);
        } else {
            drawRpgFrame(layout.closeButton, currentUiScale().dim(3.0F));
        }

        const std::vector<SettingsControl> controls = buildSettingsControls();
        for (std::size_t index = 0; index < controls.size(); ++index) {
            const SettingsControl& control = controls[index];
            const bool hovered = hoveredButtonId == control.id;
            if (control.kind == SettingsControlKind::Slider) {
                renderSettingsSliderRow(
                    control.bounds,
                    control.sliderValue != nullptr ? *control.sliderValue : 0.0F,
                    control.sliderMin,
                    control.sliderMax,
                    hovered);
                continue;
            }

            if (uiAssets.isLoaded()) {
                const float tint[4] = {1.0F, 1.0F, 1.0F, hovered ? 1.0F : 0.88F};
                uiRenderer.drawTexturedRect(
                    hovered ? uiAssets.inventorySlotHover() : uiAssets.inventorySlot(),
                    control.bounds.x,
                    control.bounds.y,
                    control.bounds.width,
                    control.bounds.height,
                    tint);
            } else {
                drawRpgButton(control.bounds, hovered, true);
            }
        }

        const MenuButton back = buildBackButton(static_cast<float>(currentUiScale().height) * 0.78F);
        const float backBase[4] = {0.25F, 0.27F, 0.32F, 1.0F};
        const float backHover[4] = {0.38F, 0.4F, 0.48F, 1.0F};
        drawMenuButtonBackground(back, backBase, backHover);
    }

    void renderSettingsPanelText() const {
        const ui::SettingsPanelLayout layout = settingsPanelLayout();
        const float labelColor[4] = {0.9F, 0.92F, 0.98F, 1.0F};
        const float valueColor[4] = {0.75F, 0.88F, 1.0F, 1.0F};

        const ui::UiScale scale = currentUiScale();
        const ui::Rect titleBounds{
            layout.panel.x,
            layout.titleY,
            layout.panel.width,
            scale.dim(40.0F)};
        textRenderer.drawTextCentered(titleBounds, "Settings", layout.titleScale, labelColor);
        const float closeMark[4] = {0.98F, 0.94F, 0.82F, 1.0F};
        textRenderer.drawTextCentered(layout.closeButton, "X", scale.dim(2.2F), closeMark);

        const int* rowIds = kSettingsRowIds;

        for (int row = 0; row < ui::SettingsPanelLayout::kRowCount; ++row) {
            const ui::SettingsRowLayout& rowLayout = layout.rows[row];
            const int controlId = rowIds[row];

            drawBoundedText(rowLayout.label, settingsRowLabel(row), layout.labelScale, labelColor);

            if (rowLayout.kind == ui::SettingsRowKind::Slider) {
                drawBoundedText(
                    rowLayout.value,
                    settingsControlValueLabel(controlId),
                    layout.valueScale,
                    valueColor);
                continue;
            }

            std::string cycleValue = settingsControlValueLabel(controlId);
            cycleValue += "  (click)";
            drawBoundedText(rowLayout.control, cycleValue, layout.valueScale, valueColor);
        }

        const MenuButton back = buildBackButton(static_cast<float>(scale.height) * 0.78F);
        drawMenuButtonLabel(back.bounds, back.label);
    }

    void renderSettings(bool drawBackdrop) {
        if (drawBackdrop) {
            renderMenuBackdrop();
        }
        uiRenderer.beginFrame();
        renderSettingsPanelBackground();
        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        renderSettingsPanelText();
        textRenderer.endOverlay();
        glEnable(GL_DEPTH_TEST);
    }

    void renderPauseOverlay() {
        applyPointerCursor(PointerCursor::Default);
        advanceSpriteAnimClock();
        uiRenderer.beginFrame();
        const float dimmer[4] = {0.02F, 0.01F, 0.015F, 0.72F};
        uiRenderer.drawFilledRect(
            0.0F, 0.0F, static_cast<float>(window.width()), static_cast<float>(window.height()), dimmer);
        drawMenuAtmosphere();

        if (pauseSettingsOpen) {
            renderSettingsPanelBackground();
            uiRenderer.endFrame();
            textRenderer.beginOverlay();
            renderSettingsPanelText();
            textRenderer.endOverlay();
            glEnable(GL_DEPTH_TEST);
            return;
        }

        const std::vector<MenuButton> buttons = buildPauseMenuButtons();
        const float stone[4] = {0.12F, 0.09F, 0.07F, 0.96F};
        const float stoneHover[4] = {0.24F, 0.16F, 0.08F, 1.0F};
        if (!buttons.empty()) {
            const ui::Rect& first = buttons.front().bounds;
            const ui::Rect& last = buttons.back().bounds;
            const float pad = currentUiScale().dim(28.0F);
            drawStonePlaque(
                {first.x - pad,
                 first.y - pad * 2.4F,
                 first.width + pad * 2.0F,
                 (last.y + last.height) - first.y + pad * 3.4F});
        }
        for (const MenuButton& button : buttons) {
            drawMenuButtonBackground(button, stone, stoneHover);
        }

        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        drawTitle("Paused", 150.0F, 3.0F);
        for (const MenuButton& button : buttons) {
            drawMenuButtonLabel(button.bounds, button.label, hoveredButtonId == button.id);
        }
        textRenderer.endOverlay();
    }

    void drawCube(
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& position,
        const glm::vec3& scale,
        const glm::vec3& color) const {
        glm::mat4 model = glm::translate(glm::mat4(1.0F), position);
        model = glm::scale(model, scale);

        worldShader.use();
        worldShader.setModel(model);
        worldShader.setView(view);
        worldShader.setProjection(projection);
        worldShader.setVec3("u_ObjectColor", color);
        cubeMesh.draw();
    }

    struct EntityHighlightBounds {
        glm::vec3 center{0.0F};
        glm::vec3 halfExtents{0.5F};
    };

    [[nodiscard]] EntityHighlightBounds highlightBoundsFor(
        const gameplay::WorldEntitySnapshot& entity) const {
        EntityHighlightBounds bounds{};
        const glm::vec3 ground = toGlm(entity.position);

        if (worldPropAssets.hasProps(entity.kind)) {
            const float height = worldPropAssets.worldHeight(entity.kind, entity.variant);
            const float radius = worldPropAssets.pickRadius(entity.kind, entity.variant);
            bounds.halfExtents = glm::vec3(radius * 1.02F, height * 0.52F, radius * 1.02F);
            bounds.center = ground + glm::vec3(0.0F, bounds.halfExtents.y, 0.0F);
            return bounds;
        }

        if (mobAssets.isLoaded() && mobAssets.isSpriteEntity(entity.kind)) {
            const float height = mobAssets.spriteWorldHeight(entity.kind);
            bounds.halfExtents = glm::vec3(height * 0.34F, height * 0.52F, height * 0.34F);
            bounds.center = ground + glm::vec3(0.0F, bounds.halfExtents.y, 0.0F);
            return bounds;
        }

        const EntityVisual visual = visualFor(entity.kind);
        bounds.halfExtents = visual.scale * 0.52F;
        bounds.center = ground + glm::vec3(0.0F, bounds.halfExtents.y, 0.0F);
        return bounds;
    }

    void drawWireHighlight(
        const glm::mat4& view,
        const glm::mat4& projection,
        const EntityHighlightBounds& bounds,
        const glm::vec4& color) const {
        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean depthMaskWasEnabled = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glm::mat4 model = glm::translate(glm::mat4(1.0F), bounds.center);
        model = glm::scale(model, bounds.halfExtents * 2.04F);

        wireShader.use();
        wireShader.setModel(model);
        wireShader.setView(view);
        wireShader.setProjection(projection);
        wireShader.setVec4("u_Color", color);
        cubeWireMesh.draw();

        glDepthMask(depthMaskWasEnabled);
        if (!blendWasEnabled) {
            glDisable(GL_BLEND);
        }
        if (cullWasEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    struct EntitySpriteVisual {
        const render::Texture* texture{nullptr};
        glm::vec3 position{0.0F};
        float height{0.0F};
        float u0{0.0F};
        float v0{0.0F};
        float u1{1.0F};
        float v1{1.0F};

        [[nodiscard]] bool isValid() const noexcept {
            return texture != nullptr && texture->isValid() && height > 0.0F;
        }
    };

    [[nodiscard]] render::SpriteFacing entitySpriteFacing(
        const gameplay::WorldEntitySnapshot& entity) const noexcept {
        const glm::vec2 currentXZ(entity.position.x, entity.position.z);
        glm::vec2 delta{0.0F};
        const auto iterator = entityLastXZ_.find(entity.id);
        if (iterator != entityLastXZ_.end()) {
            delta = currentXZ - iterator->second;
        }

        const auto facing8 = entityFacing8_.find(entity.id);
        if (facing8 != entityFacing8_.end()) {
            return render::facing4From8(facing8->second);
        }
        constexpr float kStationaryThresholdSq = 0.02F * 0.02F;
        if (glm::dot(delta, delta) <= kStationaryThresholdSq) {
            return render::SpriteFacing::Down;
        }
        return spriteFacingFromDelta(delta);
    }

    [[nodiscard]] EntitySpriteVisual entitySpriteVisual(
        const gameplay::WorldEntitySnapshot& entity) const {
        EntitySpriteVisual visual{};
        visual.position = toGlm(entity.position);

        if (worldPropAssets.hasProps(entity.kind)) {
            visual.texture = worldPropAssets.texture(entity.kind, entity.variant);
            visual.height = worldPropAssets.worldHeight(entity.kind, entity.variant);
            return visual;
        }

        if (mobAssets.isLoaded() && mobAssets.isSpriteEntity(entity.kind)) {
            const bool useIdlePose = shouldUseIdlePose(entity);
            const render::SpriteFacing facing = entitySpriteFacing(entity);
            const render::SpriteClip clip =
                useIdlePose ? render::SpriteClip::Idle : render::SpriteClip::Walk;
            const render::SpriteFrameSample sample = mobAssets.sampleMobSprite(
                entity.kind, entity.id, useIdlePose, clip, facing, spriteAnimTime_);
            if (sample.texture != nullptr) {
                visual.texture = sample.texture;
                visual.height = mobAssets.spriteWorldHeight(entity.kind);
                visual.u0 = sample.uv.u0;
                visual.v0 = sample.uv.v0;
                visual.u1 = sample.uv.u1;
                visual.v1 = sample.uv.v1;
            }
        }

        return visual;
    }

    void drawEntityHoverHighlight(
        const glm::mat4& view,
        const glm::mat4& projection,
        const gameplay::WorldEntitySnapshot& entity,
        const glm::vec4& color) const {
        const float pulse = 0.88F + 0.12F * std::sin(spriteAnimTime_ * 4.0F);
        glm::vec4 pulseColor = color;
        pulseColor.a *= pulse;

        const EntitySpriteVisual visual = entitySpriteVisual(entity);
        if (visual.isValid()) {
            const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
            const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
            GLboolean depthMaskWasEnabled = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            spriteRenderer.drawBillboardOutline(
                *visual.texture,
                visual.position,
                visual.height,
                visual.u0,
                visual.v0,
                visual.u1,
                visual.v1,
                view,
                projection,
                pulseColor,
                2.5F);

            glDepthMask(depthMaskWasEnabled);
            if (!blendWasEnabled) {
                glDisable(GL_BLEND);
            }
            if (cullWasEnabled) {
                glEnable(GL_CULL_FACE);
            } else {
                glDisable(GL_CULL_FACE);
            }
            return;
        }

        if (mobAssets.isSpriteEntity(entity.kind) || worldPropAssets.hasProps(entity.kind)) {
            return;
        }

        const EntityHighlightBounds bounds = highlightBoundsFor(entity);
        drawWireHighlight(view, projection, bounds, pulseColor);
    }

    [[nodiscard]] const gameplay::WorldEntitySnapshot* findEntityById(const std::uint32_t id) const {
        ensureSceneryCaches();
        const auto iterator = entityIndexById_.find(id);
        if (iterator == entityIndexById_.end()) {
            return nullptr;
        }

        const std::vector<gameplay::WorldEntitySnapshot>& scenery = zoneManager.scenery();
        if (iterator->second >= scenery.size() || scenery[iterator->second].id != id) {
            return nullptr;
        }
        return &scenery[iterator->second];
    }

    void updateInteractableHover(float mouseX, float mouseY) {
        hoveredInteractableId.reset();

        if (gamePaused || isMouseOverInGameUi(mouseX, mouseY)) {
            lastHoveredLogId_ = 0xFFFFFFFFU;
            return;
        }

        const gameplay::CameraMatrices cameraMatrices =
            camera.matricesForTarget(cameraFocus());
        const ScreenRay ray = buildScreenRay(
            mouseX, mouseY, window.width(), window.height(), cameraMatrices);
        hoveredInteractableId = pickInteractableEntity(ray, zoneManager.scenery());

        if (!hoveredInteractableId.has_value()) {
            lastHoveredLogId_ = 0xFFFFFFFFU;
            return;
        }

        if (*hoveredInteractableId == lastHoveredLogId_) {
            return;
        }

        lastHoveredLogId_ = *hoveredInteractableId;
        if (const gameplay::WorldEntitySnapshot* entity = findEntityById(lastHoveredLogId_)) {
            std::ostringstream message;
            message << "Hover: " << interactableHint(entity->kind);
            logInfo(message.str());
        }
    }

    struct WorldLightSettings {
        glm::vec3 playerPos{0.0F};
        float radius{10.0F};
        float ambientDark{0.06F};
        float ambientBright{0.38F};
    };

    [[nodiscard]] WorldLightSettings buildWorldLightSettings() const {
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const bool onPlains = zoneManager.activeZone() == gameplay::WorldZone::PLAINS;
        WorldLightSettings light{};
        light.playerPos = playerPosition;
        light.radius = effective.lightRadius;
        light.ambientDark = onPlains ? 0.012F : 0.028F;
        light.ambientBright = onPlains ? 0.78F : 0.64F;
        if (laneActive_) {
            light.ambientDark = 0.045F;
            light.ambientBright = 0.62F;
            light.radius = std::max(effective.lightRadius, 22.0F);
        }
        return light;
    }

    void applyPlayerLightUniforms() const {
        const WorldLightSettings light = buildWorldLightSettings();
        worldShader.use();
        worldShader.setVec3("u_PlayerPos", light.playerPos);
        worldShader.setFloat("u_LightRadius", light.radius);
        worldShader.setFloat("u_AmbientDark", light.ambientDark);
        worldShader.setFloat("u_AmbientBright", light.ambientBright);
        worldShader.setVec3("u_LightColor", glm::vec3(1.0F, 0.92F, 0.75F));
    }

    [[nodiscard]] float playerSpriteAnimTime(const render::SpriteClip clip) const noexcept {
        if ((clip == render::SpriteClip::Attack || clip == render::SpriteClip::Attack2 ||
             clip == render::SpriteClip::Cast || clip == render::SpriteClip::Hit) &&
            playerAnim_.isBusy()) {
            return playerAnim_.stateTime();
        }
        return spriteAnimTime_;
    }

    void drawSpriteShadow(
        const glm::vec3& position,
        const float height,
        const gameplay::CameraMatrices& cameraMatrices,
        const WorldLightSettings& light) const {
        if (!groundShadow_.isValid()) {
            return;
        }
        spriteRenderer.drawGroundShadow(
            groundShadow_,
            position,
            std::max(0.45F, height * 0.28F),
            cameraMatrices.view,
            cameraMatrices.projection,
            light.playerPos,
            light.radius,
            light.ambientDark,
            light.ambientBright);
    }

    void drawPlayerWorldSprite(
        const WorldLightSettings& light,
        const glm::mat4& view,
        const glm::mat4& projection) const {
        const render::SpriteClip clip = resolvePlayerClip();
        const render::SpriteFrameSample sample = mobAssets.sampleClassSprite(
            selectedClass, clip, playerAnim_.facing8(), playerSpriteAnimTime(clip));
        if (sample.texture == nullptr) {
            return;
        }

        spriteRenderer.drawBillboardUV(
            *sample.texture,
            playerPosition,
            mobAssets.spriteWorldHeight(gameplay::EntityKind::PLAYER),
            sample.uv.u0,
            sample.uv.v0,
            sample.uv.u1,
            sample.uv.v1,
            view,
            projection,
            light.playerPos,
            light.radius,
            light.ambientDark,
            light.ambientBright,
            glm::vec4(1.0F));
    }

    void finalizeEntityMotionTracking() {
        const glm::vec2 playerXZ(playerPosition.x, playerPosition.z);
        lastPlayerXZ_ = playerXZ;

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !mobAssets.isSpriteEntity(entity.kind)) {
                continue;
            }
            const glm::vec2 currentXZ(entity.position.x, entity.position.z);
            const auto last = entityLastXZ_.find(entity.id);
            if (last != entityLastXZ_.end()) {
                render::SpriteFacing8& facing = entityFacing8_[entity.id];
                facing = render::facing8FromDelta(currentXZ - last->second, facing);
            }
            entityLastXZ_[entity.id] = currentXZ;
        }
    }

    [[nodiscard]] bool isPlayerMoving() const {
        constexpr float kMoveThresholdSq = 0.02F * 0.02F;
        if (hasMoveTarget) {
            return true;
        }
        const glm::vec2 playerXZ(playerPosition.x, playerPosition.z);
        const glm::vec2 delta = playerXZ - lastPlayerXZ_;
        return glm::dot(delta, delta) > kMoveThresholdSq;
    }

    [[nodiscard]] bool isEntityMoving(const std::uint32_t entityId, const glm::vec2& currentXZ) const {
        const auto iterator = entityLastXZ_.find(entityId);
        if (iterator == entityLastXZ_.end()) {
            return false;
        }
        const glm::vec2 delta = currentXZ - iterator->second;
        constexpr float kMoveThresholdSq = 0.02F * 0.02F;
        return glm::dot(delta, delta) > kMoveThresholdSq;
    }

    [[nodiscard]] bool shouldUseIdlePose(const gameplay::WorldEntitySnapshot& entity) const {
        const glm::vec2 currentXZ(entity.position.x, entity.position.z);
        return !isEntityMoving(entity.id, currentXZ);
    }

    void drawWorldPropSprite(
        const gameplay::WorldEntitySnapshot& entity,
        const glm::mat4& view,
        const glm::mat4& projection,
        const WorldLightSettings& light) const {
        const render::Texture* texture =
            worldPropAssets.texture(entity.kind, entity.variant);
        if (texture == nullptr) {
            return;
        }

        const glm::vec3 worldPosition = toGlm(entity.position);
        const float height = worldPropAssets.worldHeight(entity.kind, entity.variant);
        spriteRenderer.drawBillboardUV(
            *texture,
            worldPosition,
            height,
            0.0F,
            0.0F,
            1.0F,
            1.0F,
            view,
            projection,
            light.playerPos,
            light.radius,
            light.ambientDark,
            light.ambientBright,
            glm::vec4(1.0F));
    }

    void drawEntitySprite(
        const gameplay::WorldEntitySnapshot& entity,
        const glm::mat4& view,
        const glm::mat4& projection,
        const WorldLightSettings& light) const {
        if (!mobAssets.isLoaded() || !mobAssets.isSpriteEntity(entity.kind)) {
            return;
        }

        bool useIdlePose = shouldUseIdlePose(entity);
        render::SpriteFacing8 facing8 = render::SpriteFacing8::South;
        const auto facing = entityFacing8_.find(entity.id);
        if (facing != entityFacing8_.end()) {
            facing8 = facing->second;
        }
        render::SpriteClip clip = useIdlePose ? render::SpriteClip::Idle : render::SpriteClip::Walk;
        float clipTime = spriteAnimTime_;
        const auto animation = mobAnimations_.find(entity.id);
        if (animation != mobAnimations_.end() && animation->second.isBusy()) {
            clip = animation->second.clip();
            clipTime = animation->second.stateTime();
            useIdlePose = false;
        }
        const render::SpriteFrameSample sample = mobAssets.sampleMobSprite(
            entity.kind, entity.id, useIdlePose, clip, facing8, clipTime);
        if (sample.texture == nullptr) {
            return;
        }

        const float height = mobAssets.spriteWorldHeight(entity.kind);
        glm::vec4 tint{1.0F};
        if (entity.kind == gameplay::EntityKind::ENEMY_BOSS) {
            tint = glm::vec4(1.0F, 0.85F, 1.0F, 1.0F);
        }

        spriteRenderer.drawBillboardUV(
            *sample.texture,
            toGlm(entity.position),
            height,
            sample.uv.u0,
            sample.uv.v0,
            sample.uv.u1,
            sample.uv.v1,
            view,
            projection,
            light.playerPos,
            light.radius,
            light.ambientDark,
            light.ambientBright,
            tint);
    }

    void drawDyingMobSprite(
        const DyingMob& dying,
        const glm::mat4& view,
        const glm::mat4& projection,
        const WorldLightSettings& light) const {
        if (!mobAssets.isLoaded()) {
            return;
        }
        const render::SpriteFrameSample sample = mobAssets.sampleMobSprite(
            dying.kind, dying.id, false, render::SpriteClip::Death, dying.anim.facing8(), dying.anim.stateTime());
        if (sample.texture == nullptr) {
            return;
        }
        const float progress = dying.anim.stateProgress();
        const float alpha = 1.0F - progress * progress;
        // Corpse sinks slightly and desaturates to red as it fades.
        const glm::vec3 position = dying.position - glm::vec3(0.0F, progress * 0.25F, 0.0F);
        const glm::vec4 tint(1.0F, 1.0F - progress * 0.6F, 1.0F - progress * 0.7F, alpha);
        spriteRenderer.drawBillboardUV(
            *sample.texture,
            position,
            mobAssets.spriteWorldHeight(dying.kind),
            sample.uv.u0,
            sample.uv.v0,
            sample.uv.u1,
            sample.uv.v1,
            view,
            projection,
            light.playerPos,
            light.radius,
            light.ambientDark,
            light.ambientBright,
            tint);
    }

    [[nodiscard]] gameplay::Vec3 cameraFocus() const noexcept {
        glm::vec3 focus = playerPosition + combatFeedback_.shakeOffset();
        if (laneActive_) {
            focus.x += 5.0F;
        }
        return toVec3(focus);
    }

    void renderWorld() {
        if (!laneActive_ && !zoneManager.allowsFreeMovement()) {
            glDisable(GL_DEPTH_TEST);
            glClearColor(0.05F, 0.035F, 0.04F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            return;
        }

        const gameplay::CameraMatrices cameraMatrices = camera.matricesForTarget(cameraFocus());

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.004F, 0.006F, 0.014F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        applyPlayerLightUniforms();

        refreshVisibleGround();
        const float centerX = (visibleGround_.minX + visibleGround_.maxX) * 0.5F;
        const float centerZ = (visibleGround_.minZ + visibleGround_.maxZ) * 0.5F;
        const float spanX = std::max(visibleGround_.maxX - visibleGround_.minX, 1.0F);
        const float spanZ = std::max(visibleGround_.maxZ - visibleGround_.minZ, 1.0F);

        drawCube(
            cameraMatrices.view,
            cameraMatrices.projection,
            glm::vec3(centerX, -0.05F, centerZ),
            glm::vec3(spanX, 0.1F, spanZ),
            glm::vec3(0.18F, 0.22F, 0.2F));

        if (laneActive_) {
            drawCube(
                cameraMatrices.view,
                cameraMatrices.projection,
                glm::vec3(centerX, 0.02F, kLaneCenterZ),
                glm::vec3(spanX, 0.05F, 4.5F),
                glm::vec3(0.34F, 0.26F, 0.16F));
        }

        const bool gateVisible = visibleGround_.maxX >= -6.0F && visibleGround_.minX <= 6.0F &&
                                 visibleGround_.maxZ >= 39.0F && visibleGround_.minZ <= 41.0F;
        if (gateVisible) {
            const float gateColor =
                zoneManager.activeZone() == gameplay::WorldZone::TOWN ? 0.35F : 0.9F;
            drawCube(
                cameraMatrices.view,
                cameraMatrices.projection,
                glm::vec3(0.0F, 0.05F, 40.0F),
                glm::vec3(12.0F, 0.15F, 2.0F),
                glm::vec3(gateColor, 0.4F, zoneManager.activeZone() == gameplay::WorldZone::TOWN ? 0.9F : 0.35F));
        }

        const WorldLightSettings worldLight = buildWorldLightSettings();
        const bool mobSpritesLoaded = mobAssets.isLoaded();

        spriteDrawCommands_.clear();
        spriteDrawCommands_.reserve(zoneManager.scenery().size() + 1U);

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active) {
                continue;
            }

            const glm::vec3 worldPosition = toGlm(entity.position);
            if (!isInsideVisibleGround(worldPosition)) {
                continue;
            }

            if (worldPropAssets.hasProps(entity.kind)) {
                SpriteDrawCommand command{};
                command.entity = &entity;
                command.isWorldProp = true;
                command.sortKey = gameplay::isometricSortKey(cameraMatrices.view, worldPosition);
                command.sortId = entity.id;
                spriteDrawCommands_.push_back(command);
                continue;
            }

            if (mobSpritesLoaded && mobAssets.isSpriteEntity(entity.kind)) {
                SpriteDrawCommand command{};
                command.entity = &entity;
                command.sortKey = gameplay::isometricSortKey(cameraMatrices.view, worldPosition);
                command.sortId = entity.id;
                spriteDrawCommands_.push_back(command);
                continue;
            }

            const EntityVisual visual = visualFor(entity.kind);
            drawCube(
                cameraMatrices.view,
                cameraMatrices.projection,
                worldPosition,
                visual.scale,
                visual.color);
        }

        if (mobAssets.hasClassSheets() && selectedClass != CharacterClass::NONE) {
            SpriteDrawCommand playerCommand{};
            playerCommand.isPlayer = true;
            playerCommand.sortKey = gameplay::isometricSortKey(cameraMatrices.view, playerPosition);
            playerCommand.sortId = std::numeric_limits<std::uint32_t>::max();
            spriteDrawCommands_.push_back(playerCommand);
        }

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (const DyingMob& dying : dyingMobs_) {
            if (!isInsideVisibleGround(dying.position)) {
                continue;
            }
            SpriteDrawCommand command{};
            command.dying = &dying;
            command.sortKey = gameplay::isometricSortKey(cameraMatrices.view, dying.position);
            command.sortId = dying.id;
            spriteDrawCommands_.push_back(command);
        }

        std::sort(
            spriteDrawCommands_.begin(),
            spriteDrawCommands_.end(),
            [](const SpriteDrawCommand& a, const SpriteDrawCommand& b) {
                if (a.sortKey != b.sortKey) {
                    return a.sortKey < b.sortKey;
                }
                return a.sortId < b.sortId;
            });

        spriteRenderer.beginBillboardPass(
            cameraMatrices.view,
            cameraMatrices.projection,
            worldLight.playerPos,
            worldLight.radius,
            worldLight.ambientDark,
            worldLight.ambientBright);

        const auto shadowForCommand = [&](const SpriteDrawCommand& command) {
            if (command.isPlayer) {
                drawSpriteShadow(
                    playerPosition, mobAssets.spriteWorldHeight(gameplay::EntityKind::PLAYER), cameraMatrices, worldLight);
            } else if (command.dying != nullptr) {
                drawSpriteShadow(
                    command.dying->position, mobAssets.spriteWorldHeight(command.dying->kind), cameraMatrices, worldLight);
            } else if (command.entity != nullptr && command.isWorldProp) {
                drawSpriteShadow(
                    toGlm(command.entity->position),
                    worldPropAssets.worldHeight(command.entity->kind, command.entity->variant),
                    cameraMatrices,
                    worldLight);
            } else if (command.entity != nullptr) {
                drawSpriteShadow(
                    toGlm(command.entity->position),
                    mobAssets.spriteWorldHeight(command.entity->kind),
                    cameraMatrices,
                    worldLight);
            }
        };
        const auto spriteForCommand = [&](const SpriteDrawCommand& command) {
            if (command.isPlayer) {
                drawPlayerWorldSprite(worldLight, cameraMatrices.view, cameraMatrices.projection);
            } else if (command.dying != nullptr) {
                drawDyingMobSprite(*command.dying, cameraMatrices.view, cameraMatrices.projection, worldLight);
            } else if (command.entity != nullptr && command.isWorldProp) {
                drawWorldPropSprite(*command.entity, cameraMatrices.view, cameraMatrices.projection, worldLight);
            } else if (command.entity != nullptr) {
                drawEntitySprite(*command.entity, cameraMatrices.view, cameraMatrices.projection, worldLight);
            }
        };

        for (const SpriteDrawCommand& command : spriteDrawCommands_) {
            shadowForCommand(command);
        }
        for (const SpriteDrawCommand& command : spriteDrawCommands_) {
            spriteForCommand(command);
        }

        spriteRenderer.endBillboardPass();

        if (hasMoveTarget) {
            renderMoveTargetMarker(cameraMatrices.view, cameraMatrices.projection, worldLight);
        }

        particleRenderer.draw(particles_, cameraMatrices.view, cameraMatrices.projection);

        glDepthMask(GL_TRUE);

        const auto drawInteractableHighlight = [&](std::uint32_t entityId, const glm::vec4& color) {
            if (const gameplay::WorldEntitySnapshot* entity = findEntityById(entityId)) {
                drawEntityHoverHighlight(
                    cameraMatrices.view, cameraMatrices.projection, *entity, color);
            }
        };

        if (combatSystem.hasTarget()) {
            drawInteractableHighlight(
                *combatSystem.targetId(), glm::vec4(1.0F, 0.42F, 0.32F, 0.52F));
        }
        if (hoveredInteractableId.has_value() &&
            (!combatSystem.hasTarget() || *hoveredInteractableId != *combatSystem.targetId())) {
            drawInteractableHighlight(
                *hoveredInteractableId, glm::vec4(0.55F, 0.88F, 1.0F, 0.48F));
        }

        if (!mobAssets.hasClassSheets() || selectedClass == CharacterClass::NONE) {
            glm::vec3 playerColor = visualFor(gameplay::EntityKind::PLAYER).color;
            if (selectedClass == CharacterClass::WARRIOR) {
                playerColor = glm::vec3(0.85F, 0.35F, 0.25F);
            } else if (selectedClass == CharacterClass::RANGER) {
                playerColor = glm::vec3(0.3F, 0.8F, 0.35F);
            } else if (selectedClass == CharacterClass::MAGE) {
                playerColor = glm::vec3(0.5F, 0.35F, 0.95F);
            }

            glm::mat4 playerModel = glm::translate(glm::mat4(1.0F), playerPosition);
            playerModel = glm::rotate(playerModel, playerYaw, glm::vec3(0.0F, 1.0F, 0.0F));
            worldShader.use();
            worldShader.setModel(playerModel);
            worldShader.setView(cameraMatrices.view);
            worldShader.setProjection(cameraMatrices.projection);
            worldShader.setVec3("u_ObjectColor", playerColor);
            cubeMesh.draw();
        }
        glDisable(GL_BLEND);
    }

    void renderMoveTargetMarker(
        const glm::mat4& view,
        const glm::mat4& projection,
        const WorldLightSettings& light) const {
        const glm::vec3 markerPosition{moveTarget.x, moveTarget.y + 0.06F, moveTarget.z};
        const float markerHeight = currentUiScale().dim(kMoveTargetMarkerWorldHeight);

        if (uiAssets.isLoaded() && uiAssets.settingsCross().isValid()) {
            const glm::vec4 tint{0.35F, 0.95F, 1.0F, 0.95F};
            spriteRenderer.drawBillboardUV(
                uiAssets.settingsCross(),
                markerPosition,
                markerHeight,
                0.0F,
                0.0F,
                1.0F,
                1.0F,
                view,
                projection,
                light.playerPos,
                light.radius,
                light.ambientDark,
                light.ambientBright,
                tint);
            return;
        }

        drawCube(
            view,
            projection,
            markerPosition,
            glm::vec3(markerHeight * 0.85F, 0.06F, markerHeight * 0.85F),
            glm::vec3(0.2F, 0.85F, 0.95F));
    }

    void renderScreenFlash() const {
        if (!combatFeedback_.hasFlash()) {
            return;
        }
        const glm::vec4 flash = combatFeedback_.flashColor();
        if (flash.a <= 0.002F) {
            return;
        }
        const float color[4] = {flash.r, flash.g, flash.b, flash.a};
        uiRenderer.drawFilledRect(
            0.0F, 0.0F, static_cast<float>(window.width()), static_cast<float>(window.height()), color);
    }

    void renderHudInfoStrip() {
        if (hudMessage.empty()) {
            return;
        }

        const ui::Rect strip = ui::computeHudChromeLayout(currentUiScale()).messageStrip;
        drawStonePlaque(strip);
    }

    void renderHudInfoStripLabel() const {
        if (hudMessage.empty()) {
            return;
        }

        const ui::UiScale layout = currentUiScale();
        const ui::Rect strip = ui::computeHudChromeLayout(layout).messageStrip;
        const float messageColor[4] = {1.0F, 0.93F, 0.55F, 1.0F};
        const ui::Rect textBounds{
            strip.x + layout.dim(8.0F),
            strip.y + layout.dim(2.0F),
            strip.width - layout.dim(16.0F),
            strip.height - layout.dim(4.0F)};
        drawBoundedText(textBounds, hudMessage, layout.dim(1.5F), messageColor);
    }

    /// Crits start oversized and "punch" down to a larger resting size over the first 0.2s.
    [[nodiscard]] static float floatingTextScale(const FloatingCombatText& floating) noexcept {
        if (!floating.critical) {
            return 1.35F;
        }
        const float punch = std::clamp(1.0F - floating.ageSeconds / 0.2F, 0.0F, 1.0F);
        return 1.55F + punch * 0.45F;
    }

    [[nodiscard]] bool placeFloatingCombatText(
        const FloatingCombatText& floating,
        const std::vector<MobScreenPlate>& plates,
        const int stack,
        float& outX,
        float& outY) const {
        const gameplay::CameraMatrices cameraMatrices = camera.matricesForTarget(cameraFocus());
        float screenX = 0.0F;
        float screenY = 0.0F;
        const glm::vec3 anchor(floating.worldPosition.x, 1.1F, floating.worldPosition.z);
        if (!worldToScreen(
                anchor,
                cameraMatrices.view,
                cameraMatrices.projection,
                window.width(),
                window.height(),
                screenX,
                screenY)) {
            return false;
        }

        const ui::UiScale scale = currentUiScale();
        const float textScale = floatingTextScale(floating);
        const float textWidth = textRenderer.measureTextWidth(floating.text.c_str(), textScale);
        const float textHeight = scale.dim(16.0F) * (textScale / 1.35F);
        const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(scale);
        const float ceiling = scale.dim(40.0F);
        const float floor = console.messageStrip.y - textHeight - scale.dim(6.0F);

        const MobScreenPlate* nearest = nullptr;
        float nearestDistance = 140.0F;
        for (const MobScreenPlate& plate : plates) {
            const float dx = screenX - (plate.bar.x + plate.bar.width * 0.5F);
            const float dy = screenY - plate.bar.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = &plate;
            }
        }

        if (nearest != nullptr) {
            screenX = nearest->bar.x + nearest->bar.width + 8.0F;
            screenY = nearest->name.y - textHeight - 2.0F - floating.ageSeconds * 16.0F -
                static_cast<float>(stack) * (textHeight + 2.0F);
        } else {
            screenY -= 10.0F + floating.ageSeconds * 22.0F + static_cast<float>(stack) * (textHeight + 2.0F);
        }
        screenY = std::clamp(screenY, ceiling, std::max(ceiling, floor));

        ui::Rect bounds{screenX, screenY, textWidth, textHeight};
        if (nearest != nullptr && (rectsOverlap(bounds, nearest->bar) || rectsOverlap(bounds, nearest->name))) {
            bounds.x = nearest->bar.x + nearest->bar.width + 10.0F;
            bounds.y = nearest->name.y - textHeight - 2.0F;
        }
        if (screenRectHitsChrome(bounds)) {
            return false;
        }
        for (const MobScreenPlate& plate : plates) {
            if (rectsOverlap(bounds, plate.bar) || rectsOverlap(bounds, plate.name)) {
                return false;
            }
        }

        outX = bounds.x;
        outY = bounds.y;
        return true;
    }

    void renderFloatingCombatTextBackgrounds() const {}

    void renderFloatingCombatTextLabels() const {
        if (floatingCombatTexts.empty() || worldReadoutsHidden()) {
            return;
        }

        std::vector<MobScreenPlate> plates;
        collectMobPlates(plates);
        int stack = 0;
        for (const FloatingCombatText& floating : floatingCombatTexts) {
            float screenX = 0.0F;
            float screenY = 0.0F;
            if (!placeFloatingCombatText(floating, plates, stack, screenX, screenY)) {
                continue;
            }
            ++stack;

            const float fade = 1.0F - (floating.ageSeconds / floating.lifetimeSeconds);
            const float alpha = std::clamp(fade, 0.0F, 1.0F);
            if (alpha <= 0.05F) {
                continue;
            }
            const float textColor[4] = {floating.colorR, floating.colorG, floating.colorB, alpha};
            const float shadowColor[4] = {0.0F, 0.0F, 0.0F, alpha * 0.85F};
            const float textScale = floatingTextScale(floating);
            textRenderer.drawText(screenX + 1.0F, screenY + 1.0F, floating.text.c_str(), textScale, shadowColor);
            textRenderer.drawText(screenX, screenY, floating.text.c_str(), textScale, textColor);
        }
    }

    void renderInteractableTooltipBackground(float mouseX, float mouseY) {
        cachedInteractableTooltip_.reset();
        if (!hoveredInteractableId.has_value()) {
            return;
        }

        const gameplay::WorldEntitySnapshot* entity = findEntityById(*hoveredInteractableId);
        if (entity == nullptr) {
            return;
        }

        const float tooltipX = mouseX + 16.0F;
        const float tooltipY = mouseY + 16.0F;
        const char* hint = interactableHint(entity->kind);
        constexpr float kTooltipScale = 2.0F;
        const ui::TooltipBoxLayout layout =
            computeItemTooltipLayout(tooltipX, tooltipY, {hint}, kTooltipScale);
        drawTooltipBackground(layout);
        cachedInteractableTooltip_ = CachedTooltip{layout, {hint}, kTooltipScale};
    }

    void renderInteractableTooltipText() const {
        if (!cachedInteractableTooltip_.has_value()) {
            return;
        }

        drawTextTooltipTextOnly(
            cachedInteractableTooltip_->layout,
            cachedInteractableTooltip_->lines,
            cachedInteractableTooltip_->scale);
    }

    void renderMinimap() {
        if (!gameplay::minimapVisible(!zoneManager.showsMinimap(), laneActive_)) {
            return;
        }
        const ui::MinimapWidgetLayout widget = minimapWidgetLayout();
        const ui::Rect& frame = widget.frame;
        const ui::Rect& content = widget.content;

        drawRpgPanel(frame);

        minimap.setViewport(ui::Rect2D{content.x, content.y, content.width, content.height});
        minimap.setViewRadius(kMinimapWorldRadius);

        glm::vec2 radarRight{1.0F, 0.0F};
        glm::vec2 radarUp{0.0F, -1.0F};
        const gameplay::CameraMatrices radarCamera = camera.matricesForTarget(cameraFocus());
        glm::vec3 screenCenter{};
        glm::vec3 screenAhead{};
        const int viewWidth = window.width();
        const int viewHeight = window.height();
        if (gameplay::screenPointToGround(
                static_cast<float>(viewWidth) * 0.5F,
                static_cast<float>(viewHeight) * 0.5F,
                viewWidth,
                viewHeight,
                radarCamera,
                screenCenter) &&
            gameplay::screenPointToGround(
                static_cast<float>(viewWidth) * 0.5F,
                static_cast<float>(viewHeight) * 0.5F - 80.0F,
                viewWidth,
                viewHeight,
                radarCamera,
                screenAhead)) {
            const glm::vec2 ahead{screenAhead.x - screenCenter.x, screenAhead.z - screenCenter.z};
            if (glm::dot(ahead, ahead) > 0.0001F) {
                radarUp = glm::normalize(ahead);
                radarRight = glm::vec2{radarUp.y, -radarUp.x};
            }
        }

        const float radiusSq = kMinimapWorldRadius * kMinimapWorldRadius;
        minimapBlips_.clear();
        const auto pushBlip = [&](const float worldX, const float worldZ, const ui::MinimapBlipKind kind) {
            const float dx = worldX - playerPosition.x;
            const float dz = worldZ - playerPosition.z;
            if (dx * dx + dz * dz > radiusSq) {
                return;
            }
            const float right = dx * radarRight.x + dz * radarRight.y;
            const float forward = dx * radarUp.x + dz * radarUp.y;
            minimapBlips_.push_back(ui::MinimapBlip{right, -forward, kind});
        };

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active) {
                continue;
            }
            const std::optional<ui::MinimapBlipKind> kind = minimapBlipKind(entity.kind);
            if (!kind.has_value()) {
                continue;
            }
            pushBlip(entity.position.x, entity.position.z, *kind);
        }
        pushBlip(0.0F, 40.0F, ui::MinimapBlipKind::Landmark);

        const ui::MinimapLayer layer = minimap.buildLayer(0.0F, 0.0F, minimapBlips_);
        const float disc[4] = {0.04F, 0.025F, 0.018F, 0.94F};
        const float ring[4] = {0.86F, 0.64F, 0.22F, 1.0F};
        const float metal[4] = {0.16F, 0.09F, 0.04F, 1.0F};
        const float ringWidth = std::max(5.0F, currentUiScale().dim(7.0F));
        uiRenderer.drawFilledCircle(layer.center.x, layer.center.y, layer.radiusPixels, ring, 40);
        uiRenderer.drawFilledCircle(
            layer.center.x, layer.center.y, std::max(1.0F, layer.radiusPixels - ringWidth * 0.45F), metal, 40);
        uiRenderer.drawFilledCircle(
            layer.center.x, layer.center.y, std::max(1.0F, layer.radiusPixels - ringWidth), disc, 36);

        const auto drawBlip = [&](const ui::MinimapMarker& marker) {
            float color[4] = {0.9F, 0.35F, 0.25F, 0.95F};
            float size = 4.0F;
            switch (marker.kind) {
            case ui::MinimapBlipKind::Boss:
                color[0] = 1.0F;
                color[1] = 0.55F;
                color[2] = 0.15F;
                size = 7.0F;
                break;
            case ui::MinimapBlipKind::Ally:
                color[0] = 0.95F;
                color[1] = 0.82F;
                color[2] = 0.35F;
                size = 5.0F;
                break;
            case ui::MinimapBlipKind::Loot:
                color[0] = 0.35F;
                color[1] = 0.85F;
                color[2] = 0.95F;
                size = 4.0F;
                break;
            case ui::MinimapBlipKind::Landmark:
                color[0] = 0.62F;
                color[1] = 0.55F;
                color[2] = 0.42F;
                size = 3.0F;
                break;
            case ui::MinimapBlipKind::Hostile:
                break;
            }
            uiRenderer.drawFilledRect(
                marker.pixel.x - size * 0.5F, marker.pixel.y - size * 0.5F, size, size, color);
        };
        for (const ui::MinimapMarker& marker : layer.entities) {
            drawBlip(marker);
        }

        const float playerDot[4] = {0.35F, 0.7F, 1.0F, 1.0F};
        uiRenderer.drawFilledRect(layer.player.pixel.x - 3.0F, layer.player.pixel.y - 3.0F, 6.0F, 6.0F, playerDot);
    }

    void renderFpsLabel() const {
        const ui::UiScale layout = currentUiScale();
        const float color[4] = {0.95F, 0.86F, 0.55F, 0.9F};
        textRenderer.drawText(layout.dim(12.0F), layout.dim(8.0F), fpsLabel_, layout.dim(1.5F), color);
    }

    static void rarityColors(
        const systems::ItemRarity rarity,
        float fill[4],
        float border[4]) noexcept {
        const systems::RarityColor tint = systems::rarityColor(rarity);
        fill[0] = tint.red * 0.32F;
        fill[1] = tint.green * 0.32F;
        fill[2] = tint.blue * 0.32F;
        fill[3] = 1.0F;
        border[0] = tint.red;
        border[1] = tint.green;
        border[2] = tint.blue;
        border[3] = 1.0F;
    }

    void renderInventoryOverlay() {
        if (!overlayState.inventoryOverlay().visible) {
            return;
        }

        const ui::InventoryPaperDollLayout layout = buildInventoryPaperDollLayout();
        const ui::Rect& panel = layout.panel;
        drawRpgPanel(panel);
        const float titleFill[4] = {0.42F, 0.08F, 0.09F, 0.92F};
        uiRenderer.drawFilledRect(
            panel.x + currentUiScale().dim(8.0F),
            panel.y + currentUiScale().dim(6.0F),
            std::max(currentUiScale().dim(40.0F), panel.width - layout.statsSidebarWidth - currentUiScale().dim(24.0F)),
            std::max(currentUiScale().dim(16.0F), layout.titleBandHeight - currentUiScale().dim(8.0F)),
            titleFill);

        const float sidebarFill[4] = {0.08F, 0.05F, 0.03F, 0.94F};
        uiRenderer.drawFilledRect(
            layout.statsSidebar.x,
            layout.statsSidebar.y,
            layout.statsSidebar.width,
            layout.statsSidebar.height,
            sidebarFill);
        drawRpgFrame(layout.statsSidebar, currentUiScale().dim(4.0F));

        const float dividerColor[4] = {0.55F, 0.45F, 0.2F, 0.85F};
        uiRenderer.drawFilledRect(
            layout.bagDivider.x,
            layout.bagDivider.y,
            layout.bagDivider.width,
            layout.bagDivider.height,
            dividerColor);

        if (uiAssets.isLoaded()) {
            const float portraitFrame[4] = {1.0F, 1.0F, 1.0F, 0.95F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                layout.portrait.x - currentUiScale().dim(6.0F),
                layout.portrait.y - currentUiScale().dim(6.0F),
                layout.portrait.width + currentUiScale().dim(12.0F),
                layout.portrait.height + currentUiScale().dim(12.0F),
                currentUiScale().dim(10.0F),
                portraitFrame);
        }

        drawPortraitInRect(layout.portrait);
        drawResourceBars(layout.hpBar, layout.xpBar);

        for (int equipmentIndex = 0;
             equipmentIndex < static_cast<int>(systems::EquipmentSlotKind::Count);
             ++equipmentIndex) {
            const ui::Rect equipSlot = layout.equipmentSlotRect(equipmentIndex);
            const bool equipHovered = hoveredEquipmentSlot_.has_value() &&
                                      *hoveredEquipmentSlot_ == equipmentIndex;
            const auto equipmentKind = static_cast<systems::EquipmentSlotKind>(equipmentIndex);
            drawEquipmentSlot(equipSlot, equipmentKind, equipHovered);
        }

        for (int index = 0; index < layout.bagColumns * layout.bagRows; ++index) {
            const ui::Rect slot = layout.inventorySlotRect(index);
            const bool hovered = hoveredInventorySlot_.has_value() && *hoveredInventorySlot_ == index;

            drawItemWell(slot, hovered);

            if (!playerInventory.isSlotOccupied(index)) {
                continue;
            }

            const systems::ItemMetadata& item = *playerInventory.slotAt(index).item;
            float iconFill[4]{};
            float iconBorder[4]{};
            rarityColors(item.rarity, iconFill, iconBorder);

            const float iconPad = 4.0F;
            if (!hasItemIcon(item)) {
                uiRenderer.drawFilledRect(
                    slot.x + iconPad,
                    slot.y + iconPad,
                    slot.width - iconPad * 2.0F,
                    slot.height - iconPad * 2.0F,
                    iconFill);
            }
            uiRenderer.drawOutlineRect(
                slot.x + iconPad,
                slot.y + iconPad,
                slot.width - iconPad * 2.0F,
                slot.height - iconPad * 2.0F,
                iconBorder);
            drawItemIcon(slot, item);
        }
    }

    void renderInventoryOverlayText() const {
        if (!overlayState.inventoryOverlay().visible) {
            return;
        }

        const ui::UiScale layoutScale = currentUiScale();
        const ui::InventoryPaperDollLayout layout = buildInventoryPaperDollLayout();
        const ui::CharacterScreenData& base = overlayState.characterScreen();

        const float titleColor[4] = {0.95F, 0.9F, 0.7F, 1.0F};
        const float subtitleColor[4] = {0.72F, 0.76F, 0.84F, 0.95F};
        const float hintColor[4] = {0.55F, 0.58F, 0.64F, 0.85F};

        std::ostringstream title;
        title << "CHARACTER   " << characterClassName(selectedClass) << "  Lv " << base.level;
        const ui::Rect titleBounds{
            layout.panel.x + layoutScale.dim(12.0F),
            layout.panel.y + layoutScale.dim(6.0F),
            layout.panel.width - layout.statsSidebarWidth - layoutScale.dim(28.0F),
            layout.titleBandHeight - layoutScale.dim(8.0F)};
        drawBoundedText(titleBounds, title.str(), layoutScale.dim(1.95F), titleColor);

        const ui::Rect hintBounds{
            layout.panel.x + layoutScale.dim(12.0F),
            layout.panel.y + layout.panel.height - layoutScale.dim(22.0F),
            layout.panel.width - layoutScale.dim(24.0F),
            layoutScale.dim(16.0F)};
        drawBoundedText(
            hintBounds,
            "Click gear to unequip | Click bag items to equip | C abilities | Esc close",
            layoutScale.dim(1.05F),
            hintColor);

        const ui::Rect sidebarTitle{
            layout.statsSidebar.x + layoutScale.dim(8.0F),
            layout.statsSidebar.y + layoutScale.dim(4.0F),
            layout.statsSidebar.width - layoutScale.dim(16.0F),
            layoutScale.dim(22.0F)};
        drawBoundedText(sidebarTitle, "Attributes", layoutScale.dim(1.45F), subtitleColor);

        drawResourceBarLabels(layout.hpBar, layout.xpBar);

        const float goldColor[4] = {0.95F, 0.82F, 0.28F, 1.0F};
        std::ostringstream goldLine;
        goldLine << "Gold " << ui::formatGroupedNumber(tradeSystem.playerGold());
        drawBoundedText(layout.goldLabel, goldLine.str(), layoutScale.dim(1.35F), goldColor);

        const std::vector<std::string> statLines = buildCharacterStatLines(true);
        float statY = layout.goldLabel.y + layout.goldLabel.height + layoutScale.dim(8.0F);
        const float statLineHeight = layoutScale.dim(16.0F);
        const float statBottom = layout.statsSidebar.y + layout.statsSidebar.height - layoutScale.dim(6.0F);
        const float statColor[4] = {0.78F, 0.82F, 0.9F, 1.0F};
        const float statScale = layoutScale.dim(1.22F);
        for (const std::string& line : statLines) {
            if (statY + statLineHeight > statBottom) {
                break;
            }
            const ui::Rect row{
                layout.statsSidebar.x + layoutScale.dim(8.0F),
                statY,
                layout.statsSidebar.width - layoutScale.dim(16.0F),
                statLineHeight};
            drawBoundedText(row, line, statScale, statColor);
            statY += statLineHeight;
        }

        const ui::Rect bagCount{
            layout.bagHeader.x,
            layout.bagHeader.y,
            layout.bagHeader.width,
            layout.bagHeader.height};
        std::ostringstream bagTitle;
        bagTitle << "Bag " << playerInventory.usedSlots() << "/" << playerInventory.capacity();
        drawBoundedText(bagCount, bagTitle.str(), layoutScale.dim(1.35F), subtitleColor);

        for (int equipmentIndex = 0;
             equipmentIndex < static_cast<int>(systems::EquipmentSlotKind::Count);
             ++equipmentIndex) {
            const ui::Rect equipSlot = layout.equipmentSlotRect(equipmentIndex);
            const auto equipmentKind = static_cast<systems::EquipmentSlotKind>(equipmentIndex);
            drawEquipmentSlotLetter(equipSlot, equipmentKind);
        }

        for (int index = 0; index < layout.bagColumns * layout.bagRows; ++index) {
            if (!playerInventory.isSlotOccupied(index)) {
                continue;
            }

            const ui::Rect slot = layout.inventorySlotRect(index);
            if (slotLetterOverlapsTooltip(slot)) {
                continue;
            }

            const systems::ItemMetadata& item = *playerInventory.slotAt(index).item;
            if (hasItemIcon(item)) {
                continue;
            }
            const char iconLetter[2] = {item.iconLetter, '\0'};
            const float iconTextColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            const float letterScale = layoutScale.dim(2.4F);
            const float letterWidth = textRenderer.measureTextWidth(iconLetter, letterScale);
            textRenderer.drawText(
                slot.x + slot.width * 0.5F - letterWidth * 0.5F,
                slot.y + slot.height * 0.5F - layoutScale.dim(10.0F),
                iconLetter,
                letterScale,
                iconTextColor);
        }
    }

    void tooltipToneColor(
        const systems::TooltipTone tone,
        const systems::ItemRarity rarity,
        const std::string& text,
        float color[4]) const {
        const systems::RarityColor tint = systems::rarityColor(rarity);
        color[3] = 1.0F;
        switch (tone) {
        case systems::TooltipTone::Title:
            color[0] = tint.red;
            color[1] = tint.green;
            color[2] = tint.blue;
            break;
        case systems::TooltipTone::Headline:
            color[0] = 0.96F;
            color[1] = 0.94F;
            color[2] = 0.86F;
            break;
        case systems::TooltipTone::Attribute:
            color[0] = 0.45F;
            color[1] = 0.66F;
            color[2] = 1.0F;
            break;
        case systems::TooltipTone::Effect:
            color[0] = 1.0F;
            color[1] = 0.62F;
            color[2] = 0.22F;
            break;
        case systems::TooltipTone::Footer:
            color[0] = 0.92F;
            color[1] = 0.78F;
            color[2] = 0.32F;
            break;
        case systems::TooltipTone::Compare:
            if (!text.empty() && text.front() == '-') {
                color[0] = 0.95F;
                color[1] = 0.35F;
                color[2] = 0.32F;
            } else {
                color[0] = 0.4F;
                color[1] = 0.9F;
                color[2] = 0.48F;
            }
            break;
        case systems::TooltipTone::Meta:
        default:
            color[0] = 0.78F;
            color[1] = 0.74F;
            color[2] = 0.64F;
            break;
        }
    }

    void drawItemCardBackground(const CachedItemCard& card) const {
        const systems::RarityColor tint = systems::rarityColor(card.rarity);
        const float border[4] = {tint.red, tint.green, tint.blue, 1.0F};
        drawTooltipBackground(card.layout, border);
    }

    void drawItemCardText(const CachedItemCard& card) const {
        const float lineWidth = card.layout.box.width - card.layout.contentInsetX * 2.0F;
        float cursorY = card.layout.box.y + card.layout.contentInsetY;
        for (const systems::TooltipLine& line : card.lines) {
            float color[4]{};
            tooltipToneColor(line.tone, card.rarity, line.text, color);
            const ui::Rect row{
                card.layout.box.x + card.layout.contentInsetX,
                cursorY,
                lineWidth,
                card.layout.lineHeight};
            drawBoundedText(row, line.text, card.scale, color);
            cursorY += card.layout.lineHeight + card.layout.lineGap;
        }
    }

    [[nodiscard]] static std::vector<std::string> tooltipTexts(const systems::ItemTooltipCard& card) {
        std::vector<std::string> lines;
        lines.reserve(card.lines.size());
        for (const systems::TooltipLine& line : card.lines) {
            lines.push_back(line.text);
        }
        return lines;
    }

    void renderItemTooltipBackground() {
        cachedItemTooltip_.reset();
        cachedItemCard_.reset();
        cachedCompareCard_.reset();

        std::optional<systems::ItemMetadata> hoveredItem;
        float anchorX = 0.0F;
        float anchorY = 0.0F;

        if (hoveredEquipmentSlot_.has_value()) {
            const auto slotKind =
                static_cast<systems::EquipmentSlotKind>(*hoveredEquipmentSlot_);

            if (overlayState.inventoryOverlay().visible) {
                const ui::InventoryPaperDollLayout layout = buildInventoryPaperDollLayout();
                const ui::Rect slot = layout.equipmentSlotRect(*hoveredEquipmentSlot_);
                anchorX = slot.x + slot.width;
                anchorY = slot.y;

                if (playerEquipment.isSlotOccupied(slotKind)) {
                    hoveredItem = *playerEquipment.itemAt(slotKind);
                } else {
                    const std::string slotHint =
                        std::string(systems::Equipment::slotLabel(slotKind)) + " slot (empty)";
                    std::vector<std::string> lines = {slotHint, "Click bag item to equip"};
                    constexpr float kTooltipScale = 1.85F;
                    const ui::TooltipBoxLayout tooltipLayout =
                        computeItemTooltipLayout(anchorX, anchorY, lines, kTooltipScale);
                    drawTooltipBackground(tooltipLayout);
                    cachedItemTooltip_ = CachedTooltip{tooltipLayout, std::move(lines), kTooltipScale};
                    return;
                }
            } else {
                return;
            }
        } else if (
            hoveredInventorySlot_.has_value() &&
            playerInventory.isSlotOccupied(*hoveredInventorySlot_)) {
            hoveredItem = *playerInventory.slotAt(*hoveredInventorySlot_).item;
            const ui::InventoryPaperDollLayout inventoryLayout = buildInventoryPaperDollLayout();
            const ui::Rect slot = inventoryLayout.inventorySlotRect(*hoveredInventorySlot_);
            anchorX = slot.x + slot.width;
            anchorY = slot.y;
        } else if (
            stateManager.currentState() == gameplay::GameState::TRADING &&
            hoveredTradePlayerSlot_.has_value() &&
            playerInventory.isSlotOccupied(*hoveredTradePlayerSlot_)) {
            hoveredItem = *playerInventory.slotAt(*hoveredTradePlayerSlot_).item;
            const ui::TradeWindowLayout tradeLayout = ui::computeTradeWindowLayout(currentUiScale());
            const ui::Rect slot = tradeLayout.playerSlotRect(*hoveredTradePlayerSlot_);
            anchorX = slot.x + slot.width;
            anchorY = slot.y;
        } else if (
            stateManager.currentState() == gameplay::GameState::TRADING &&
            hoveredTradeVendorSlot_.has_value() &&
            vendorInventory.isSlotOccupied(*hoveredTradeVendorSlot_)) {
            hoveredItem = *vendorInventory.slotAt(*hoveredTradeVendorSlot_).item;
            const ui::TradeWindowLayout tradeLayout = ui::computeTradeWindowLayout(currentUiScale());
            const ui::Rect slot = tradeLayout.vendorSlotRect(*hoveredTradeVendorSlot_);
            anchorX = slot.x + slot.width;
            anchorY = slot.y;
        } else {
            return;
        }

        const bool equippedSlot = hoveredEquipmentSlot_.has_value();
        systems::ItemTooltipCard candidate =
            systems::buildItemTooltipCard(*hoveredItem, equippedSlot ? "Equipped" : nullptr);
        systems::ItemTooltipCard equippedCard{};
        bool showEquipped = false;

        if (!equippedSlot && systems::Equipment::isEquippableCategory(hoveredItem->category)) {
            const systems::EquipmentSlotKind targetSlot =
                systems::Equipment::resolveEquipSlot(hoveredItem->category, playerEquipment);
            if (playerEquipment.isSlotOccupied(targetSlot)) {
                equippedCard = systems::buildItemTooltipCard(*playerEquipment.itemAt(targetSlot), "Equipped");
                showEquipped = true;
                const std::vector<std::string> comparison =
                    systems::formatItemComparisonLines(*hoveredItem, *playerEquipment.itemAt(targetSlot));
                for (const std::string& line : comparison) {
                    candidate.lines.push_back(systems::TooltipLine{line, systems::TooltipTone::Compare});
                }
            } else {
                candidate.lines.push_back(systems::TooltipLine{
                    std::string("(empty ") + systems::Equipment::slotLabel(targetSlot) + " slot)",
                    systems::TooltipTone::Meta});
            }
        }

        if (stateManager.currentState() == gameplay::GameState::TRADING) {
            if (hoveredTradePlayerSlot_.has_value()) {
                candidate.lines.push_back(systems::TooltipLine{
                    "Click to sell for " + std::to_string(systems::blacksmithSellPrice(*hoveredItem)) + " gold",
                    systems::TooltipTone::Footer});
            } else if (hoveredTradeVendorSlot_.has_value()) {
                candidate.lines.push_back(systems::TooltipLine{
                    "Click to buy for " + std::to_string(systems::blacksmithBuyPrice(*hoveredItem)) + " gold",
                    systems::TooltipTone::Footer});
            }
        }

        constexpr float kTooltipScale = 1.7F;
        const bool inventoryOpen = overlayState.inventoryOverlay().visible;
        const ui::InventoryPaperDollLayout inventoryLayout =
            inventoryOpen ? buildInventoryPaperDollLayout() : ui::InventoryPaperDollLayout{};
        const ui::Rect* avoidSidebar = inventoryOpen ? &inventoryLayout.statsSidebar : nullptr;
        const ui::ItemCompareCards placed = ui::placeItemCompareCards(
            currentUiScale(),
            anchorX,
            anchorY,
            tooltipTexts(candidate),
            showEquipped ? tooltipTexts(equippedCard) : std::vector<std::string>{},
            kTooltipScale,
            textMeasureFn(),
            window.width(),
            window.height(),
            inventoryOpen,
            avoidSidebar);

        cachedItemCard_ = CachedItemCard{placed.candidate, std::move(candidate.lines), kTooltipScale, hoveredItem->rarity};
        drawItemCardBackground(*cachedItemCard_);
        if (showEquipped) {
            cachedCompareCard_ =
                CachedItemCard{placed.equipped, std::move(equippedCard.lines), kTooltipScale, equippedCard.rarity};
            drawItemCardBackground(*cachedCompareCard_);
        }

        const float iconSize = currentUiScale().dim(36.0F);
        const ui::Rect iconRect{
            placed.candidate.box.x - iconSize - currentUiScale().dim(8.0F),
            placed.candidate.box.y,
            iconSize,
            iconSize};
        float iconFill[4]{};
        float iconBorder[4]{};
        rarityColors(hoveredItem->rarity, iconFill, iconBorder);
        uiRenderer.drawFilledRect(iconRect.x, iconRect.y, iconRect.width, iconRect.height, iconFill);
        uiRenderer.drawOutlineRect(iconRect.x, iconRect.y, iconRect.width, iconRect.height, iconBorder, 2.0F);
        drawItemIcon(iconRect, *hoveredItem);
    }

    void renderItemTooltipText() const {
        if (cachedItemTooltip_.has_value()) {
            drawTextTooltipTextOnly(
                cachedItemTooltip_->layout, cachedItemTooltip_->lines, cachedItemTooltip_->scale);
        }
        if (cachedCompareCard_.has_value()) {
            drawItemCardText(*cachedCompareCard_);
        }
        if (cachedItemCard_.has_value()) {
            drawItemCardText(*cachedItemCard_);
        }
    }

    void drawTalentNode(const ui::Rect& button, const float tint[4], const bool hovered) const {
        const float cx = button.x + button.width * 0.5F;
        const float cy = button.y + button.height * 0.5F;
        const float radius = std::min(button.width, button.height) * 0.42F;
        const float rim[4] = {0.86F, 0.68F, 0.28F, hovered ? 1.0F : 0.85F};
        const float core[4] = {tint[0], tint[1], tint[2], hovered ? 1.0F : 0.9F};
        uiRenderer.drawFilledCircle(cx, cy, radius + currentUiScale().dim(3.0F), rim, 22);
        uiRenderer.drawFilledCircle(cx, cy, radius, core, 22);
    }

    void renderCharacterScreen() {
        if (!overlayState.characterScreen().visible) {
            return;
        }

        const ui::UiScale scale = currentUiScale();
        const ui::CharacterPanelLayout panel = ui::computeCharacterPanelLayout(scale);
        drawRpgPanel(panel.panel);
        const float titleFill[4] = {0.42F, 0.08F, 0.09F, 0.95F};
        uiRenderer.drawFilledRect(
            panel.titleBand.x, panel.titleBand.y, panel.titleBand.width, panel.titleBand.height, titleFill);

        const float parchment[4] = {0.62F, 0.48F, 0.30F, 0.96F};
        if (!drawGeneratedFrame("parchment", panel.spellsPane)) {
            uiRenderer.drawFilledRect(
                panel.spellsPane.x, panel.spellsPane.y, panel.spellsPane.width, panel.spellsPane.height, parchment);
            drawRpgFrame(panel.spellsPane, scale.dim(5.0F));
        }

        const float nebula[4] = {0.03F, 0.03F, 0.08F, 0.94F};
        if (!drawGeneratedFrame("nebula", panel.talentsPane)) {
            uiRenderer.drawFilledRect(
                panel.talentsPane.x, panel.talentsPane.y, panel.talentsPane.width, panel.talentsPane.height, nebula);
            const float redWash[4] = {0.55F, 0.08F, 0.05F, 0.28F};
            const float blueWash[4] = {0.08F, 0.16F, 0.55F, 0.38F};
            uiRenderer.drawFilledRect(
                panel.talentsPane.x,
                panel.talentsPane.y,
                panel.talentsPane.width * 0.48F,
                panel.talentsPane.height,
                redWash);
            uiRenderer.drawFilledRect(
                panel.talentsPane.x + panel.talentsPane.width * 0.48F,
                panel.talentsPane.y,
                panel.talentsPane.width * 0.52F,
                panel.talentsPane.height,
                blueWash);
        }

        drawPortraitInRect(panel.portrait);
        drawResourceBars(panel.hpBar, panel.xpBar);

        const ui::AbilityBoardLayout board = ui::computeAbilityBoardLayout(panel, scale);
        const systems::SkillId basicIds[] = {
            systems::SkillId::PowerStrike, systems::SkillId::Cleave, systems::SkillId::Slam};
        const systems::SkillId strongIds[] = {systems::SkillId::Whirlwind, systems::SkillId::Firebolt};
        const systems::SkillId specialtyIds[] = {
            systems::SkillId::Heal, systems::SkillId::Dash, systems::SkillId::Shout};
        const auto paintSection = [&](const ui::AbilitySpellLayout& section, const systems::SkillId* ids) {
            for (int index = 0; index < section.iconCount; ++index) {
                const systems::SkillDefinition& skill = systems::skillDefinition(ids[index]);
                const ui::Rect& icon = section.icons[static_cast<std::size_t>(index)];
                const float fill[4] = {skill.colorR * 0.35F, skill.colorG * 0.35F, skill.colorB * 0.35F, 0.95F};
                uiRenderer.drawFilledRect(icon.x, icon.y, icon.width, icon.height, fill);
                drawRpgFrame(icon, scale.dim(4.0F));
                const char* iconName = skillIconFrame(skill.id);
                if (iconName != nullptr) {
                    const float inset = icon.width * 0.08F;
                    const ui::Rect glyph{icon.x + inset, icon.y + inset, icon.width - inset * 2.0F, icon.height - inset * 2.0F};
                    drawGeneratedFrame(iconName, glyph);
                }
            }
        };
        paintSection(board.basic, basicIds);
        paintSection(board.strong, strongIds);
        paintSection(board.specialties, specialtyIds);

        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const bool inTown = zoneManager.activeZone() == gameplay::WorldZone::TOWN;
        const int upgradeCost = systems::soulUpgradeCost(base.statUpgradesPurchased);
        const bool canUpgrade = inTown && base.carriedSouls >= upgradeCost;
        const ui::Rect* nodes[] = {
            &panel.upgradeStrengthButton, &panel.upgradeDexterityButton, &panel.upgradeVitalityButton};
        const float nodeTints[3][4] = {
            {0.85F, 0.28F, 0.18F, 1.0F},
            {0.25F, 0.55F, 0.95F, 1.0F},
            {0.25F, 0.78F, 0.38F, 1.0F},
        };
        const float link[4] = {0.85F, 0.68F, 0.28F, 0.55F};
        for (int index = 0; index < 3; ++index) {
            const ui::Rect& button = *nodes[index];
            const float cx = button.x + button.width * 0.5F;
            const float top = panel.upgradeHeader.y + panel.upgradeHeader.height + scale.dim(8.0F);
            uiRenderer.drawFilledRect(cx - scale.dim(1.5F), top, scale.dim(3.0F), std::max(1.0F, button.y - top), link);
            const bool hovered = hoveredStatUpgradeButton_.has_value() && *hoveredStatUpgradeButton_ == index;
            drawPanelButtonBackground(button, hovered, canUpgrade);
            drawTalentNode(button, nodeTints[index], hovered);
        }
    }

    void renderCharacterScreenText() const {
        if (!overlayState.characterScreen().visible) {
            return;
        }

        const ui::UiScale scale = currentUiScale();
        const ui::CharacterPanelLayout panel = ui::computeCharacterPanelLayout(scale);
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const bool inTown = zoneManager.activeZone() == gameplay::WorldZone::TOWN;
        const int upgradeCost = systems::soulUpgradeCost(base.statUpgradesPurchased);
        const float titleColor[4] = {0.98F, 0.9F, 0.62F, 1.0F};
        const float ink[4] = {0.22F, 0.12F, 0.06F, 1.0F};
        const float hintColor[4] = {0.72F, 0.68F, 0.58F, 0.9F};
        const float headerColor[4] = {0.78F, 0.86F, 1.0F, 1.0F};

        drawBoundedText(panel.titleBand, "ABILITIES", panel.titleScale, titleColor);

        const ui::Rect spellsTitle{
            panel.portrait.x + panel.portrait.width + scale.dim(8.0F),
            panel.portrait.y,
            std::max(scale.dim(40.0F), panel.spellsPane.x + panel.spellsPane.width - panel.portrait.x - panel.portrait.width - scale.dim(12.0F)),
            scale.dim(22.0F)};
        drawBoundedText(spellsTitle, "SPELLS", panel.bodyScale, ink);
        drawResourceBarLabels(panel.hpBar, panel.xpBar);

        const float goldColor[4] = {0.35F, 0.22F, 0.08F, 1.0F};
        std::ostringstream goldLine;
        goldLine << "Gold " << ui::formatGroupedNumber(tradeSystem.playerGold());
        drawBoundedText(panel.goldLabel, goldLine.str(), panel.statLabelScale, goldColor);

        const ui::AbilityBoardLayout board = ui::computeAbilityBoardLayout(panel, scale);
        const systems::SkillId basicIds[] = {
            systems::SkillId::PowerStrike, systems::SkillId::Cleave, systems::SkillId::Slam};
        const systems::SkillId strongIds[] = {systems::SkillId::Whirlwind, systems::SkillId::Firebolt};
        const systems::SkillId specialtyIds[] = {
            systems::SkillId::Heal, systems::SkillId::Dash, systems::SkillId::Shout};
        const auto labelSection = [&](const ui::AbilitySpellLayout& section, const char* title, const systems::SkillId* ids) {
            drawBoundedText(section.header, title, panel.statLabelScale, ink);
            const float glyph[4] = {0.98F, 0.96F, 0.9F, 1.0F};
            for (int index = 0; index < section.iconCount; ++index) {
                const systems::SkillDefinition& skill = systems::skillDefinition(ids[index]);
                if (generatedFrameReady(skillIconFrame(skill.id))) {
                    continue;
                }
                const char letter[2] = {skill.glyph, '\0'};
                textRenderer.drawTextCentered(section.icons[static_cast<std::size_t>(index)], letter, panel.bodyScale, glyph);
            }
        };
        labelSection(board.basic, "BASIC ATTACKS", basicIds);
        labelSection(board.strong, "STRONG ATTACKS", strongIds);
        labelSection(board.specialties, "SPECIALTIES", specialtyIds);

        std::ostringstream upgradeHeader;
        upgradeHeader << "TALENTS   " << base.carriedSouls << "/" << upgradeCost;
        if (!inTown) {
            upgradeHeader << "  town";
        }
        drawBoundedText(panel.upgradeHeader, upgradeHeader.str(), panel.statLabelScale, headerColor);

        const ui::Rect* nodes[] = {
            &panel.upgradeStrengthButton, &panel.upgradeDexterityButton, &panel.upgradeVitalityButton};
        const char* names[] = {"STR", "DEX", "VIT"};
        const int values[] = {base.strength, base.dexterity, base.vitality};
        const float nodeText[4] = {0.98F, 0.96F, 0.9F, 1.0F};
        for (int index = 0; index < 3; ++index) {
            std::ostringstream label;
            label << names[index] << ' ' << values[index];
            drawBoundedText(*nodes[index], label.str(), panel.statLabelScale, nodeText);
        }

        std::ostringstream soulsLine;
        soulsLine << (inTown ? "Click a node to spend souls" : "Return to town to spend souls");
        drawBoundedText(panel.statsText, soulsLine.str(), panel.statLabelScale, headerColor);

        const char* footerHint = "Spells are on keys 1-8  |  I gear  |  Esc close";
        drawBoundedText(panel.footerHint, footerHint, panel.statLabelScale, hintColor);
    }

    void drawTradeItemSlot(
        const ui::Rect& slot,
        const systems::ItemMetadata& item,
        bool hovered) const {
        drawItemWell(slot, hovered);

        float iconFill[4]{};
        float iconBorder[4]{};
        rarityColors(item.rarity, iconFill, iconBorder);
        const float iconPad = 4.0F;
        if (!hasItemIcon(item)) {
            uiRenderer.drawFilledRect(
                slot.x + iconPad,
                slot.y + iconPad,
                slot.width - iconPad * 2.0F,
                slot.height - iconPad * 2.0F,
                iconFill);
        }
        uiRenderer.drawOutlineRect(
            slot.x + iconPad,
            slot.y + iconPad,
            slot.width - iconPad * 2.0F,
            slot.height - iconPad * 2.0F,
            iconBorder);
        drawItemIcon(slot, item);
    }

    void renderTradePanels() {
        if (stateManager.currentState() != gameplay::GameState::TRADING) {
            return;
        }

        const ui::TradeWindowLayout layout = ui::computeTradeWindowLayout(currentUiScale());
        drawRpgPanel(layout.playerPanel);
        drawRpgPanel(layout.vendorPanel);
        drawRpgPanel(layout.servicesPanel);

        const float plaque[4] = {0.32F, 0.1F, 0.06F, 0.92F};
        const auto titlePlaque = [&](const ui::Rect& title) {
            uiRenderer.drawFilledRect(title.x, title.y, title.width, title.height, plaque);
            drawRpgFrame(title, 3.0F);
        };
        titlePlaque(layout.playerTitle);
        titlePlaque(layout.vendorTitle);
        titlePlaque(layout.servicesTitle);

        for (int index = 0; index < playerInventory.capacity(); ++index) {
            const ui::Rect slot = layout.playerSlotRect(index);
            const bool hovered =
                hoveredTradePlayerSlot_.has_value() && *hoveredTradePlayerSlot_ == index;
            if (!playerInventory.isSlotOccupied(index)) {
                drawItemWell(slot, hovered);
                continue;
            }
            drawTradeItemSlot(slot, *playerInventory.slotAt(index).item, hovered);
        }

        for (int index = 0; index < vendorInventory.capacity(); ++index) {
            const ui::Rect slot = layout.vendorSlotRect(index);
            const bool hovered =
                hoveredTradeVendorSlot_.has_value() && *hoveredTradeVendorSlot_ == index;
            if (!vendorInventory.isSlotOccupied(index)) {
                drawItemWell(slot, hovered);
                continue;
            }
            drawTradeItemSlot(slot, *vendorInventory.slotAt(index).item, hovered);
        }

        const systems::BlacksmithUnlockState unlocks = blacksmithUnlockState();
        for (int serviceIndex = 0; serviceIndex < ui::TradeWindowLayout::kServiceCount; ++serviceIndex) {
            const auto service = static_cast<systems::BlacksmithServiceKind>(serviceIndex);
            const ui::Rect button = layout.serviceButtonRect(serviceIndex);
            const bool hovered =
                hoveredBlacksmithService_.has_value() && *hoveredBlacksmithService_ == serviceIndex;
            const bool unlocked = systems::isBlacksmithServiceUnlocked(service, unlocks);
            drawRpgButton(button, hovered, unlocked);
        }
    }

    void renderTradePanelsText() const {
        if (stateManager.currentState() != gameplay::GameState::TRADING) {
            return;
        }

        const ui::TradeWindowLayout layout = ui::computeTradeWindowLayout(currentUiScale());
        const float labelColor[4] = {0.96F, 0.9F, 0.72F, 1.0F};
        const float mutedColor[4] = {0.78F, 0.68F, 0.48F, 0.95F};
        drawBoundedText(layout.playerTitle, "Sell from Backpack", layout.titleScale, labelColor);
        drawBoundedText(layout.vendorTitle, "Buy Wares", layout.titleScale, labelColor);
        drawBoundedText(layout.servicesTitle, "Forge Services", layout.titleScale, labelColor);

        std::ostringstream playerGold;
        playerGold << "Gold: " << tradeSystem.playerGold() << "  |  click item to sell";
        drawBoundedText(layout.playerGoldLabel, playerGold.str(), layout.valueScale, labelColor);

        std::ostringstream vendorGold;
        vendorGold << "Stock: " << vendorInventory.usedSlots() << " items  |  click to buy";
        drawBoundedText(layout.vendorGoldLabel, vendorGold.str(), layout.valueScale, labelColor);

        const systems::BlacksmithUnlockState unlocks = blacksmithUnlockState();
        for (int serviceIndex = 0; serviceIndex < ui::TradeWindowLayout::kServiceCount; ++serviceIndex) {
            const auto service = static_cast<systems::BlacksmithServiceKind>(serviceIndex);
            const systems::BlacksmithServiceDescriptor descriptor =
                systems::serviceDescriptor(service);
            const ui::Rect button = layout.serviceButtonRect(serviceIndex);
            const bool unlocked = systems::isBlacksmithServiceUnlocked(service, unlocks);
            const float padX = std::max(4.0F, button.width * 0.06F);
            const float padY = std::max(3.0F, button.height * 0.08F);
            const ui::Rect nameRect{
                button.x + padX,
                button.y + padY,
                std::max(1.0F, button.width - padX * 2.0F),
                std::max(1.0F, button.height * 0.42F)};
            const ui::Rect hintRect{
                button.x + padX,
                nameRect.y + nameRect.height,
                nameRect.width,
                std::max(1.0F, button.y + button.height - padY - (nameRect.y + nameRect.height))};
            const float textColor[4] = {unlocked ? 0.98F : 0.62F, unlocked ? 0.9F : 0.55F, unlocked ? 0.62F : 0.4F, 1.0F};
            const std::string detail =
                unlocked ? std::string(descriptor.shortHint) : systems::blacksmithUnlockHint(service);
            drawBoundedText(nameRect, descriptor.label, layout.serviceScale, textColor);
            drawBoundedText(hintRect, detail, layout.serviceScale * 0.82F, unlocked ? labelColor : mutedColor);
        }

        for (int index = 0; index < playerInventory.capacity(); ++index) {
            if (!playerInventory.isSlotOccupied(index)) {
                continue;
            }
            const systems::ItemMetadata& item = *playerInventory.slotAt(index).item;
            const ui::Rect slot = layout.playerSlotRect(index);
            if (slotLetterOverlapsTooltip(slot)) {
                continue;
            }
            if (hasItemIcon(item)) {
                continue;
            }
            const char iconLetter[2] = {item.iconLetter, '\0'};
            const float iconTextColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            const float letterScale = currentUiScale().dim(2.0F);
            const float letterWidth = textRenderer.measureTextWidth(iconLetter, letterScale);
            textRenderer.drawText(
                slot.x + slot.width * 0.5F - letterWidth * 0.5F,
                slot.y + slot.height * 0.5F - currentUiScale().dim(8.0F),
                iconLetter,
                letterScale,
                iconTextColor);

            std::ostringstream sellPrice;
            sellPrice << systems::blacksmithSellPrice(item) << 'g';
            const float priceColor[4] = {0.95F, 0.82F, 0.35F, 0.95F};
            drawBoundedText(
                {slot.x, slot.y + slot.height - currentUiScale().dim(14.0F), slot.width, currentUiScale().dim(12.0F)},
                sellPrice.str(),
                currentUiScale().dim(1.05F),
                priceColor);
        }

        for (int index = 0; index < vendorInventory.capacity(); ++index) {
            if (!vendorInventory.isSlotOccupied(index)) {
                continue;
            }
            const systems::ItemMetadata& item = *vendorInventory.slotAt(index).item;
            const ui::Rect slot = layout.vendorSlotRect(index);
            if (slotLetterOverlapsTooltip(slot)) {
                continue;
            }
            if (hasItemIcon(item)) {
                continue;
            }
            const char iconLetter[2] = {item.iconLetter, '\0'};
            const float iconTextColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            const float letterScale = currentUiScale().dim(2.0F);
            const float letterWidth = textRenderer.measureTextWidth(iconLetter, letterScale);
            textRenderer.drawText(
                slot.x + slot.width * 0.5F - letterWidth * 0.5F,
                slot.y + slot.height * 0.5F - currentUiScale().dim(8.0F),
                iconLetter,
                letterScale,
                iconTextColor);

            std::ostringstream buyPrice;
            buyPrice << systems::blacksmithBuyPrice(item) << 'g';
            const float priceColor[4] = {0.95F, 0.82F, 0.35F, 0.95F};
            drawBoundedText(
                {slot.x, slot.y + slot.height - currentUiScale().dim(14.0F), slot.width, currentUiScale().dim(12.0F)},
                buyPrice.str(),
                currentUiScale().dim(1.05F),
                priceColor);
        }
    }

    [[nodiscard]] bool generatedFrameReady(const char* name) const noexcept {
        return name != nullptr && generatedUi_.isLoaded() && generatedUi_.uvFor(name).valid;
    }

    bool drawGeneratedFrame(const char* name, const ui::Rect& rect, const float alpha = 1.0F) const {
        if (!generatedUi_.isLoaded() || name == nullptr || rect.width < 1.0F || rect.height < 1.0F) {
            return false;
        }
        const render::UiFrameUv uv = generatedUi_.uvFor(name);
        if (!uv.valid) {
            return false;
        }
        const float tint[4] = {1.0F, 1.0F, 1.0F, alpha};
        uiRenderer.drawTexturedRectUV(
            generatedUi_.texture(), rect.x, rect.y, rect.width, rect.height, uv.u0, uv.v0, uv.u1, uv.v1, tint);
        return true;
    }

    /// Ornate atlas frame. Source border stays on the filigree corners; the center of the frame is empty.
    bool drawGeneratedNineSlice(
        const char* name,
        const ui::Rect& rect,
        const float sourceBorder,
        const float destBorder) const {
        if (!generatedUi_.isLoaded() || name == nullptr || sourceBorder < 1.0F || destBorder < 1.0F) {
            return false;
        }
        const render::UiFrameUv uv = generatedUi_.uvFor(name);
        const render::Texture& texture = generatedUi_.texture();
        if (!uv.valid || texture.width() <= 0 || texture.height() <= 0) {
            return false;
        }
        const float border = std::min(destBorder, std::min(rect.width, rect.height) * 0.45F);
        if (border < 1.0F) {
            return false;
        }
        const float texW = static_cast<float>(texture.width());
        const float texH = static_cast<float>(texture.height());
        const float du = sourceBorder / texW;
        const float dv = sourceBorder / texH;
        const float u0 = uv.u0;
        const float u1 = uv.u0 + du;
        const float u2 = uv.u1 - du;
        const float u3 = uv.u1;
        const float vTop = uv.v1;
        const float vUpper = uv.v1 - dv;
        const float vLower = uv.v0 + dv;
        const float vBot = uv.v0;
        const float x0 = rect.x;
        const float x1 = rect.x + border;
        const float x2 = rect.x + rect.width - border;
        const float y0 = rect.y;
        const float y1 = rect.y + border;
        const float y2 = rect.y + rect.height - border;
        const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        const auto piece = [&](const float x, const float y, const float w, const float h, const float ua, const float va, const float ub, const float vb) {
            if (w < 0.5F || h < 0.5F) {
                return;
            }
            uiRenderer.drawTexturedRectUV(texture, x, y, w, h, ua, va, ub, vb, white);
        };
        piece(x0, y0, border, border, u0, vUpper, u1, vTop);
        piece(x1, y0, std::max(0.0F, x2 - x1), border, u1, vUpper, u2, vTop);
        piece(x2, y0, border, border, u2, vUpper, u3, vTop);
        piece(x0, y1, border, std::max(0.0F, y2 - y1), u0, vLower, u1, vUpper);
        piece(x2, y1, border, std::max(0.0F, y2 - y1), u2, vLower, u3, vUpper);
        piece(x0, y2, border, border, u0, vBot, u1, vLower);
        piece(x1, y2, std::max(0.0F, x2 - x1), border, u1, vBot, u2, vLower);
        piece(x2, y2, border, border, u2, vBot, u3, vLower);
        return true;
    }

    bool drawOrnateWindow(const ui::Rect& rect) const {
        const float border = std::clamp(std::min(rect.width, rect.height) * 0.035F, 16.0F, 40.0F);
        const ui::Rect framed{
            rect.x - border * 0.42F,
            rect.y - border * 0.55F,
            rect.width + border * 0.84F,
            rect.height + border * 0.95F};
        return drawGeneratedNineSlice("inventory_panel", framed, 40.0F, border);
    }

    [[nodiscard]] int countBeltPotions() const {
        int count = 0;
        for (int index = 0; index < playerInventory.capacity(); ++index) {
            const systems::InventorySlot& slot = playerInventory.slotAt(index);
            if (slot.item.has_value() && slot.item->category == systems::ItemCategory::Consumable) {
                ++count;
            }
        }
        return count;
    }

    /// Shared metal / gold filigree. Border strips only, so icons underneath stay visible.
    void drawRpgFrame(const ui::Rect& rect, const float requestedLip = 0.0F) const {
        if (rect.width < 6.0F || rect.height < 6.0F) {
            return;
        }
        const float shortest = std::min(rect.width, rect.height);
        float lip = requestedLip > 0.5F ? requestedLip : shortest * 0.11F;
        lip = std::clamp(lip, 3.0F, currentUiScale().dim(12.0F));
        lip = std::min(lip, shortest * 0.28F);
        const float goldW = std::max(1.5F, lip * 0.36F);
        const float dark[4] = {0.08F, 0.045F, 0.025F, 1.0F};
        const float gold[4] = {0.86F, 0.62F, 0.22F, 1.0F};
        const float hi[4] = {0.98F, 0.88F, 0.52F, 1.0F};
        const float bronze[4] = {0.42F, 0.26F, 0.1F, 1.0F};

        const auto band = [&](const float x, const float y, const float w, const float h, const float color[4]) {
            if (w > 0.4F && h > 0.4F) {
                uiRenderer.drawFilledRect(x, y, w, h, color);
            }
        };
        band(rect.x, rect.y, rect.width, lip, dark);
        band(rect.x, rect.y + rect.height - lip, rect.width, lip, dark);
        band(rect.x, rect.y, lip, rect.height, dark);
        band(rect.x + rect.width - lip, rect.y, lip, rect.height, dark);
        band(rect.x + 1.0F, rect.y + 1.0F, rect.width - 2.0F, goldW, hi);
        band(rect.x + 1.0F, rect.y + rect.height - 1.0F - goldW, rect.width - 2.0F, goldW, gold);
        band(rect.x + 1.0F, rect.y, goldW, rect.height, hi);
        band(rect.x + rect.width - 1.0F - goldW, rect.y, goldW, rect.height, gold);

        const float inset = std::max(lip - 1.0F, goldW + 1.0F);
        band(rect.x + inset, rect.y + inset, rect.width - inset * 2.0F, 1.6F, gold);
        band(rect.x + inset, rect.y + rect.height - inset - 1.6F, rect.width - inset * 2.0F, 1.6F, bronze);
        band(rect.x + inset, rect.y + inset, 1.6F, rect.height - inset * 2.0F, gold);
        band(rect.x + rect.width - inset - 1.6F, rect.y + inset, 1.6F, rect.height - inset * 2.0F, bronze);

        if (shortest < 16.0F) {
            return;
        }
        const float stud = std::clamp(lip * 0.42F, 2.5F, 6.0F);
        const float corners[4][2] = {
            {rect.x + lip * 0.35F, rect.y + lip * 0.35F},
            {rect.x + rect.width - lip * 0.35F, rect.y + lip * 0.35F},
            {rect.x + lip * 0.35F, rect.y + rect.height - lip * 0.35F},
            {rect.x + rect.width - lip * 0.35F, rect.y + rect.height - lip * 0.35F},
        };
        for (const auto& corner : corners) {
            uiRenderer.drawFilledCircle(corner[0], corner[1], stud, hi, 8);
            uiRenderer.drawFilledCircle(corner[0], corner[1], stud * 0.45F, bronze, 6);
        }
    }

    void drawRpgPanel(const ui::Rect& rect) const {
        if (rect.width < 8.0F || rect.height < 8.0F) {
            return;
        }
        const float shadow[4] = {0.0F, 0.0F, 0.0F, 0.42F};
        uiRenderer.drawFilledRect(rect.x + 3.0F, rect.y + 4.0F, rect.width, rect.height, shadow);
        const float wood[4] = {0.11F, 0.065F, 0.038F, 0.96F};
        uiRenderer.drawFilledRect(rect.x, rect.y, rect.width, rect.height, wood);
        const float parchment[4] = {0.18F, 0.11F, 0.06F, 0.42F};
        const float pad = std::clamp(std::min(rect.width, rect.height) * 0.04F, 6.0F, 18.0F);
        if (rect.width > pad * 2.5F && rect.height > pad * 2.5F) {
            uiRenderer.drawFilledRect(
                rect.x + pad, rect.y + pad, rect.width - pad * 2.0F, rect.height - pad * 2.0F, parchment);
        }
        const float grain[4] = {0.28F, 0.16F, 0.07F, 0.18F};
        uiRenderer.drawFilledRect(rect.x + pad, rect.y + rect.height * 0.22F, std::max(1.0F, rect.width - pad * 2.0F), std::max(2.0F, rect.height * 0.015F), grain);
        drawRpgFrame(rect);
    }

    void drawRpgButton(const ui::Rect& rect, const bool hovered, const bool enabled) const {
        const float locked[4] = {0.10F, 0.08F, 0.07F, 0.94F};
        const float ready[4] = {0.20F, 0.11F, 0.055F, 0.96F};
        const float hot[4] = {0.34F, 0.18F, 0.07F, 1.0F};
        const float* fill = !enabled ? locked : (hovered ? hot : ready);
        uiRenderer.drawFilledRect(rect.x, rect.y, rect.width, rect.height, fill);
        if (enabled) {
            const float sheen[4] = {1.0F, 0.86F, 0.55F, hovered ? 0.16F : 0.08F};
            uiRenderer.drawFilledRect(rect.x, rect.y, rect.width, std::max(2.0F, rect.height * 0.18F), sheen);
        }
        drawRpgFrame(rect, currentUiScale().dim(4.0F));
    }

    void drawVitalBar(const ui::Rect& bar, const float ratio, const float liquid[4]) const {
        const float well[4] = {0.03F, 0.018F, 0.015F, 0.98F};
        uiRenderer.drawFilledRect(bar.x, bar.y, bar.width, bar.height, well);
        const float lip = std::min(bar.height * 0.22F, currentUiScale().dim(5.0F));
        const float innerW = std::max(0.0F, bar.width - lip * 2.0F);
        const float innerH = std::max(0.0F, bar.height - lip * 2.0F);
        const float clamped = std::clamp(ratio, 0.0F, 1.0F);
        if (clamped > 0.001F && innerW > 1.0F && innerH > 1.0F) {
            uiRenderer.drawFilledRect(bar.x + lip, bar.y + lip, innerW * clamped, innerH, liquid);
            const float sheen[4] = {1.0F, 0.92F, 0.78F, 0.30F};
            uiRenderer.drawFilledRect(
                bar.x + lip, bar.y + lip, innerW * clamped, std::max(1.0F, innerH * 0.32F), sheen);
        }
        drawRpgFrame(bar, std::max(3.0F, lip));
    }

    void drawItemWell(const ui::Rect& slot, const bool hovered) const {
        const float empty[4] = {hovered ? 0.16F : 0.07F, hovered ? 0.10F : 0.045F, 0.03F, 0.96F};
        uiRenderer.drawFilledRect(slot.x, slot.y, slot.width, slot.height, empty);
        drawRpgFrame(slot, std::max(3.0F, currentUiScale().dim(3.5F)));
    }

    void drawHudVignette(const ui::HudConsoleLayout& console) const {
        constexpr int kSteps = 6;
        const float slice = console.panel.height / static_cast<float>(kSteps);
        for (int step = 0; step < kSteps; ++step) {
            const float fade = static_cast<float>(step + 1) / static_cast<float>(kSteps);
            const float vignette[4] = {0.0F, 0.0F, 0.01F, 0.22F * fade * fade};
            uiRenderer.drawFilledRect(
                console.panel.x,
                console.panel.y + slice * static_cast<float>(step),
                console.panel.width,
                slice + 1.0F,
                vignette);
        }
    }

    void renderHudConsole() const {
        const ui::UiScale scale = currentUiScale();
        const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(scale);
        drawHudVignette(console);

        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const int maxHealth = std::max(effective.maxHealth, 1);
        const float healthRatio = static_cast<float>(playerCurrentHealth_) / static_cast<float>(maxHealth);
        const float manaRatio = skillBar_.manaRatio();
        const float healthLiquid[4] = {0.72F, 0.08F, 0.1F, 0.98F};
        const float manaLiquid[4] = {0.16F, 0.32F, 0.86F, 0.98F};
        drawVitalBar(console.healthBar, healthRatio, healthLiquid);
        drawVitalBar(console.manaBar, manaRatio, manaLiquid);

        const float badge[4] = {0.28F, 0.08F, 0.06F, 0.96F};
        uiRenderer.drawFilledRect(
            console.levelBadge.x, console.levelBadge.y, console.levelBadge.width, console.levelBadge.height, badge);
        drawRpgFrame(console.levelBadge, scale.dim(4.0F));

        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const int nextXp = std::max(base.experienceToNextLevel, 1);
        const float xpRatio = std::clamp(static_cast<float>(base.experience) / static_cast<float>(nextXp), 0.0F, 1.0F);
        const float xpEmpty[4] = {0.08F, 0.05F, 0.03F, 0.94F};
        const float xpFill[4] = {0.92F, 0.72F, 0.22F, 1.0F};
        uiRenderer.drawFilledRect(console.xpBar.x, console.xpBar.y, console.xpBar.width, console.xpBar.height, xpEmpty);
        if (xpRatio > 0.0F) {
            uiRenderer.drawFilledRect(
                console.xpBar.x, console.xpBar.y, console.xpBar.width * xpRatio, console.xpBar.height, xpFill);
        }
        drawRpgFrame(console.xpBar, std::max(2.0F, console.xpBar.height * 0.22F));

        const float cooldown[4] = {0.02F, 0.01F, 0.01F, 0.72F};
        const float keyBand = scale.dim(ui::kHudHotkeyBand);
        const float slotLip = scale.dim(6.0F);
        for (int slot = 0; slot < ui::HudConsoleLayout::kSkillSlotCount; ++slot) {
            const ui::Rect& bounds = console.skillSlots[static_cast<std::size_t>(slot)];
            const systems::SkillDefinition& skill = systems::skillDefinition(skillBar_.slot(slot));
            const float well[4] = {0.05F, 0.03F, 0.02F, 0.94F};
            uiRenderer.drawFilledRect(bounds.x, bounds.y, bounds.width, bounds.height, well);
            drawRpgFrame(bounds, slotLip);
            const ui::Rect glyph = ui::hudGlyphRect(bounds, keyBand);
            const float pad = std::max(glyph.width * 0.1F, slotLip * 0.35F);
            const float fill[4] = {skill.colorR * 0.45F, skill.colorG * 0.45F, skill.colorB * 0.45F, 0.9F};
            uiRenderer.drawFilledRect(glyph.x + pad, glyph.y + pad, glyph.width - pad * 2.0F, glyph.height - pad * 2.0F, fill);
            const char* iconName = skillIconFrame(skill.id);
            if (iconName != nullptr) {
                const ui::Rect icon{
                    glyph.x + pad, glyph.y + pad, glyph.width - pad * 2.0F, glyph.height - pad * 2.0F};
                drawGeneratedFrame(iconName, icon);
            }
            const float ratio = skillBar_.cooldownRatio(slot);
            if (ratio > 0.0F) {
                uiRenderer.drawRadialCooldown(
                    bounds.x + bounds.width * 0.5F,
                    bounds.y + bounds.height * 0.5F,
                    ui::hudCooldownRadius(bounds),
                    ratio,
                    cooldown);
            }
        }

        int beltIndex = 0;
        for (const ui::Rect& bounds : console.beltSlots) {
            const float well[4] = {0.05F, 0.03F, 0.02F, 0.94F};
            uiRenderer.drawFilledRect(bounds.x, bounds.y, bounds.width, bounds.height, well);
            drawRpgFrame(bounds, slotLip);
            if (beltIndex == 0) {
                const ui::Rect vialRect{
                    bounds.x + bounds.width * 0.16F,
                    bounds.y + bounds.height * 0.06F,
                    bounds.width * 0.68F,
                    bounds.height * 0.70F};
                if (!drawGeneratedFrame("potion_vial", vialRect)) {
                    const float vial[4] = {0.75F, 0.1F, 0.12F, 0.95F};
                    const float pad = bounds.width * 0.22F;
                    uiRenderer.drawFilledRect(
                        bounds.x + pad, bounds.y + pad, bounds.width - pad * 2.0F, bounds.height - pad * 2.0F, vial);
                    systems::ItemMetadata potion{};
                    potion.category = systems::ItemCategory::Consumable;
                    drawItemIcon(bounds, potion);
                }
            }
            ++beltIndex;
        }

        for (int index = 0; index < ui::HudConsoleLayout::kMenuIconCount; ++index) {
            const ui::Rect& icon = console.menuIcons[static_cast<std::size_t>(index)];
            const bool hovered = hoveredHudMenu_ == index;
            const float core[4] = {hovered ? 0.22F : 0.08F, hovered ? 0.12F : 0.045F, 0.03F, 0.96F};
            uiRenderer.drawFilledRect(icon.x, icon.y, icon.width, icon.height, core);
            drawRpgFrame(icon, scale.dim(4.0F));
            const float pad = icon.width * 0.16F;
            const ui::Rect glyph{icon.x + pad, icon.y + pad, icon.width - pad * 2.0F, icon.height - pad * 2.0F};
            if (!drawGeneratedFrame(menuIconFrame(index), glyph)) {
                const float rim[4] = {0.82F, 0.62F, 0.24F, hovered ? 1.0F : 0.7F};
                uiRenderer.drawFilledCircle(
                    glyph.x + glyph.width * 0.5F,
                    glyph.y + glyph.height * 0.5F,
                    std::min(glyph.width, glyph.height) * 0.36F,
                    rim,
                    12);
            }
        }
    }

    void renderHudConsoleLabels() const {
        const ui::UiScale scale = currentUiScale();
        const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(scale);
        const float text[4] = {0.96F, 0.94F, 0.88F, 1.0F};
        const float shadow[4] = {0.0F, 0.0F, 0.0F, 0.85F};

        const auto drawFittedCentered = [&](const ui::Rect& bounds, const std::string& value, float textScale) {
            const float measured = textRenderer.measureTextWidth(value.c_str(), textScale);
            if (measured > bounds.width && measured > 1.0F) {
                textScale *= bounds.width / measured;
            }
            const ui::Rect shadowBounds{bounds.x + 1.0F, bounds.y + 1.0F, bounds.width, bounds.height};
            textRenderer.drawTextCentered(shadowBounds, value.c_str(), textScale, shadow);
            textRenderer.drawTextCentered(bounds, value.c_str(), textScale, text);
        };

        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        std::ostringstream health;
        health << playerCurrentHealth_ << " / " << std::max(effective.maxHealth, 1);
        drawFittedCentered(console.healthLabel, health.str(), console.labelScale);

        std::ostringstream mana;
        mana << skillBar_.mana() << " / " << skillBar_.maxMana();
        drawFittedCentered(console.manaLabel, mana.str(), console.labelScale * 0.9F);

        const ui::CharacterScreenData& base = overlayState.characterScreen();
        std::ostringstream level;
        level << "Lv. " << base.level;
        const float measuredLevel = textRenderer.measureTextWidth(level.str().c_str(), console.hotkeyScale);
        float levelScale = console.hotkeyScale;
        if (measuredLevel > console.levelBadge.width && measuredLevel > 1.0F) {
            levelScale *= console.levelBadge.width / measuredLevel;
        }
        const float levelColor[4] = {0.98F, 0.86F, 0.42F, 1.0F};
        const ui::Rect levelShadow{
            console.levelBadge.x + 1.0F, console.levelBadge.y + 1.0F, console.levelBadge.width, console.levelBadge.height};
        textRenderer.drawTextCentered(levelShadow, level.str().c_str(), levelScale, shadow);
        textRenderer.drawTextCentered(console.levelBadge, level.str().c_str(), levelScale, levelColor);

        std::ostringstream souls;
        souls << "Souls " << base.carriedSouls << "   Depth " << runProgression_.depth();
        const float soulColor[4] = {0.9F, 0.78F, 0.4F, 0.9F};
        drawBoundedText(console.soulsLabel, souls.str(), console.hotkeyScale, soulColor);

        const float hotkeyColor[4] = {0.9F, 0.76F, 0.38F, 0.95F};
        const float keyBand = scale.dim(ui::kHudHotkeyBand);
        for (int slot = 0; slot < ui::HudConsoleLayout::kSkillSlotCount; ++slot) {
            const ui::Rect& bounds = console.skillSlots[static_cast<std::size_t>(slot)];
            const systems::SkillDefinition& skill = systems::skillDefinition(skillBar_.slot(slot));
            const char glyph[2] = {skill.glyph, '\0'};
            const float remaining = skillBar_.cooldownRemaining(slot);
            if (remaining >= 0.15F) {
                std::ostringstream seconds;
                seconds.setf(std::ios::fixed);
                seconds.precision(remaining >= 10.0F ? 0 : 1);
                seconds << remaining;
                const float coolText[4] = {1.0F, 0.92F, 0.75F, 1.0F};
                textRenderer.drawTextCentered(ui::hudGlyphRect(bounds, keyBand), seconds.str().c_str(), console.hotkeyScale, coolText);
            } else if (!generatedFrameReady(skillIconFrame(skill.id))) {
                textRenderer.drawTextCentered(ui::hudGlyphRect(bounds, keyBand), glyph, console.labelScale, text);
            }
            const char hotkey[2] = {static_cast<char>('1' + slot), '\0'};
            textRenderer.drawTextCentered(ui::hudHotkeyRect(bounds, keyBand), hotkey, console.hotkeyScale, hotkeyColor);
        }

        const int potions = countBeltPotions();
        std::ostringstream potionCount;
        potionCount << potions;
        const float potionColor[4] = {1.0F, 0.9F, 0.85F, 1.0F};
        const ui::Rect potionCountRect{
            console.beltSlots[0].x,
            console.beltSlots[0].y,
            console.beltSlots[0].width,
            keyBand};
        textRenderer.drawTextCentered(potionCountRect, potionCount.str().c_str(), console.hotkeyScale, potionColor);
        textRenderer.drawTextCentered(ui::hudHotkeyRect(console.beltSlots[0], keyBand), "Q", console.hotkeyScale, hotkeyColor);

        const char* menuGlyphs[ui::HudConsoleLayout::kMenuIconCount] = {"C", "I", "M", "S", "P"};
        for (int index = 0; index < ui::HudConsoleLayout::kMenuIconCount; ++index) {
            if (generatedFrameReady(menuIconFrame(index))) {
                continue;
            }
            const float menuColor[4] = {
                hoveredHudMenu_ == index ? 1.0F : 0.92F,
                hoveredHudMenu_ == index ? 0.86F : 0.74F,
                0.4F,
                1.0F};
            textRenderer.drawTextCentered(
                console.menuIcons[static_cast<std::size_t>(index)], menuGlyphs[index], console.labelScale, menuColor);
        }
    }

    [[nodiscard]] static float lootLabelLineHeight(const ui::UiScale& scale) noexcept {
        return scale.dim(16.0F);
    }

    [[nodiscard]] static float lootLabelTextScale(const ui::UiScale& scale, const float intensity) noexcept {
        return scale.dim(intensity >= 1.6F ? 1.55F : 1.35F);
    }

    [[nodiscard]] float lootLabelStackTop(
        const float screenY,
        const float beamHeightPx,
        const std::size_t labelCount,
        const ui::UiScale& scale) const {
        const float lineHeight = lootLabelLineHeight(scale);
        float cursorY =
            screenY - beamHeightPx - lineHeight * static_cast<float>(labelCount) - scale.dim(4.0F);
        // Keep plates readable above the HUD chrome / top strip instead of clamping into them.
        const float ceiling = scale.dim(40.0F);
        const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(scale);
        const float floor = console.messageStrip.y - lineHeight * static_cast<float>(std::max<std::size_t>(labelCount, 1)) -
            scale.dim(6.0F);
        return std::clamp(cursorY, ceiling, std::max(ceiling, floor));
    }

    void renderLootBeacons() const {
        if (lootPresentation_.beacons().empty() || worldReadoutsHidden()) {
            return;
        }

        const gameplay::CameraMatrices cameraMatrices = camera.matricesForTarget(cameraFocus());
        const ui::UiScale scale = currentUiScale();
        for (const ui::LootBeacon& beacon : lootPresentation_.beacons()) {
            float screenX = 0.0F;
            float screenY = 0.0F;
            const glm::vec3 anchor(beacon.x, 0.15F, beacon.z);
            if (!worldToScreen(
                    anchor,
                    cameraMatrices.view,
                    cameraMatrices.projection,
                    window.width(),
                    window.height(),
                    screenX,
                    screenY)) {
                continue;
            }

            const float fade = std::clamp(1.0F - beacon.ageSeconds / beacon.lifetimeSeconds, 0.0F, 1.0F);
            const float height = scale.dim(ui::LootPresentation::beamHeight(beacon.intensity));
            const ui::LootLabel& top = beacon.labels.front();
            const float baseWidth = scale.dim(14.0F + beacon.intensity * 7.0F);
            const float outline[4] = {0.0F, 0.0F, 0.0F, fade * 0.45F};
            uiRenderer.drawFilledRect(
                screenX - baseWidth * 0.7F, screenY - height, baseWidth * 1.4F, height, outline);
            constexpr int kSlices = 16;
            for (int slice = 0; slice < kSlices; ++slice) {
                const float t0 = static_cast<float>(slice) / static_cast<float>(kSlices);
                const float t1 = static_cast<float>(slice + 1) / static_cast<float>(kSlices);
                const float y = screenY - height * t1;
                const float band = height / static_cast<float>(kSlices) + 1.0F;
                const float width = baseWidth * (1.05F - 0.35F * t0);
                const float alpha = fade * (0.62F + 0.38F * (1.0F - t0));
                const float color[4] = {top.red, top.green, top.blue, alpha};
                uiRenderer.drawFilledRect(screenX - width * 0.5F, y, width, band, color);
            }
            const float coreWidth = std::max(3.0F, baseWidth * 0.22F);
            const float core[4] = {1.0F, 0.97F, 0.88F, fade * 0.9F};
            uiRenderer.drawFilledRect(screenX - coreWidth * 0.5F, screenY - height, coreWidth, height, core);
            const float glowWidth = baseWidth * 2.6F;
            const float glow[4] = {top.red, top.green, top.blue, fade * 0.55F};
            uiRenderer.drawFilledRect(screenX - glowWidth * 0.5F, screenY - scale.dim(4.0F), glowWidth, scale.dim(10.0F), glow);

            const float lineHeight = lootLabelLineHeight(scale);
            const float textScale = lootLabelTextScale(scale, beacon.intensity);
            float cursorY = lootLabelStackTop(screenY, height, beacon.labels.size(), scale);
            for (const ui::LootLabel& label : beacon.labels) {
                const float width = std::max(scale.dim(24.0F), textRenderer.measureTextWidth(label.name.c_str(), textScale));
                const float plate[4] = {0.0F, 0.0F, 0.0F, fade * 0.62F};
                uiRenderer.drawFilledRect(screenX - width * 0.5F - scale.dim(4.0F), cursorY - scale.dim(1.0F), width + scale.dim(8.0F), lineHeight, plate);
                cursorY += lineHeight;
            }
        }
    }

    void renderLootBeaconLabels() const {
        if (lootPresentation_.beacons().empty() || worldReadoutsHidden()) {
            return;
        }

        const gameplay::CameraMatrices cameraMatrices = camera.matricesForTarget(cameraFocus());
        const ui::UiScale scale = currentUiScale();
        const float lineHeight = lootLabelLineHeight(scale);
        for (const ui::LootBeacon& beacon : lootPresentation_.beacons()) {
            float screenX = 0.0F;
            float screenY = 0.0F;
            const glm::vec3 anchor(beacon.x, 0.15F, beacon.z);
            if (!worldToScreen(
                    anchor,
                    cameraMatrices.view,
                    cameraMatrices.projection,
                    window.width(),
                    window.height(),
                    screenX,
                    screenY)) {
                continue;
            }

            const float fade = std::clamp(1.0F - beacon.ageSeconds / beacon.lifetimeSeconds, 0.0F, 1.0F);
            const float height = scale.dim(ui::LootPresentation::beamHeight(beacon.intensity));
            const float textScale = lootLabelTextScale(scale, beacon.intensity);
            float cursorY = lootLabelStackTop(screenY, height, beacon.labels.size(), scale);
            for (const ui::LootLabel& label : beacon.labels) {
                const float width = textRenderer.measureTextWidth(label.name.c_str(), textScale);
                const float x = screenX - width * 0.5F;
                const float shadowColor[4] = {0.0F, 0.0F, 0.0F, fade * 0.9F};
                const float color[4] = {label.red, label.green, label.blue, fade};
                textRenderer.drawText(x + 1.0F, cursorY + 1.0F, label.name.c_str(), textScale, shadowColor);
                textRenderer.drawText(x, cursorY, label.name.c_str(), textScale, color);
                cursorY += lineHeight;
            }
        }
    }

    void renderDragGhost(const float mouseX, const float mouseY) const {
        if (!inventoryDrag_.active()) {
            return;
        }
        const float size = currentUiScale().dim(28.0F);
        const float ghost[4] = {0.85F, 0.7F, 0.28F, 0.85F};
        uiRenderer.drawFilledRect(mouseX - size * 0.5F, mouseY - size * 0.5F, size, size, ghost);
    }

    void renderInGameUi(float mouseX, float mouseY) {
        ensureUiHitRegionsBuilt();
        updateInventoryHover(mouseX, mouseY);
        updateTownHover(mouseX, mouseY);

        uiRenderer.beginFrame();
        renderTownScene();
        renderScreenFlash();
        renderLootBeacons();
        renderInventoryOverlay();
        renderCharacterScreen();
        renderTradePanels();
        renderHudConsole();
        renderHudInfoStrip();
        renderMobHealthBars();
        renderTargetMobHud();
        renderMinimap();
        renderCampaignMap();
        renderInteractableTooltipBackground(mouseX, mouseY);
        renderFloatingCombatTextBackgrounds();
        renderDragGhost(mouseX, mouseY);
        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        renderTownSceneText();
        renderHudConsoleLabels();
        renderLootBeaconLabels();
        renderHudInfoStripLabel();
        renderFpsLabel();
        renderLaneBanner();
        renderCampaignMapText();
        renderTargetMobHudLabel();
        renderMobNameplates();
        renderFloatingCombatTextLabels();
        renderInventoryOverlayText();
        renderCharacterScreenText();
        renderTradePanelsText();
        renderInteractableTooltipText();
        textRenderer.endOverlay();

        uiRenderer.beginFrame();
        renderItemTooltipBackground();
        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        renderItemTooltipText();
        textRenderer.endOverlay();
        syncGameplayPointer();
    }

    void update(float deltaSeconds) {
        noteFrameDelta(deltaSeconds);
        if (appScreen == AppScreen::MAIN_MENU) {
            handleMainMenuInput();
            return;
        }
        if (appScreen == AppScreen::CHARACTER_SELECT) {
            handleCharacterSelectInput();
            return;
        }
        if (appScreen == AppScreen::LOAD_CHARACTER) {
            handleLoadCharacterInput();
            return;
        }
        if (appScreen == AppScreen::SETTINGS) {
            handleSettingsInput();
            return;
        }

        ++frameIndex_;

        if (!gamePaused) {
            combatFeedback_.update(deltaSeconds);
            refreshManaPool();
            skillBar_.update(deltaSeconds);
        }
        lootPresentation_.update(deltaSeconds);
        // Hit-stop slows the simulated world for a few frames while input/UI stay responsive.
        const float simulationDelta = deltaSeconds * combatFeedback_.timeScale();

        handleInGameInput(simulationDelta);

        if (!gamePaused && zoneManager.activeZone() == gameplay::WorldZone::PLAINS) {
            if (!laneActive_) {
                zoneManager.updatePlainsSimulation(simulationDelta, toVec3(playerPosition));
            }
            updateMobMeleeThreats(simulationDelta);
        }

        overlayState.syncInventoryVisibility(playerInventory.usedSlots());
        if (!gamePaused) {
            updatePlayerSpriteAnimation(simulationDelta);
            updateEntityAnimations(simulationDelta);
            finalizeEntityMotionTracking();
            updateParticles(deltaSeconds);
        }
    }

    [[nodiscard]] float ambientDustDensity() const noexcept {
        switch (gameSettings.graphicsQuality) {
        case 0:
            return 0.0F;
        case 2:
            return 14.0F;
        case 1:
        default:
            return 6.0F;
        }
    }

    void updateParticles(const float deltaSeconds) {
        particles_.update(deltaSeconds);
        const float dustRadius = std::max(6.0F, effectiveCharacterStats().lightRadius * 1.1F);
        particles_.updateAmbientDust(playerPosition, dustRadius, deltaSeconds, ambientDustDensity());
    }

    void renderFrame() {
        float mouseX = 0.0F;
        float mouseY = 0.0F;
        readMousePosition(mouseX, mouseY);

        if (appScreen == AppScreen::MAIN_MENU) {
            pollHover(mouseX, mouseY, buildMainMenuButtons());
            renderMainMenu();
            return;
        }
        if (appScreen == AppScreen::CHARACTER_SELECT) {
            std::vector<MenuButton> buttons = buildClassButtons();
            buttons.push_back(buildBackButton(static_cast<float>(window.height()) * 0.78F));
            pollHover(mouseX, mouseY, buttons);
            renderCharacterSelect();
            return;
        }
        if (appScreen == AppScreen::LOAD_CHARACTER) {
            pollHover(mouseX, mouseY, buildLoadCharacterButtons());
            renderLoadCharacterScreen();
            return;
        }
        if (appScreen == AppScreen::SETTINGS) {
            pollHover(mouseX, mouseY, {buildBackButton(static_cast<float>(window.height()) * 0.72F)});
            renderSettings(true);
            return;
        }

        if (gamePaused) {
            if (pauseSettingsOpen) {
                pollHover(mouseX, mouseY, {buildBackButton(static_cast<float>(window.height()) * 0.72F)});
            } else {
                pollHover(mouseX, mouseY, buildPauseMenuButtons());
            }
        } else {
            updateInteractableHover(mouseX, mouseY);
        }

        {
            const auto worldStart = std::chrono::steady_clock::now();
            renderWorld();
            engine::FrameProbe::instance().addWorldMs(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - worldStart).count());
        }
        if (gamePaused) {
            renderPauseOverlay();
        } else {
            const auto uiStart = std::chrono::steady_clock::now();
            renderInGameUi(mouseX, mouseY);
            engine::FrameProbe::instance().addUiMs(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - uiStart).count());
        }
    }
};

GameApplication::Impl* GameApplication::Impl::inputOwner_ = nullptr;

GameApplication::GameApplication(engine::Window& window, std::string assetsRoot)
    : impl_(new Impl(window, std::move(assetsRoot))) {
    logInfo("GameApplication ready.");
}

GameApplication::~GameApplication() {
    logInfo("Shutting down GameApplication.");
    delete impl_;
    impl_ = nullptr;
}

void GameApplication::tickOneFrame(const float deltaSeconds) {
    engine::FrameProbe::instance().beginFrame();
    const auto updateStart = std::chrono::steady_clock::now();
    impl_->update(deltaSeconds);
    engine::FrameProbe::instance().addUpdateMs(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - updateStart).count());
    impl_->renderFrame();
    engine::FrameProbe::instance().endFrame();
}

void GameApplication::simulateMouseMove(const float screenX, const float screenY) {
    impl_->simulateMouseMove(screenX, screenY);
}

void GameApplication::simulateMouseClick(const float screenX, const float screenY) {
    impl_->simulateMouseClick(screenX, screenY);
}

void GameApplication::simulateKeyPress(const int glfwKey) {
    impl_->simulateKeyPress(glfwKey);
}

AppScreen GameApplication::currentScreen() const {
    return impl_->appScreen;
}

gameplay::WorldZone GameApplication::activeWorldZone() const {
    return impl_->zoneManager.activeZone();
}

bool GameApplication::isGamePaused() const {
    return impl_->gamePaused;
}

int GameApplication::characterLevel() const {
    return impl_->overlayState.characterScreen().level;
}

int GameApplication::characterExperience() const {
    return impl_->overlayState.characterScreen().carriedSouls;
}

bool GameApplication::projectWorldToScreen(
    const float worldX,
    const float worldY,
    const float worldZ,
    float& screenX,
    float& screenY) const {
    return impl_->projectWorldToScreen(worldX, worldY, worldZ, screenX, screenY);
}

bool GameApplication::tryGetNearestAttackableMobScreenPosition(float& screenX, float& screenY) {
    return impl_->tryGetNearestAttackableMobScreenPosition(screenX, screenY);
}

bool GameApplication::engageNearestMobForTest() {
    return impl_->engageNearestMobForTest();
}

int GameApplication::runDepth() const {
    return impl_->runProgression_.depth();
}

int GameApplication::lootCoinPool() const {
    return impl_->lootEngine.coinPool();
}

int GameApplication::playerInventoryUsedSlots() const {
    return impl_->playerInventory.usedSlots();
}

int GameApplication::playerMana() const {
    return impl_->skillBar_.mana();
}

int GameApplication::playerMaxMana() const {
    return impl_->skillBar_.maxMana();
}

int GameApplication::playerCurrentHealthForTest() const {
    return impl_->playerCurrentHealth_;
}

std::size_t GameApplication::activeParticleCount() const {
    return impl_->particles_.aliveCount();
}

float GameApplication::cameraTrauma() const {
    return impl_->combatFeedback_.trauma();
}

int GameApplication::difficultyTierIndex() const {
    return static_cast<int>(impl_->runProgression_.tier());
}

int GameApplication::lootJackpotCount() const {
    return impl_->lootEngine.telemetry().jackpot;
}

int GameApplication::lootSpinCount() const {
    return impl_->lootEngine.telemetry().spins;
}

int GameApplication::openVirtualChestsForTest(const int count) {
    const bool restoreLane = impl_->laneActive_;
    impl_->lootEngine.setLootCeiling(systems::LootCeiling::Unique);
    int jackpots = 0;
    for (int index = 0; index < count; ++index) {
        const int before = impl_->lootEngine.telemetry().jackpot;
        impl_->lootEngine.insertCoins(systems::ActionType::CHEST_OPEN);
        static_cast<void>(impl_->processLootDrop(systems::EntityTier::Standard, impl_->playerPosition));
        if (impl_->lootEngine.telemetry().jackpot > before) {
            ++jackpots;
        }
        // Keep the bag from saturating so every reel keeps paying out during the harness.
        if (impl_->playerInventory.usedSlots() >= impl_->playerInventory.capacity() - 2) {
            for (int slot = 0; slot < impl_->playerInventory.capacity(); ++slot) {
                const systems::InventorySlot& bagSlot = impl_->playerInventory.slotAt(slot);
                if (bagSlot.item.has_value() && bagSlot.item->rarity == systems::ItemRarity::Common) {
                    impl_->playerInventory.discardAt(slot);
                }
            }
        }
    }
    if (restoreLane) {
        impl_->applyLanePresentation();
    }
    return jackpots;
}

void GameApplication::setPlayerWorldPositionForTest(const float worldX, const float worldZ) {
    impl_->playerPosition = glm::vec3(worldX, 0.0F, worldZ);
    impl_->hasMoveTarget = false;
    const gameplay::ZoneTransitionResult transition =
        impl_->zoneManager.updatePlayerPosition(toVec3(impl_->playerPosition));
    impl_->playerPosition = toGlm(impl_->zoneManager.player().position());

    if (transition.transitioned) {
        if (transition.toZone == gameplay::WorldZone::PLAINS) {
            impl_->closeTransientOverlays();
            impl_->stateManager.transitionTo(gameplay::GameState::PLAINS);
            impl_->onPlainsZoneEntered();
        } else {
            impl_->closeTransientOverlays();
            impl_->stateManager.transitionTo(gameplay::GameState::TOWN);
            impl_->combatSystem.clearTarget();
        }
    }

    impl_->tradeSystem.setPlayerGold(impl_->zoneManager.player().gold());
}

double GameApplication::runPlainsFrameBenchmarkForTest(
    const int warmupFrames,
    const int measureFrames) {
    constexpr float kFrameDelta = 1.0F / 60.0F;

    for (int frame = 0; frame < warmupFrames; ++frame) {
        tickOneFrame(kFrameDelta);
    }

    engine::FrameProbe::instance().beginCapture();
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(std::max(measureFrames, 1)));

    for (int frame = 0; frame < measureFrames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        tickOneFrame(kFrameDelta);
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    if (samples.empty()) {
        impl_->lastBenchmarkMedianFrameMs_ = 0.0;
        return 0.0;
    }

    std::sort(samples.begin(), samples.end());
    const double median = samples[samples.size() / 2U];
    impl_->lastBenchmarkMedianFrameMs_ = median;

    engine::FrameProbe::instance().endCapture();
    const engine::FrameProbeSample profile = engine::FrameProbe::instance().average();
    std::fprintf(
        stderr,
        "frame profile over %d frames: median %.2f ms | update %.2f ms | world %.2f ms | ui %.2f ms | "
        "draws %u | uiQuads %u | text %u | glGetError %u | glGetUniformLocation %u | glUseProgram %u\n",
        engine::FrameProbe::instance().frames(),
        median,
        profile.updateMs,
        profile.worldMs,
        profile.uiMs,
        profile.draws,
        profile.uiQuads,
        profile.textDraws,
        profile.glErrorChecks,
        profile.uniformQueries,
        profile.programBinds);
    return median;
}

double GameApplication::lastBenchmarkMedianFrameMs() const {
    return impl_->lastBenchmarkMedianFrameMs_;
}

void GameApplication::run() {
    impl_->lastFrameTime = glfwGetTime();
    logInfo("Entering main loop.");

#ifdef __EMSCRIPTEN__
    struct LoopState {
        GameApplication* application;
        Impl* impl;
    };
    static LoopState loopState{};
    loopState.application = this;
    loopState.impl = impl_;

    emscripten_set_main_loop(
        []() {
            if (loopState.application == nullptr || loopState.impl == nullptr) {
                emscripten_cancel_main_loop();
                return;
            }

            if (loopState.impl->window.shouldClose()) {
                logInfo("Main loop ended.");
                emscripten_cancel_main_loop();
                loopState.application = nullptr;
                loopState.impl = nullptr;
                return;
            }

            const double now = glfwGetTime();
            const float deltaSeconds =
                static_cast<float>(now - loopState.impl->lastFrameTime);
            loopState.impl->lastFrameTime = now;

            loopState.application->tickOneFrame(deltaSeconds);
            loopState.impl->window.pollEvents();
            loopState.impl->window.swapBuffers();
        },
        0,
        true);
    glfwSwapInterval(1);
#else
    while (!impl_->window.shouldClose()) {
        const double now = glfwGetTime();
        const float deltaSeconds = static_cast<float>(now - impl_->lastFrameTime);
        impl_->lastFrameTime = now;

        tickOneFrame(deltaSeconds);

        impl_->window.pollEvents();
        impl_->window.swapBuffers();
    }

    logInfo("Main loop ended.");
#endif
}

} // namespace game
