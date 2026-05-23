#include "game/GameApplication.hpp"

#include "game/SaveGame.hpp"
#include "game/AppFlow.hpp"
#include "game/CombatSystem.hpp"
#include "game/EntityPicker.hpp"
#include "game/GameDebug.hpp"
#include "game/GameSettings.hpp"
#include "systems/CharacterCombat.hpp"
#include "gameplay/GameStateManager.hpp"
#include "gameplay/IsometricCamera.hpp"
#include "gameplay/ZoneManager.hpp"
#include "render/Mesh.hpp"
#include "render/MobAssets.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/SpriteSheet.hpp"
#include "render/TextRenderer.hpp"
#include "render/UiAssets.hpp"
#include "render/UiRenderer.hpp"
#include "render/WorldPropAssets.hpp"
#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemStats.hpp"
#include "systems/RunProgression.hpp"
#include "systems/SoulProgression.hpp"
#include "systems/WeaponMastery.hpp"
#include "systems/LootEngine.hpp"
#include "systems/Blacksmith.hpp"
#include "systems/TradeSystem.hpp"
#include "ui/MinimapSystem.hpp"
#include "ui/OverlayState.hpp"
#include "ui/UiHitTest.hpp"
#include "ui/UiInteraction.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiScale.hpp"
#include "ui/UiTextLayout.hpp"
#include "Shader.hpp"

#include "engine/GlBindings.hpp"
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
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

bool screenPointToGround(
    float mouseX,
    float mouseY,
    int screenWidth,
    int screenHeight,
    const gameplay::CameraMatrices& cameraMatrices,
    glm::vec3& outPoint) {
    const float normalizedX = (2.0F * mouseX) / static_cast<float>(screenWidth) - 1.0F;
    const float normalizedY = 1.0F - (2.0F * mouseY) / static_cast<float>(screenHeight);

    glm::vec4 rayClip(normalizedX, normalizedY, -1.0F, 1.0F);
    glm::vec4 rayEye = glm::inverse(cameraMatrices.projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0F, 0.0F);
    const glm::vec4 rayWorld = glm::inverse(cameraMatrices.view) * rayEye;
    const glm::vec3 rayDirection = glm::normalize(glm::vec3(rayWorld));

    if (std::abs(rayDirection.y) < 1e-4F) {
        return false;
    }

    const float intersectionDistance = -cameraMatrices.eye.y / rayDirection.y;
    if (intersectionDistance < 0.0F) {
        return false;
    }

    outPoint = cameraMatrices.eye + rayDirection * intersectionDistance;
    return true;
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

constexpr float kPortraitSize = 64.0F;
constexpr float kHudBarWidth = 240.0F;
constexpr float kHudBarHeight = 22.0F;

struct FloatingCombatText {
    std::string text;
    glm::vec3 worldPosition{};
    float ageSeconds{0.0F};
    float lifetimeSeconds{1.6F};
    float colorR{1.0F};
    float colorG{0.4F};
    float colorB{0.32F};
};

constexpr float kMeleeAttackRange = 2.8F;
constexpr float kMobMeleeRange = 2.35F;
constexpr float kMobAggroRadius = 16.0F;
constexpr float kMobAttackCooldownMob = 1.35F;
constexpr float kMobAttackCooldownBoss = 0.95F;
constexpr float kPlayerAttackAnimDuration = 0.65F;
constexpr float kMoveTargetMarkerWorldHeight = 0.82F;

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
    std::unordered_map<std::uint32_t, glm::vec2> entityLastXZ_{};
    glm::vec2 lastPlayerXZ_{0.0F};
    float spriteAnimTime_{0.0F};
    float playerAttackAnimTime_{0.0F};
    render::SpriteFacing playerFacing_{render::SpriteFacing::Down};

    gameplay::GameStateManager stateManager;
    gameplay::ZoneManager zoneManager;
    gameplay::IsometricCamera camera;

    systems::Inventory playerInventory{6, 4};
    systems::Equipment playerEquipment{};
    systems::Inventory vendorInventory{6, 3};
    systems::TradeSystem tradeSystem;
    systems::LootEngine lootEngine;
    systems::RunProgression runProgression_;

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
        bool isPlayer{false};
        bool isWorldProp{false};
        float depth{0.0F};
    };

    std::vector<SpriteDrawCommand> spriteDrawCommands_{};
    std::vector<ui::Vec2> minimapEntityDots_{};
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

    std::optional<CachedTooltip> cachedItemTooltip_;
    std::optional<CachedTooltip> cachedInteractableTooltip_;

    CombatSystem combatSystem;
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
          tradeSystem(playerInventory, vendorInventory) {
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

        logInfo("Booting into Main Menu.");
        logHelp("Click Play to choose Warrior, Ranger, or Mage.");
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
        playerInventory.addItem({101U, "Traveler Blade", systems::ItemRarity::Common, 12});
        playerInventory.addItem({102U, "Health Tonic", systems::ItemRarity::Common, 5});
        playerInventory.addItem({201U, "Scout Charm", systems::ItemRarity::Rare, 30});
        vendorInventory.addItem({301U, "Forged Sword", systems::ItemRarity::Rare, 80});
        vendorInventory.addItem({302U, "Plate Vest", systems::ItemRarity::Rare, 120});
        vendorInventory.addItem({401U, "Dragon Scale", systems::ItemRarity::Legendary, 500});
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
            hudMessage = "Depth 1 Plains | Souls lost on death | C level up | I gear | Esc";
            logHelp(hudMessage);
        }
    }

    void applyClassStats(CharacterClass characterClass) {
        ui::CharacterScreenData& stats = overlayState.characterScreen();
        stats.level = 1;
        stats.experience = 0;
        stats.experienceToNextLevel = 100;
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
        lootEngine.setSeed(snapshot.progression.lootRngSeed);
        lootEngine.setCoinPool(snapshot.progression.lootCoinPool);
        syncRunDifficulty();

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

        const gameplay::GameState gameplayState =
            snapshot.world.activeZone == gameplay::WorldZone::PLAINS ? gameplay::GameState::PLAINS
                                                                     : gameplay::GameState::TOWN;
        stateManager.forceState(gameplayState);

        overlayState.inventoryOverlay().visible = false;
        overlayState.characterScreen().visible = false;
        gamePaused = false;
        pauseSettingsOpen = false;
        hasMoveTarget = false;
        attackCooldownSeconds_ = 0.0F;
        floatingCombatTexts.clear();
        hoveredInventorySlot_.reset();
        hoveredEquipmentSlot_.reset();
        cachedItemTooltip_.reset();
        cachedInteractableTooltip_.reset();
        invalidateUiHitRegions();

        std::ostringstream message;
        message << "Loaded " << characterClassName(selectedClass) << " (depth "
                << runProgression_.depth() << ").";
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
            camera.matricesForTarget(toVec3(playerPosition));
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

    void drawMenuButtonLabel(const ui::Rect& bounds, const char* label) const {
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 1.0F};
        const float textColor[4] = {1.0F, 0.98F, 0.9F, 1.0F};
        const float labelScale = currentUiScale().dim(2.4F);
        const ui::Rect shadowBounds{
            bounds.x + 2.0F, bounds.y + 2.0F, bounds.width, bounds.height};
        textRenderer.drawTextCentered(shadowBounds, label, labelScale, shadowColor);
        textRenderer.drawTextCentered(bounds, label, labelScale, textColor);
    }

    void drawPanelButtonBackground(
        const ui::Rect& bounds,
        bool hovered,
        bool enabled) const {
        const float enabledBase[4] = {0.22F, 0.48F, 0.28F, 1.0F};
        const float enabledHover[4] = {0.3F, 0.62F, 0.36F, 1.0F};
        const float disabledBase[4] = {0.18F, 0.19F, 0.22F, 0.85F};
        const float border[4] = {0.92F, 0.78F, 0.32F, enabled ? (hovered ? 1.0F : 0.8F) : 0.35F};
        const float* fill = !enabled ? disabledBase : (hovered ? enabledHover : enabledBase);
        uiRenderer.drawFilledRect(bounds.x, bounds.y, bounds.width, bounds.height, fill);
        uiRenderer.drawOutlineRect(bounds.x, bounds.y, bounds.width, bounds.height, border, 2.0F);
    }

    void drawPanelButtonLabel(const ui::Rect& bounds, const char* label, bool enabled) const {
        const float textColor[4] = {0.98F, 0.98F, 1.0F, enabled ? 1.0F : 0.45F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.85F};
        textRenderer.drawTextCentered(bounds, label, currentUiScale().dim(1.25F), shadowColor);
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
        const float border[4] = {0.92F, 0.78F, 0.32F, hovered ? 1.0F : 0.75F};
        uiRenderer.drawFilledRect(
            button.bounds.x, button.bounds.y, button.bounds.width, button.bounds.height, fill);
        uiRenderer.drawOutlineRect(
            button.bounds.x, button.bounds.y, button.bounds.width, button.bounds.height, border, 2.0F);
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

        const glm::vec2 playerXZ(playerPosition.x, playerPosition.z);
        if (hasMoveTarget) {
            glm::vec2 moveDelta = glm::vec2(moveTarget.x, moveTarget.z) - playerXZ;
            playerFacing_ = facingFromDelta(moveDelta);
        } else {
            playerFacing_ = facingFromDelta(playerXZ - lastPlayerXZ_);
        }
    }

    [[nodiscard]] render::SpriteClip resolvePlayerClip() const noexcept {
        if (playerAttackAnimTime_ > 0.0F) {
            return render::SpriteClip::Attack;
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
        const float textColor[4] = {0.92F, 0.95F, 1.0F, 1.0F};
        const float titleWidth = layout.dim(520.0F);
        ui::Rect titleBounds{
            static_cast<float>(layout.width) * 0.5F - titleWidth * 0.5F,
            layout.y(y),
            titleWidth,
            layout.dim(48.0F)};
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
            {{centerX - buttonW * 0.5F, startY, buttonW, buttonH}, "START", 1},
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
            {{centerX - buttonW * 0.5F, settingsY, buttonW, buttonH}, "SETTINGS", 2});
        buttons.push_back(
            {{centerX - buttonW * 0.5F, exitY, buttonW, buttonH}, "EXIT", 3});
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

        zoneManager.forceRespawnInTown();
        playerPosition = toGlm(zoneManager.player().position());
        stateManager.forceState(gameplay::GameState::TOWN);
        combatSystem.clearTarget();
        hasMoveTarget = false;
        attackCooldownSeconds_ = 0.0F;
        mobAttackCooldowns_.clear();
        floatingCombatTexts.clear();

        playerCurrentHealth_ = effectiveCharacterStats().maxHealth;

        std::ostringstream message;
        message << "YOU DIED. Lost " << lostSouls << " souls.";
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

            const int damage = systems::mobMeleeDamage(
                entity.kind, modifiers.mobHpMultiplier, modifiers.mobDamageMultiplier);
            playerCurrentHealth_ -= damage;
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
        const int rowIds[ui::SettingsPanelLayout::kRowCount] = {
            kSettingsResolution,
            kSettingsVolume,
            kSettingsMinimapSize,
            kSettingsMinimapAnchor,
            kSettingsMouseSensitivity,
            kSettingsGraphicsQuality,
        };

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
        default:
            break;
        }
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

    void renderPlayerStatusHud() {
        syncPlayerHealth();

        const ui::UiScale layout = currentUiScale();
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

        const float hudX = layout.x(14.0F);
        const float hudY = layout.y(12.0F);
        const float portraitSize = layout.dim(kPortraitSize);
        const float barHeight = layout.dim(kHudBarHeight);
        const float barWidth = layout.dim(kHudBarWidth);
        const float portraitX = hudX;
        const float portraitY = hudY;
        const float barX = portraitX + portraitSize + layout.dim(10.0F);
        const float hpBarY = portraitY + layout.dim(6.0F);
        const float xpBarY = hpBarY + barHeight + layout.dim(8.0F);

        const float frameColor[4] = {0.08F, 0.09F, 0.14F, 0.92F};
        const float frameBorder[4] = {0.75F, 0.62F, 0.28F, 1.0F};
        uiRenderer.drawFilledRect(
            portraitX - 2.0F, portraitY - 2.0F, portraitSize + 4.0F, portraitSize + 4.0F, frameColor);
        uiRenderer.drawOutlineRect(
            portraitX - 2.0F, portraitY - 2.0F, portraitSize + 4.0F, portraitSize + 4.0F, frameBorder);

        if (mobAssets.hasClassSheets() && selectedClass != CharacterClass::NONE) {
            drawClassSpriteUi(
                selectedClass,
                portraitX + 4.0F,
                portraitY + 4.0F,
                portraitSize - 8.0F,
                portraitSize - 4.0F,
                render::SpriteClip::Idle);
        } else if (uiAssets.isLoaded()) {
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
                portraitX + 2.0F,
                portraitY + 2.0F,
                portraitSize - 4.0F,
                portraitSize - 4.0F,
                frameU0,
                0.0F,
                frameU1,
                1.0F,
                white);
        } else {
            glm::vec3 portraitColor = visualFor(gameplay::EntityKind::PLAYER).color;
            if (selectedClass == CharacterClass::WARRIOR) {
                portraitColor = glm::vec3(0.85F, 0.35F, 0.25F);
            } else if (selectedClass == CharacterClass::RANGER) {
                portraitColor = glm::vec3(0.3F, 0.8F, 0.35F);
            } else if (selectedClass == CharacterClass::MAGE) {
                portraitColor = glm::vec3(0.5F, 0.35F, 0.95F);
            }

            const float portraitFill[4] = {portraitColor.r, portraitColor.g, portraitColor.b, 1.0F};
            uiRenderer.drawFilledRect(portraitX, portraitY, portraitSize, portraitSize, portraitFill);
        }

        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            const float heartSize = layout.dim(24.0F);
            uiRenderer.drawTexturedRect(
                uiAssets.heartRed(), barX - heartSize - 4.0F, hpBarY - 1.0F, heartSize, heartSize, white);
            drawTexturedBar(
                uiAssets.barFrame(), uiAssets.hpBarFill(), barX, hpBarY, barWidth, barHeight, healthRatio);

            uiRenderer.drawTexturedRect(
                uiAssets.heartYellow(), barX - heartSize - 4.0F, xpBarY - 1.0F, heartSize, heartSize, white);
            drawTexturedBar(
                uiAssets.barFrame(), uiAssets.hpBarFill(), barX, xpBarY, barWidth, barHeight, soulRatio);
        } else {
            const float hpFill[4] = {0.82F, 0.15F, 0.12F, 1.0F};
            const float hpEmpty[4] = {0.14F, 0.1F, 0.1F, 0.95F};
            const float xpFill[4] = {0.95F, 0.82F, 0.18F, 1.0F};
            const float xpEmpty[4] = {0.18F, 0.16F, 0.1F, 0.95F};
            drawSegmentedBar(barX, hpBarY, barWidth, barHeight, 16, healthRatio, hpFill, hpEmpty);
            drawSegmentedBar(barX, xpBarY, barWidth, barHeight, 16, soulRatio, xpFill, xpEmpty);
        }
    }

    void renderPlayerStatusHudLabels() const {
        const ui::UiScale layout = currentUiScale();
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const systems::EffectiveCharacterStats effective = effectiveCharacterStats();
        const int maxHealth = std::max(effective.maxHealth, 1);

        const float hudX = layout.x(14.0F);
        const float hudY = layout.y(12.0F);
        const float portraitSize = layout.dim(kPortraitSize);
        const float barHeight = layout.dim(kHudBarHeight);
        const float barWidth = layout.dim(kHudBarWidth);
        const float barX = hudX + portraitSize + layout.dim(10.0F);
        const float hpBarY = hudY + layout.dim(6.0F);
        const float xpBarY = hpBarY + barHeight + layout.dim(8.0F);

        const float labelColor[4] = {0.98F, 0.98F, 1.0F, 1.0F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.85F};
        const float hudTextScale = layout.dim(1.45F);

        std::ostringstream hpText;
        hpText << playerCurrentHealth_ << " / " << maxHealth << " HP";
        const ui::Rect hpLabelRect{barX, hpBarY, barWidth, barHeight};
        textRenderer.drawTextCentered(hpLabelRect, hpText.str().c_str(), hudTextScale, shadowColor);
        textRenderer.drawTextCentered(hpLabelRect, hpText.str().c_str(), hudTextScale, labelColor);

        std::ostringstream soulText;
        soulText << "Souls " << base.carriedSouls;
        if (zoneManager.activeZone() == gameplay::WorldZone::PLAINS) {
            soulText << " (at risk)";
        }
        const ui::Rect xpLabelRect{barX, xpBarY, barWidth, barHeight};
        textRenderer.drawTextCentered(xpLabelRect, soulText.str().c_str(), hudTextScale, shadowColor);
        textRenderer.drawTextCentered(xpLabelRect, soulText.str().c_str(), hudTextScale, labelColor);

        std::ostringstream levelText;
        levelText << "Lv " << base.level << "  "
                  << systems::formatSoulGainMultiplier(base.soulGainMultiplier);
        const ui::Rect levelRect{barX + barWidth - layout.dim(52.0F), hpBarY - layout.dim(22.0F), layout.dim(48.0F), layout.dim(18.0F)};
        textRenderer.drawTextCentered(levelRect, levelText.str().c_str(), hudTextScale, labelColor);
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

    void renderTargetMobHud() const {
        if (!combatSystem.hasTarget()) {
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

        const float frameColor[4] = {0.08F, 0.06F, 0.1F, 0.92F};
        const float borderColor[4] = {0.95F, 0.35F, 0.25F, 1.0F};
        uiRenderer.drawFilledRect(barX, barY, barWidth, barHeight, frameColor);
        uiRenderer.drawOutlineRect(barX, barY, barWidth, barHeight, borderColor);

        const float fillColor[4] = {0.82F, 0.18F, 0.14F, 1.0F};
        if (healthRatio > 0.0F) {
            uiRenderer.drawFilledRect(barX, barY, barWidth * healthRatio, barHeight, fillColor);
        }
    }

    void renderTargetMobHudLabel() const {
        if (!combatSystem.hasTarget()) {
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
        label << entityDisplayName(target->kind) << "  " << health->currentHp << " / " << health->maxHp << " HP";
        const ui::Rect labelRect{barX, barY, barWidth, barHeight};
        textRenderer.drawTextCentered(labelRect, label.str().c_str(), 1.5F, shadowColor);
        textRenderer.drawTextCentered(labelRect, label.str().c_str(), 1.5F, textColor);
    }

    void renderMobNameplates() const {
        if (!showMobNameplates()) {
            return;
        }

        const gameplay::CameraMatrices cameraMatrices =
            camera.matricesForTarget(toVec3(playerPosition));
        const float textColor[4] = {0.95F, 0.92F, 0.85F, 1.0F};
        const float shadowColor[4] = {0.0F, 0.0F, 0.0F, 0.9F};

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !isAttackableEntity(entity.kind) || !combatSystem.isMobAlive(entity.id)) {
                continue;
            }

            const glm::vec3 worldPosition = toGlm(entity.position);
            if (!isWithinRenderDistance(worldPosition)) {
                continue;
            }

            float screenX = 0.0F;
            float screenY = 0.0F;
            const float billboardHeight = mobAssets.spriteWorldHeight(entity.kind);
            const glm::vec3 nameplatePosition = toGlm(entity.position) + glm::vec3(0.0F, billboardHeight + 0.35F, 0.0F);
            if (!worldToScreen(
                    nameplatePosition,
                    cameraMatrices.view,
                    cameraMatrices.projection,
                    window.width(),
                    window.height(),
                    screenX,
                    screenY)) {
                continue;
            }

            const char* name = entityDisplayName(entity.kind);
            const float textWidth = textRenderer.measureTextWidth(name, 1.35F);
            const ui::Rect labelRect{screenX - textWidth * 0.5F - 4.0F, screenY - 8.0F, textWidth + 8.0F, 16.0F};
            textRenderer.drawTextCentered(labelRect, name, 1.35F, shadowColor);
            textRenderer.drawTextCentered(labelRect, name, 1.35F, textColor);
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
            {{centerX - buttonW * 0.5F, startY + buttonH + gap, buttonW, buttonH}, "Settings", 21},
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

    void drawTooltipBackground(const ui::TooltipBoxLayout& layout) const {
        const ui::Rect& box = layout.box;
        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                box.x,
                box.y,
                box.width,
                box.height,
                layout.nineSliceBorder,
                white);
        } else {
            const float bg[4] = {0.05F, 0.06F, 0.1F, 0.94F};
            const float border[4] = {0.85F, 0.75F, 0.35F, 1.0F};
            uiRenderer.drawFilledRect(box.x, box.y, box.width, box.height, bg);
            uiRenderer.drawOutlineRect(box.x, box.y, box.width, box.height, border);
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

        const float textColor[4] = {0.95F, 0.96F, 1.0F, 1.0F};
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

        const ui::HitRegion* region = uiInteraction_.hitTest(mouseX, mouseY);
        if (region == nullptr) {
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
        if (!cachedItemTooltip_.has_value()) {
            return false;
        }

        const ui::Rect& box = cachedItemTooltip_->layout.box;
        const float centerX = slot.x + slot.width * 0.5F;
        const float centerY = slot.y + slot.height * 0.5F;
        return box.contains(centerX, centerY);
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

        if (uiAssets.isLoaded()) {
            const float tint[4] = {1.0F, 1.0F, 1.0F, hovered ? 1.0F : 0.92F};
            uiRenderer.drawTexturedRect(
                hovered ? uiAssets.inventorySlotHover() : uiAssets.inventorySlot(),
                slot.x,
                slot.y,
                slot.width,
                slot.height,
                tint);
        } else {
            const float emptySlot[4] = {0.12F, 0.14F, 0.18F, 1.0F};
            const float slotBorder[4] = {0.35F, 0.38F, 0.45F, 1.0F};
            uiRenderer.drawFilledRect(slot.x, slot.y, slot.width, slot.height, emptySlot);
            uiRenderer.drawOutlineRect(slot.x, slot.y, slot.width, slot.height, slotBorder);
        }

        if (!playerEquipment.isSlotOccupied(equipmentSlot)) {
            return;
        }

        const systems::ItemMetadata& item = *playerEquipment.itemAt(equipmentSlot);
        float iconFill[4]{};
        float iconBorder[4]{};
        rarityColors(item.rarity, iconFill, iconBorder);

        const float iconPad = 5.0F;
        uiRenderer.drawFilledRect(
            slot.x + iconPad,
            slot.y + iconPad,
            slot.width - iconPad * 2.0F,
            slot.height - iconPad * 2.0F,
            iconFill);
        uiRenderer.drawOutlineRect(
            slot.x + iconPad,
            slot.y + iconPad,
            slot.width - iconPad * 2.0F,
            slot.height - iconPad * 2.0F,
            iconBorder);
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

    [[nodiscard]] float worldRenderDistance() const noexcept {
        if (zoneManager.activeZone() != gameplay::WorldZone::PLAINS) {
            return 9999.0F;
        }
        switch (gameSettings.graphicsQuality) {
        case 0:
            return 70.0F;
        case 2:
            return 200.0F;
        case 1:
        default:
            return 110.0F;
        }
    }

    [[nodiscard]] bool isWithinRenderDistance(const glm::vec3& worldPosition) const {
        const glm::vec3 delta = worldPosition - playerPosition;
        const float maxDistance = worldRenderDistance();
        return glm::dot(delta, delta) <= maxDistance * maxDistance;
    }

    [[nodiscard]] bool showMobNameplates() const noexcept {
        return gameSettings.graphicsQuality > 0;
    }

    void syncRunDifficulty() {
        const systems::DifficultyModifiers modifiers = runProgression_.modifiers();
        combatSystem.setDifficultyModifiers(modifiers);
        lootEngine.setZoneDepth(runProgression_.depth());
        lootEngine.setLootTierBonus(modifiers.lootTierBonus);
    }

    void refreshPlainsRun() {
        zoneManager.respawnPlainsContent(runProgression_.runSeed(), runProgression_.depth());
        combatSystem.reset();
        invalidateSceneryCaches();
        syncRunDifficulty();
        ensureCombatSynced();
    }

    void onPlainsZoneEntered() {
        invalidateSceneryCaches();
        syncRunDifficulty();
        ensureCombatSynced();
    }

    bool processLootDrop(const systems::EntityTier tier, const glm::vec3& worldPosition) {
        const systems::LootDropResult drop = lootEngine.triggerDropCheck(tier);
        if (!drop.dropped) {
            return false;
        }

        const systems::InventoryAddResult added = playerInventory.addItem(drop.item);
        if (!added.success) {
            hudMessage = "Inventory full — loot lost!";
            logInfo(hudMessage);
            return false;
        }

        spawnLootNumber(worldPosition, drop.item.name);
        std::ostringstream message;
        message << "Loot: " << drop.item.name;
        hudMessage = message.str();
        logInfo(hudMessage);
        return true;
    }

    void tryInteractWithProp(const gameplay::WorldEntitySnapshot& entity) {
        if (!entity.active) {
            return;
        }

        const glm::vec3 worldPosition = toGlm(entity.position);
        if (entity.kind == gameplay::EntityKind::ENV_CHEST) {
            lootEngine.registerAction(systems::ActionType::CHEST_OPEN);
            processLootDrop(systems::EntityTier::Standard, worldPosition);
            if (!zoneManager.deactivateEntity(entity.id)) {
                logInfo("Warning: failed to remove opened chest from world.");
            }
            return;
        }

        if (entity.kind == gameplay::EntityKind::ENV_ROCK) {
            lootEngine.registerAction(systems::ActionType::ROCK_CLICK);
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

        const int damage = effective.damage;
        const std::optional<DamageResult> result = combatSystem.applyDamage(targetId, damage);
        if (!result.has_value()) {
            return;
        }

        spawnDamageNumber(targetPosition, result->damageDealt);
        playerAttackAnimTime_ = kPlayerAttackAnimDuration;
        grantWeaponMasteryFromCombat(result->damageDealt, 0, false);

        std::ostringstream hitMessage;
        hitMessage << "Hit for " << result->damageDealt << " (" << result->remainingHp << " HP left)";
        logInfo(hitMessage.str());

        if (!result->killed) {
            return;
        }

        grantWeaponMasteryFromCombat(0, result->xpReward, true);

        if (!zoneManager.deactivateEntity(targetId)) {
            logInfo("Warning: failed to remove defeated mob from world.");
        }

        ui::CharacterScreenData& stats = overlayState.characterScreen();
        const float gainMultiplier = stats.soulGainMultiplier;
        const int soulsAwarded =
            systems::scaleSoulReward(result->xpReward, gainMultiplier);
        grantSouls(soulsAwarded);
        spawnSoulsNumber(targetPosition, soulsAwarded);
        runProgression_.onMobKill();

        systems::EntityTier lootTier = systems::EntityTier::Standard;
        if (target->kind == gameplay::EntityKind::ENEMY_BOSS) {
            systems::registerBossSoulGain(stats);
            lootEngine.registerAction(systems::ActionType::BOSS_KILL);
            processLootDrop(systems::EntityTier::Boss, targetPosition);
            runProgression_.onBossDefeated();
            refreshPlainsRun();
            std::ostringstream depthMessage;
            depthMessage << "Boss slain! +" << soulsAwarded << " souls "
                         << systems::formatSoulGainMultiplier(gainMultiplier)
                         << " -> "
                         << systems::formatSoulGainMultiplier(stats.soulGainMultiplier)
                         << " | Depth " << runProgression_.depth();
            hudMessage = depthMessage.str();
            logInfo(hudMessage);
            return;
        }

        systems::registerMobSoulGain(stats);
        lootEngine.registerAction(systems::ActionType::MOB_KILL);
        processLootDrop(lootTier, targetPosition);

        std::ostringstream killMessage;
        killMessage << "Mob defeated (+" << soulsAwarded << " souls "
                    << systems::formatSoulGainMultiplier(gainMultiplier) << " -> "
                    << systems::formatSoulGainMultiplier(stats.soulGainMultiplier) << ")";
        hudMessage = killMessage.str();
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
        constexpr float moveSpeed = 12.0F;
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
            }
        }

        tradeSystem.setPlayerGold(zoneManager.player().gold());
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
            if (overlayState.inventoryOverlay().visible) {
                overlayState.showInventoryOverlay(false);
            } else {
                if (stateManager.currentState() == gameplay::GameState::CHARACTER_MENU) {
                    stateManager.closeCharacterMenu();
                    overlayState.showCharacterScreen(false);
                }
                overlayState.showInventoryOverlay(true);
            }
        }

        if (keyPressed(GLFW_KEY_E)) {
            if (zoneManager.isInsideBlacksmithRadius(toVec3(playerPosition)) &&
                stateManager.currentState() == gameplay::GameState::TOWN) {
                tradeSystem.setPlayerGold(zoneManager.player().gold());
                stateManager.enterTrading();
                hudMessage = "Blacksmith — click items to sell, forge services below";
                logInfo(hudMessage);
            }
        }

        float mouseX = 0.0F;
        float mouseY = 0.0F;
        const bool clicked = mouseClicked(mouseX, mouseY);
        if (clicked && isMouseOverInGameUi(mouseX, mouseY)) {
            handleEquipmentUiClick(mouseX, mouseY);
        }

        if (stateManager.isPausedForUi()) {
            return;
        }

        if (clicked && !isMouseOverInGameUi(mouseX, mouseY)) {
            const gameplay::CameraMatrices cameraMatrices =
                camera.matricesForTarget(toVec3(playerPosition));
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
            if (screenPointToGround(
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

        updateMovementTowardTarget(deltaSeconds);
        updateCombat(deltaSeconds);
    }

    void renderMenuBackdrop() {
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.06F, 0.07F, 0.1F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        uiRenderer.beginFrame();
        const float backdrop[4] = {0.05F, 0.06F, 0.09F, 1.0F};
        uiRenderer.drawFilledRect(
            0.0F, 0.0F, static_cast<float>(window.width()), static_cast<float>(window.height()), backdrop);
    }

    void renderMainMenu() {
        renderMenuBackdrop();

        const std::vector<MenuButton> buttons = buildMainMenuButtons();
        const float playBase[4] = {0.18F, 0.55F, 0.32F, 1.0F};
        const float playHover[4] = {0.28F, 0.75F, 0.42F, 1.0F};
        const float continueBase[4] = {0.62F, 0.48F, 0.16F, 1.0F};
        const float continueHover[4] = {0.82F, 0.62F, 0.22F, 1.0F};
        const float settingsBase[4] = {0.22F, 0.35F, 0.62F, 1.0F};
        const float settingsHover[4] = {0.32F, 0.48F, 0.82F, 1.0F};
        const float exitBase[4] = {0.55F, 0.2F, 0.2F, 1.0F};
        const float exitHover[4] = {0.75F, 0.28F, 0.28F, 1.0F};

        for (const MenuButton& button : buttons) {
            if (button.id == 1) {
                drawMenuButtonBackground(button, playBase, playHover);
            } else if (button.id == 4) {
                drawMenuButtonBackground(button, continueBase, continueHover);
            } else if (button.id == 2) {
                drawMenuButtonBackground(button, settingsBase, settingsHover);
            } else {
                drawMenuButtonBackground(button, exitBase, exitHover);
            }
        }

        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        drawTitle("Game Engine RPG", 72.0F, 3.2F);
        for (const MenuButton& button : buttons) {
            drawMenuButtonLabel(button.bounds, button.label);
        }
        textRenderer.endOverlay();
        glEnable(GL_DEPTH_TEST);
    }

    void renderCharacterSelect() {
        advanceSpriteAnimClock();
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
        advanceSpriteAnimClock();
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

        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                panel.x,
                panel.y,
                panel.width,
                panel.height,
                currentUiScale().dim(16.0F),
                white);
            uiRenderer.drawTexturedRect(
                uiAssets.settingsCross(),
                layout.closeButton.x,
                layout.closeButton.y,
                layout.closeButton.width,
                layout.closeButton.height,
                white);
        } else {
            const float panelColor[4] = {0.1F, 0.11F, 0.16F, 0.96F};
            const float borderColor[4] = {0.45F, 0.55F, 0.85F, 1.0F};
            uiRenderer.drawFilledRect(panel.x, panel.y, panel.width, panel.height, panelColor);
            uiRenderer.drawOutlineRect(panel.x, panel.y, panel.width, panel.height, borderColor);
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
                const float cycleBg[4] = {0.18F, 0.22F, 0.32F, hovered ? 1.0F : 0.92F};
                const float cycleBorder[4] = {0.5F, 0.62F, 0.95F, 1.0F};
                uiRenderer.drawFilledRect(
                    control.bounds.x, control.bounds.y, control.bounds.width, control.bounds.height, cycleBg);
                uiRenderer.drawOutlineRect(
                    control.bounds.x, control.bounds.y, control.bounds.width, control.bounds.height, cycleBorder);
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

        const int rowIds[ui::SettingsPanelLayout::kRowCount] = {
            kSettingsResolution,
            kSettingsVolume,
            kSettingsMinimapSize,
            kSettingsMinimapAnchor,
            kSettingsMouseSensitivity,
            kSettingsGraphicsQuality,
        };

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
        uiRenderer.beginFrame();
        const float dimmer[4] = {0.0F, 0.0F, 0.0F, 0.62F};
        uiRenderer.drawFilledRect(
            0.0F, 0.0F, static_cast<float>(window.width()), static_cast<float>(window.height()), dimmer);

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
        const float saveBase[4] = {0.5F, 0.22F, 0.2F, 1.0F};
        const float saveHover[4] = {0.7F, 0.3F, 0.28F, 1.0F};
        const float settingsBase[4] = {0.22F, 0.35F, 0.62F, 1.0F};
        const float settingsHover[4] = {0.32F, 0.48F, 0.82F, 1.0F};
        const float resumeBase[4] = {0.18F, 0.55F, 0.32F, 1.0F};
        const float resumeHover[4] = {0.28F, 0.75F, 0.42F, 1.0F};

        for (const MenuButton& button : buttons) {
            if (button.id == 20) {
                drawMenuButtonBackground(button, saveBase, saveHover);
            } else if (button.id == 21) {
                drawMenuButtonBackground(button, settingsBase, settingsHover);
            } else {
                drawMenuButtonBackground(button, resumeBase, resumeHover);
            }
        }

        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        drawTitle("Paused", static_cast<float>(currentUiScale().height) * 0.28F, 3.0F);
        for (const MenuButton& button : buttons) {
            drawMenuButtonLabel(button.bounds, button.label);
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
            camera.matricesForTarget(toVec3(playerPosition));
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
        light.ambientDark = onPlains ? 0.04F : 0.08F;
        light.ambientBright = onPlains ? 0.42F : 0.48F;
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
        if (clip == render::SpriteClip::Attack && playerAttackAnimTime_ > 0.0F) {
            return kPlayerAttackAnimDuration - playerAttackAnimTime_;
        }
        return spriteAnimTime_;
    }

    void drawPlayerWorldSprite(
        const WorldLightSettings& light,
        const glm::mat4& view,
        const glm::mat4& projection) const {
        const render::SpriteClip clip = resolvePlayerClip();
        const render::SpriteFrameSample sample = mobAssets.sampleClassSprite(
            selectedClass, clip, playerFacing_, playerSpriteAnimTime(clip));
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

        const float renderDistance = worldRenderDistance();
        const float renderDistanceSq = renderDistance * renderDistance;

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active || !mobAssets.isSpriteEntity(entity.kind)) {
                continue;
            }
            const glm::vec3 delta = toGlm(entity.position) - playerPosition;
            if (glm::dot(delta, delta) > renderDistanceSq) {
                continue;
            }
            entityLastXZ_[entity.id] = glm::vec2(entity.position.x, entity.position.z);
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

        const bool useIdlePose = shouldUseIdlePose(entity);
        const render::SpriteFacing facing = entitySpriteFacing(entity);
        const render::SpriteClip clip =
            useIdlePose ? render::SpriteClip::Idle : render::SpriteClip::Walk;
        const render::SpriteFrameSample sample = mobAssets.sampleMobSprite(
            entity.kind, entity.id, useIdlePose, clip, facing, spriteAnimTime_);
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

    void renderWorld() {
        const gameplay::Vec3 target = toVec3(playerPosition);
        const gameplay::CameraMatrices cameraMatrices = camera.matricesForTarget(target);

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.04F, 0.05F, 0.08F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        applyPlayerLightUniforms();

        const gameplay::AxisAlignedBounds bounds =
            zoneManager.activeZone() == gameplay::WorldZone::TOWN ? zoneManager.townBounds()
                                                                  : zoneManager.plainsBounds();
        const float centerX = (bounds.minX + bounds.maxX) * 0.5F;
        const float centerZ = (bounds.minZ + bounds.maxZ) * 0.5F;
        const float spanX = bounds.maxX - bounds.minX;
        const float spanZ = bounds.maxZ - bounds.minZ;

        drawCube(
            cameraMatrices.view,
            cameraMatrices.projection,
            glm::vec3(centerX, -0.05F, centerZ),
            glm::vec3(spanX, 0.1F, spanZ),
            glm::vec3(0.18F, 0.22F, 0.2F));

        const float gateColor =
            zoneManager.activeZone() == gameplay::WorldZone::TOWN ? 0.35F : 0.9F;
        drawCube(
            cameraMatrices.view,
            cameraMatrices.projection,
            glm::vec3(0.0F, 0.05F, 40.0F),
            glm::vec3(12.0F, 0.15F, 2.0F),
            glm::vec3(gateColor, 0.4F, zoneManager.activeZone() == gameplay::WorldZone::TOWN ? 0.9F : 0.35F));

        const WorldLightSettings worldLight = buildWorldLightSettings();
        const float renderDistance = worldRenderDistance();
        const float renderDistanceSq = renderDistance * renderDistance;
        const bool mobSpritesLoaded = mobAssets.isLoaded();
        const glm::vec3 eye = cameraMatrices.eye;

        spriteDrawCommands_.clear();
        spriteDrawCommands_.reserve(zoneManager.scenery().size() + 1U);

        for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
            if (!entity.active) {
                continue;
            }

            const glm::vec3 worldPosition = toGlm(entity.position);
            const glm::vec3 delta = worldPosition - playerPosition;
            if (glm::dot(delta, delta) > renderDistanceSq) {
                continue;
            }

            if (worldPropAssets.hasProps(entity.kind)) {
                SpriteDrawCommand command{};
                command.entity = &entity;
                command.isWorldProp = true;
                command.depth = glm::length(worldPosition - eye);
                spriteDrawCommands_.push_back(command);
                continue;
            }

            if (mobSpritesLoaded && mobAssets.isSpriteEntity(entity.kind)) {
                SpriteDrawCommand command{};
                command.entity = &entity;
                command.depth = glm::length(worldPosition - eye);
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
            playerCommand.depth = glm::length(playerPosition - eye);
            spriteDrawCommands_.push_back(playerCommand);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::sort(
            spriteDrawCommands_.begin(),
            spriteDrawCommands_.end(),
            [](const SpriteDrawCommand& a, const SpriteDrawCommand& b) { return a.depth > b.depth; });

        spriteRenderer.beginBillboardPass(
            cameraMatrices.view,
            cameraMatrices.projection,
            worldLight.playerPos,
            worldLight.radius,
            worldLight.ambientDark,
            worldLight.ambientBright);

        for (const SpriteDrawCommand& command : spriteDrawCommands_) {
            if (command.isPlayer) {
                drawPlayerWorldSprite(worldLight, cameraMatrices.view, cameraMatrices.projection);
            } else if (command.entity != nullptr && command.isWorldProp) {
                drawWorldPropSprite(
                    *command.entity, cameraMatrices.view, cameraMatrices.projection, worldLight);
            } else if (command.entity != nullptr) {
                drawEntitySprite(
                    *command.entity, cameraMatrices.view, cameraMatrices.projection, worldLight);
            }
        }

        spriteRenderer.endBillboardPass();

        if (hasMoveTarget) {
            renderMoveTargetMarker(cameraMatrices.view, cameraMatrices.projection, worldLight);
        }

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

    void renderHudInfoStrip() {
        if (hudMessage.empty()) {
            return;
        }

        const float stripColor[4] = {0.05F, 0.07F, 0.1F, 0.75F};
        const ui::Rect strip = ui::computeHudChromeLayout(currentUiScale()).messageStrip;
        uiRenderer.drawFilledRect(strip.x, strip.y, strip.width, strip.height, stripColor);
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
        drawBoundedText(textBounds, hudMessage, layout.dim(1.7F), messageColor);
    }

    void renderFloatingCombatTextBackgrounds() const {
        if (floatingCombatTexts.empty()) {
            return;
        }

        const gameplay::CameraMatrices cameraMatrices =
            camera.matricesForTarget(toVec3(playerPosition));

        for (const FloatingCombatText& floating : floatingCombatTexts) {
            float screenX = 0.0F;
            float screenY = 0.0F;
            if (!worldToScreen(
                    floating.worldPosition,
                    cameraMatrices.view,
                    cameraMatrices.projection,
                    window.width(),
                    window.height(),
                    screenX,
                    screenY)) {
                continue;
            }

            const float fade = 1.0F - (floating.ageSeconds / floating.lifetimeSeconds);
            const float pad = 8.0F;
            const float textWidth = textRenderer.measureTextWidth(floating.text.c_str(), 2.2F);
            const float boxW = textWidth + pad * 2.0F;
            const float boxH = 30.0F;
            const float bg[4] = {
                floating.colorR * 0.12F,
                floating.colorG * 0.12F,
                floating.colorB * 0.12F,
                0.85F * fade};
            const float border[4] = {floating.colorR, floating.colorG, floating.colorB, 0.95F * fade};

            uiRenderer.drawFilledRect(screenX - boxW * 0.5F, screenY - boxH, boxW, boxH, bg);
            uiRenderer.drawOutlineRect(screenX - boxW * 0.5F, screenY - boxH, boxW, boxH, border);
        }
    }

    void renderFloatingCombatTextLabels() const {
        if (floatingCombatTexts.empty()) {
            return;
        }

        const gameplay::CameraMatrices cameraMatrices =
            camera.matricesForTarget(toVec3(playerPosition));

        for (const FloatingCombatText& floating : floatingCombatTexts) {
            float screenX = 0.0F;
            float screenY = 0.0F;
            if (!worldToScreen(
                    floating.worldPosition,
                    cameraMatrices.view,
                    cameraMatrices.projection,
                    window.width(),
                    window.height(),
                    screenX,
                    screenY)) {
                continue;
            }

            const float fade = 1.0F - (floating.ageSeconds / floating.lifetimeSeconds);
            const float alpha = std::clamp(fade, 0.25F, 1.0F);
            const float textColor[4] = {floating.colorR, floating.colorG, floating.colorB, alpha};
            const float textScale = 2.2F;
            const float textWidth = textRenderer.measureTextWidth(floating.text.c_str(), textScale);
            const float boxH = 30.0F;
            textRenderer.drawText(
                screenX - textWidth * 0.5F,
                screenY - boxH + 7.0F,
                floating.text.c_str(),
                textScale,
                textColor);
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
        const ui::MinimapWidgetLayout widget = minimapWidgetLayout();
        const ui::Rect& frame = widget.frame;
        const ui::Rect& content = widget.content;

        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                frame.x,
                frame.y,
                frame.width,
                frame.height,
                currentUiScale().dim(14.0F),
                white);
        } else {
            const float frameColor[4] = {0.04F, 0.05F, 0.08F, 0.9F};
            const float frameBorder[4] = {0.3F, 0.75F, 0.45F, 1.0F};
            uiRenderer.drawFilledRect(frame.x, frame.y, frame.width, frame.height, frameColor);
            uiRenderer.drawOutlineRect(frame.x, frame.y, frame.width, frame.height, frameBorder);
        }

        const gameplay::AxisAlignedBounds bounds =
            zoneManager.activeZone() == gameplay::WorldZone::TOWN ? zoneManager.townBounds()
                                                                  : zoneManager.plainsBounds();
        minimap.setViewport(ui::Rect2D{content.x, content.y, content.width, content.height});
        minimap.setTerrainBounds(
            ui::TerrainBounds{bounds.minX, bounds.maxX, bounds.minZ, bounds.maxZ});

        minimapEntityDots_.clear();
        if (gameSettings.graphicsQuality > 0) {
            minimapEntityDots_.reserve(zoneManager.scenery().size());
            for (const gameplay::WorldEntitySnapshot& entity : zoneManager.scenery()) {
                if (!entity.active) {
                    continue;
                }
                minimapEntityDots_.push_back(ui::Vec2{entity.position.x, entity.position.z});
            }
        }

        const ui::MinimapLayer layer =
            minimap.buildLayer(playerPosition.x, playerPosition.z, minimapEntityDots_);

        const float playerDot[4] = {0.3F, 0.65F, 1.0F, 1.0F};
        uiRenderer.drawFilledRect(layer.player.pixel.x - 4.0F, layer.player.pixel.y - 4.0F, 8.0F, 8.0F, playerDot);

        const float entityDot[4] = {0.9F, 0.35F, 0.25F, 0.9F};
        for (const ui::MinimapMarker& marker : layer.entities) {
            if (!marker.inBounds) {
                continue;
            }
            uiRenderer.drawFilledRect(marker.pixel.x - 2.0F, marker.pixel.y - 2.0F, 4.0F, 4.0F, entityDot);
        }
    }

    static void rarityColors(
        const systems::ItemRarity rarity,
        float fill[4],
        float border[4]) noexcept {
        switch (rarity) {
        case systems::ItemRarity::Rare:
            fill[0] = 0.22F;
            fill[1] = 0.32F;
            fill[2] = 0.62F;
            fill[3] = 1.0F;
            border[0] = 0.45F;
            border[1] = 0.65F;
            border[2] = 1.0F;
            border[3] = 1.0F;
            break;
        case systems::ItemRarity::Legendary:
            fill[0] = 0.55F;
            fill[1] = 0.28F;
            fill[2] = 0.62F;
            fill[3] = 1.0F;
            border[0] = 0.95F;
            border[1] = 0.55F;
            border[2] = 1.0F;
            border[3] = 1.0F;
            break;
        case systems::ItemRarity::Common:
        default:
            fill[0] = 0.22F;
            fill[1] = 0.38F;
            fill[2] = 0.24F;
            fill[3] = 1.0F;
            border[0] = 0.45F;
            border[1] = 0.75F;
            border[2] = 0.45F;
            border[3] = 1.0F;
            break;
        }
    }

    void renderInventoryOverlay() {
        if (!overlayState.inventoryOverlay().visible) {
            return;
        }

        const ui::InventoryPaperDollLayout layout = buildInventoryPaperDollLayout();
        const ui::Rect& panel = layout.panel;
        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                panel.x,
                panel.y,
                panel.width,
                panel.height,
                currentUiScale().dim(16.0F),
                white);
        } else {
            const float panelColor[4] = {0.07F, 0.08F, 0.12F, 0.94F};
            const float borderColor[4] = {0.75F, 0.6F, 0.2F, 1.0F};
            uiRenderer.drawFilledRect(panel.x, panel.y, panel.width, panel.height, panelColor);
            uiRenderer.drawOutlineRect(panel.x, panel.y, panel.width, panel.height, borderColor);
        }

        const float sidebarFill[4] = {0.05F, 0.06F, 0.1F, 0.55F};
        uiRenderer.drawFilledRect(
            layout.statsSidebar.x,
            layout.statsSidebar.y,
            layout.statsSidebar.width,
            layout.statsSidebar.height,
            sidebarFill);
        const float sidebarBorder[4] = {0.45F, 0.38F, 0.22F, 0.85F};
        uiRenderer.drawOutlineRect(
            layout.statsSidebar.x,
            layout.statsSidebar.y,
            layout.statsSidebar.width,
            layout.statsSidebar.height,
            sidebarBorder);

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

            if (uiAssets.isLoaded()) {
                const float tint[4] = {1.0F, 1.0F, 1.0F, hovered ? 1.0F : 0.92F};
                uiRenderer.drawTexturedRect(
                    hovered ? uiAssets.inventorySlotHover() : uiAssets.inventorySlot(),
                    slot.x,
                    slot.y,
                    slot.width,
                    slot.height,
                    tint);
            } else {
                const float emptySlot[4] = {0.12F, 0.14F, 0.18F, 1.0F};
                const float slotBorder[4] = {0.35F, 0.38F, 0.45F, 1.0F};
                uiRenderer.drawFilledRect(slot.x, slot.y, slot.width, slot.height, emptySlot);
                uiRenderer.drawOutlineRect(slot.x, slot.y, slot.width, slot.height, slotBorder);
            }

            if (!playerInventory.isSlotOccupied(index)) {
                continue;
            }

            const systems::ItemMetadata& item = *playerInventory.slotAt(index).item;
            float iconFill[4]{};
            float iconBorder[4]{};
            rarityColors(item.rarity, iconFill, iconBorder);

            const float iconPad = 5.0F;
            uiRenderer.drawFilledRect(
                slot.x + iconPad,
                slot.y + iconPad,
                slot.width - iconPad * 2.0F,
                slot.height - iconPad * 2.0F,
                iconFill);
            uiRenderer.drawOutlineRect(
                slot.x + iconPad,
                slot.y + iconPad,
                slot.width - iconPad * 2.0F,
                slot.height - iconPad * 2.0F,
                iconBorder);
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
        title << characterClassName(selectedClass) << "  |  Lv " << base.level;
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
            "Click gear to unequip | Click bag items to equip | C level up | Esc close",
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
        goldLine << "Gold " << tradeSystem.playerGold();
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

        std::ostringstream bagTitle;
        bagTitle << "Backpack " << playerInventory.usedSlots() << "/"
                 << playerInventory.capacity();
        drawBoundedText(layout.bagHeader, bagTitle.str(), layoutScale.dim(1.35F), subtitleColor);

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

    void renderItemTooltipBackground() {
        cachedItemTooltip_.reset();

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

        std::string tooltip = systems::formatItemTooltip(*hoveredItem);
        if (stateManager.currentState() == gameplay::GameState::TRADING) {
            if (hoveredTradePlayerSlot_.has_value()) {
                tooltip += "\nClick to sell for " +
                            std::to_string(systems::blacksmithSellPrice(*hoveredItem)) + " gold";
            } else if (hoveredTradeVendorSlot_.has_value()) {
                tooltip += "\nClick to buy for " +
                            std::to_string(systems::blacksmithBuyPrice(*hoveredItem)) + " gold";
            }
        }

        std::vector<std::string> lines;
        std::istringstream stream(tooltip);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }

        constexpr float kTooltipScale = 1.85F;
        const ui::TooltipBoxLayout tooltipLayout =
            computeItemTooltipLayout(anchorX, anchorY, lines, kTooltipScale);
        drawTooltipBackground(tooltipLayout);
        cachedItemTooltip_ = CachedTooltip{tooltipLayout, std::move(lines), kTooltipScale};
    }

    void renderItemTooltipText() const {
        if (!cachedItemTooltip_.has_value()) {
            return;
        }

        drawTextTooltipTextOnly(
            cachedItemTooltip_->layout, cachedItemTooltip_->lines, cachedItemTooltip_->scale);
    }

    [[nodiscard]] std::vector<std::string> buildCharacterScreenLines() const {
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const int upgradeCost = systems::soulUpgradeCost(base.statUpgradesPurchased);
        const bool inTown = zoneManager.activeZone() == gameplay::WorldZone::TOWN;
        std::vector<std::string> lines;

        std::ostringstream soulsLine;
        soulsLine << "Souls carried: " << base.carriedSouls;
        if (!inTown) {
            soulsLine << " (lost on death)";
        }
        lines.push_back(soulsLine.str());

        std::ostringstream gainLine;
        gainLine << "Kill streak " << systems::formatSoulGainMultiplier(base.soulGainMultiplier)
                 << " | Next upgrade " << upgradeCost << " souls";
        lines.push_back(gainLine.str());

        const std::vector<std::string> statLines = buildCharacterStatLines(false);
        lines.insert(lines.end(), statLines.begin(), statLines.end());
        return lines;
    }

    void renderCharacterScreen() {
        if (!overlayState.characterScreen().visible) {
            return;
        }

        const ui::CharacterPanelLayout panel = ui::computeCharacterPanelLayout(currentUiScale());
        if (uiAssets.isLoaded()) {
            const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                panel.panel.x,
                panel.panel.y,
                panel.panel.width,
                panel.panel.height,
                currentUiScale().dim(16.0F),
                white);
        } else {
            const float panelColor[4] = {0.08F, 0.09F, 0.14F, 0.96F};
            const float borderColor[4] = {0.55F, 0.4F, 0.95F, 1.0F};
            uiRenderer.drawFilledRect(
                panel.panel.x, panel.panel.y, panel.panel.width, panel.panel.height, panelColor);
            uiRenderer.drawOutlineRect(
                panel.panel.x, panel.panel.y, panel.panel.width, panel.panel.height, borderColor);
        }

        if (uiAssets.isLoaded()) {
            const float portraitFrame[4] = {1.0F, 1.0F, 1.0F, 0.95F};
            uiRenderer.drawNineSlice(
                uiAssets.inventoryPanelSlice(),
                panel.portrait.x - currentUiScale().dim(4.0F),
                panel.portrait.y - currentUiScale().dim(4.0F),
                panel.portrait.width + currentUiScale().dim(8.0F),
                panel.portrait.height + currentUiScale().dim(8.0F),
                currentUiScale().dim(8.0F),
                portraitFrame);
        }

        drawPortraitInRect(panel.portrait);
        drawResourceBars(panel.hpBar, panel.xpBar);

        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const bool inTown = zoneManager.activeZone() == gameplay::WorldZone::TOWN;
        const int upgradeCost = systems::soulUpgradeCost(base.statUpgradesPurchased);
        const bool canUpgrade = inTown && base.carriedSouls >= upgradeCost;

        drawPanelButtonBackground(
            panel.upgradeStrengthButton,
            hoveredStatUpgradeButton_.has_value() && *hoveredStatUpgradeButton_ == 0,
            canUpgrade);
        drawPanelButtonBackground(
            panel.upgradeDexterityButton,
            hoveredStatUpgradeButton_.has_value() && *hoveredStatUpgradeButton_ == 1,
            canUpgrade);
        drawPanelButtonBackground(
            panel.upgradeVitalityButton,
            hoveredStatUpgradeButton_.has_value() && *hoveredStatUpgradeButton_ == 2,
            canUpgrade);
    }

    void renderCharacterScreenText() const {
        if (!overlayState.characterScreen().visible) {
            return;
        }

        const ui::CharacterPanelLayout panel = ui::computeCharacterPanelLayout(currentUiScale());
        const ui::CharacterScreenData& base = overlayState.characterScreen();
        const bool inTown = zoneManager.activeZone() == gameplay::WorldZone::TOWN;
        const int upgradeCost = systems::soulUpgradeCost(base.statUpgradesPurchased);
        const bool canUpgrade = inTown && base.carriedSouls >= upgradeCost;
        const float titleColor[4] = {0.95F, 0.92F, 1.0F, 1.0F};
        const float labelColor[4] = {0.8F, 0.85F, 0.98F, 1.0F};
        const float hintColor[4] = {0.55F, 0.58F, 0.64F, 0.85F};
        const float headerColor[4] = {0.92F, 0.86F, 0.55F, 1.0F};

        std::ostringstream title;
        title << characterClassName(selectedClass) << "  |  Lv " << base.level << "  [C]";
        drawBoundedText(panel.titleBand, title.str(), panel.titleScale, titleColor);

        drawResourceBarLabels(panel.hpBar, panel.xpBar);

        const float goldColor[4] = {0.95F, 0.82F, 0.28F, 1.0F};
        std::ostringstream goldLine;
        goldLine << "Gold " << tradeSystem.playerGold();
        drawBoundedText(panel.goldLabel, goldLine.str(), panel.statLabelScale, goldColor);

        std::ostringstream upgradeHeader;
        if (inTown) {
            upgradeHeader << "Level Up — spend " << upgradeCost << " souls per stat";
        } else {
            upgradeHeader << "Level Up — return to Town to spend souls";
        }
        drawBoundedText(panel.upgradeHeader, upgradeHeader.str(), panel.statLabelScale, headerColor);

        drawPanelButtonLabel(panel.upgradeStrengthButton, "+ STR", canUpgrade);
        drawPanelButtonLabel(panel.upgradeDexterityButton, "+ DEX", canUpgrade);
        drawPanelButtonLabel(panel.upgradeVitalityButton, "+ VIT", canUpgrade);

        const std::vector<std::string> lines = buildCharacterScreenLines();
        const float lineHeight = std::min(
            currentUiScale().dim(18.0F),
            panel.statsText.height / std::max<std::size_t>(lines.size(), 1));
        float cursorY = panel.statsText.y;
        for (const std::string& line : lines) {
            const ui::Rect row{panel.statsText.x, cursorY, panel.statsText.width, lineHeight};
            drawBoundedText(row, line, panel.statLabelScale, labelColor);
            cursorY += lineHeight;
            if (cursorY + lineHeight > panel.statsText.y + panel.statsText.height) {
                break;
            }
        }

        const char* footerHint = inTown ? "Click +STR / +DEX / +VIT to level up  |  I gear  |  Esc close"
                                        : "Return to Town to level up  |  I gear  |  Esc close";
        drawBoundedText(panel.footerHint, footerHint, panel.statLabelScale, hintColor);
    }

    void drawTradeItemSlot(
        const ui::Rect& slot,
        const systems::ItemMetadata& item,
        bool hovered) const {
        const float emptySlot[4] = {0.12F, 0.14F, 0.18F, 1.0F};
        const float slotBorder[4] = {0.35F, 0.38F, 0.45F, hovered ? 1.0F : 0.85F};
        uiRenderer.drawFilledRect(slot.x, slot.y, slot.width, slot.height, emptySlot);
        uiRenderer.drawOutlineRect(slot.x, slot.y, slot.width, slot.height, slotBorder);

        float iconFill[4]{};
        float iconBorder[4]{};
        rarityColors(item.rarity, iconFill, iconBorder);
        const float iconPad = 5.0F;
        uiRenderer.drawFilledRect(
            slot.x + iconPad,
            slot.y + iconPad,
            slot.width - iconPad * 2.0F,
            slot.height - iconPad * 2.0F,
            iconFill);
        uiRenderer.drawOutlineRect(
            slot.x + iconPad,
            slot.y + iconPad,
            slot.width - iconPad * 2.0F,
            slot.height - iconPad * 2.0F,
            iconBorder);
    }

    void renderTradePanels() {
        if (stateManager.currentState() != gameplay::GameState::TRADING) {
            return;
        }

        const ui::TradeWindowLayout layout = ui::computeTradeWindowLayout(currentUiScale());
        const float panelColor[4] = {0.06F, 0.07F, 0.1F, 0.93F};
        const float borderPlayer[4] = {0.35F, 0.7F, 0.95F, 1.0F};
        const float borderVendor[4] = {0.95F, 0.55F, 0.2F, 1.0F};

        uiRenderer.drawFilledRect(
            layout.playerPanel.x,
            layout.playerPanel.y,
            layout.playerPanel.width,
            layout.playerPanel.height,
            panelColor);
        uiRenderer.drawOutlineRect(
            layout.playerPanel.x,
            layout.playerPanel.y,
            layout.playerPanel.width,
            layout.playerPanel.height,
            borderPlayer);
        uiRenderer.drawFilledRect(
            layout.vendorPanel.x,
            layout.vendorPanel.y,
            layout.vendorPanel.width,
            layout.vendorPanel.height,
            panelColor);
        uiRenderer.drawOutlineRect(
            layout.vendorPanel.x,
            layout.vendorPanel.y,
            layout.vendorPanel.width,
            layout.vendorPanel.height,
            borderVendor);

        const float borderServices[4] = {0.75F, 0.45F, 0.95F, 1.0F};
        uiRenderer.drawFilledRect(
            layout.servicesPanel.x,
            layout.servicesPanel.y,
            layout.servicesPanel.width,
            layout.servicesPanel.height,
            panelColor);
        uiRenderer.drawOutlineRect(
            layout.servicesPanel.x,
            layout.servicesPanel.y,
            layout.servicesPanel.width,
            layout.servicesPanel.height,
            borderServices);

        for (int index = 0; index < playerInventory.capacity(); ++index) {
            const ui::Rect slot = layout.playerSlotRect(index);
            const bool hovered =
                hoveredTradePlayerSlot_.has_value() && *hoveredTradePlayerSlot_ == index;
            if (!playerInventory.isSlotOccupied(index)) {
                const float emptySlot[4] = {0.1F, 0.11F, 0.15F, 0.9F};
                const float slotBorder[4] = {0.3F, 0.34F, 0.4F, hovered ? 1.0F : 0.7F};
                uiRenderer.drawFilledRect(slot.x, slot.y, slot.width, slot.height, emptySlot);
                uiRenderer.drawOutlineRect(slot.x, slot.y, slot.width, slot.height, slotBorder);
                continue;
            }
            drawTradeItemSlot(slot, *playerInventory.slotAt(index).item, hovered);
        }

        for (int index = 0; index < vendorInventory.capacity(); ++index) {
            const ui::Rect slot = layout.vendorSlotRect(index);
            const bool hovered =
                hoveredTradeVendorSlot_.has_value() && *hoveredTradeVendorSlot_ == index;
            if (!vendorInventory.isSlotOccupied(index)) {
                const float emptySlot[4] = {0.1F, 0.11F, 0.15F, 0.9F};
                const float slotBorder[4] = {0.4F, 0.28F, 0.18F, hovered ? 1.0F : 0.7F};
                uiRenderer.drawFilledRect(slot.x, slot.y, slot.width, slot.height, emptySlot);
                uiRenderer.drawOutlineRect(slot.x, slot.y, slot.width, slot.height, slotBorder);
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
            const float lockedFill[4] = {0.14F, 0.12F, 0.12F, 0.95F};
            const float unlockedFill[4] = {0.18F, 0.22F, 0.32F, hovered ? 1.0F : 0.92F};
            const float border[4] = {unlocked ? 0.55F : 0.35F, unlocked ? 0.72F : 0.28F, 0.95F, 1.0F};
            uiRenderer.drawFilledRect(
                button.x,
                button.y,
                button.width,
                button.height,
                unlocked ? unlockedFill : lockedFill);
            uiRenderer.drawOutlineRect(button.x, button.y, button.width, button.height, border);
        }
    }

    void renderTradePanelsText() const {
        if (stateManager.currentState() != gameplay::GameState::TRADING) {
            return;
        }

        const ui::TradeWindowLayout layout = ui::computeTradeWindowLayout(currentUiScale());
        const float labelColor[4] = {0.92F, 0.95F, 1.0F, 1.0F};
        const float mutedColor[4] = {0.55F, 0.58F, 0.64F, 0.9F};
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
            const float textColor[4] = {unlocked ? 0.92F : 0.55F, unlocked ? 0.95F : 0.5F, 1.0F, 1.0F};
            drawBoundedText(button, descriptor.label, layout.serviceScale, textColor);
            if (!unlocked) {
                const ui::Rect hintRect{
                    button.x,
                    button.y + button.height * 0.55F,
                    button.width,
                    button.height * 0.4F};
                drawBoundedText(
                    hintRect,
                    systems::blacksmithUnlockHint(service),
                    layout.serviceScale * 0.82F,
                    mutedColor);
            }
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

    void renderInGameUi(float mouseX, float mouseY) {
        ensureUiHitRegionsBuilt();
        updateInventoryHover(mouseX, mouseY);

        uiRenderer.beginFrame();
        renderPlayerStatusHud();
        renderHudInfoStrip();
        renderTargetMobHud();
        renderMinimap();
        renderInventoryOverlay();
        renderCharacterScreen();
        renderTradePanels();
        renderInteractableTooltipBackground(mouseX, mouseY);
        renderFloatingCombatTextBackgrounds();
        uiRenderer.endFrame();

        textRenderer.beginOverlay();
        renderPlayerStatusHudLabels();
        renderHudInfoStripLabel();
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
    }

    void update(float deltaSeconds) {
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

        handleInGameInput(deltaSeconds);

        if (!gamePaused && zoneManager.activeZone() == gameplay::WorldZone::PLAINS) {
            zoneManager.updatePlainsSimulation(deltaSeconds, toVec3(playerPosition));
            updateMobMeleeThreats(deltaSeconds);
        }

        overlayState.syncInventoryVisibility(playerInventory.usedSlots());
        if (!gamePaused) {
            updatePlayerSpriteAnimation(deltaSeconds);
            finalizeEntityMotionTracking();
        }
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

        renderWorld();
        if (gamePaused) {
            renderPauseOverlay();
        } else {
            renderInGameUi(mouseX, mouseY);
        }
    }
};

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
    impl_->update(deltaSeconds);
    impl_->renderFrame();
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
