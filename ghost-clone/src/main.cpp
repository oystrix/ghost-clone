#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "GhostFrame.hpp"
#include "GhostRecorder.hpp"
#include "GhostPlayback.hpp"
#include "GhostMenuPopup.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Глобальные указатели на активные recorder/playback текущего PlayLayer.
// Они нужны, чтобы хук PlayerObject::update мог дописывать кадры записи,
// не имея прямого доступа к Fields модифицированного PlayLayer.
// Указатели действительны только пока жив соответствующий PlayLayer.
// ---------------------------------------------------------------------------
static GhostRecorder* g_activeRecorder = nullptr;
static GhostPlayback* g_activePlayback = nullptr;

static GhostGamemode currentGamemode(PlayerObject* p) {
    if (p->m_isShip) return GhostGamemode::Ship;
    if (p->m_isBird) return GhostGamemode::Ufo;
    if (p->m_isBall) return GhostGamemode::Ball;
    if (p->m_isDart) return GhostGamemode::Wave;
    if (p->m_isRobot) return GhostGamemode::Robot;
    if (p->m_isSpider) return GhostGamemode::Spider;
    if (p->m_isSwing) return GhostGamemode::Swing;
    return GhostGamemode::Cube;
}

// -------------------------- Хук PlayerObject --------------------------
// Дописывает по одному кадру записи на каждый тик, если это m_player1
// активного PlayLayer и запись сейчас идёт.
class $modify(GCPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        auto pl = PlayLayer::get();
        if (!pl || this != pl->m_player1) return;
        if (!g_activeRecorder || !g_activeRecorder->isRecording()) return;

        GhostFrame frame;
        frame.x = this->getPositionX();
        frame.y = this->getPositionY();
        frame.rotation = this->getRotation();
        frame.flipX = this->isFlipX();
        frame.flipY = this->isFlipY();
        frame.isUpsideDown = this->m_isUpsideDown;
        frame.isDashing = this->m_isDashing;
        frame.isHidden = !this->isVisible();
        frame.gamemode = currentGamemode(this);

        g_activeRecorder->addFrame(frame);
    }
};

// -------------------------- Хук PlayLayer --------------------------
class $modify(GCPlayLayer, PlayLayer) {
    struct Fields {
        GhostRecorder recorder;
        GhostPlayback playback;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        int levelID = level ? level->m_levelID.value() : 0;

        // Запись выключена в Practice Mode — там результат "нечестный"
        bool autoRecord = Mod::get()->getSettingValue<bool>("auto-record");
        if (autoRecord && !this->m_isPracticeMode && levelID != 0) {
            m_fields->recorder.begin(levelID);
        }

        // Загружаем и показываем призрака из лучшей сохранённой попытки
        bool ghostEnabledForLevel = Mod::get()->getSavedValue<bool>(
            "ghost-enabled-" + std::to_string(levelID), true
        );
        if (levelID != 0 && m_fields->playback.load(levelID)) {
            m_fields->playback.createClone(this);
            m_fields->playback.setEnabled(ghostEnabledForLevel);
        }

        g_activeRecorder = &m_fields->recorder;
        g_activePlayback = &m_fields->playback;

        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        // Новая попытка — старую незавершённую запись отбрасываем и начинаем заново
        if (!this->m_isPracticeMode && Mod::get()->getSettingValue<bool>("auto-record")) {
            int levelID = this->m_level ? this->m_level->m_levelID.value() : 0;
            m_fields->recorder.reset();
            m_fields->recorder.begin(levelID);
        }

        m_fields->playback.restart();
    }

    void update(float dt) {
        PlayLayer::update(dt);
        m_fields->playback.update(dt);
    }

    // Игрок прошёл уровень — фиксируем 100% и пробуем сохранить как лучшую попытку
    void levelComplete() {
        m_fields->recorder.saveIfBest(100.f);
        PlayLayer::levelComplete();
    }

    void onQuit() {
        // При выходе с уровня тоже сохраняем прогресс (по последнему проценту),
        // если текущая попытка оказалась лучше предыдущей записи
        if (this->m_player1) {
            float percent = this->getCurrentPercent();
            m_fields->recorder.saveIfBest(percent);
        }

        g_activeRecorder = nullptr;
        g_activePlayback = nullptr;

        PlayLayer::onQuit();
    }
};

// Игрок умер — тоже подходящий момент сохранить прогресс, если процент лучше
class $modify(GCPlayLayerDeath, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        if (player == this->m_player1) {
            auto self = static_cast<GCPlayLayer*>(this);
            float percent = this->getCurrentPercent();
            self->m_fields->recorder.saveIfBest(percent);
        }
    }
};

// -------------------------- Хук PauseLayer --------------------------
// Добавляет кнопку открытия меню призрака в стандартное меню паузы.
class $modify(GCPauseLayer, PauseLayer) {
    bool init(bool isDualMode) {
        if (!PauseLayer::init(isDualMode)) return false;

        if (!Mod::get()->getSettingValue<bool>("show-pause-button")) {
            return true;
        }

        auto menu = this->getChildByID("pause-menu");
        if (!menu) {
            // На некоторых версиях меню лежит без ID — берём первый CCMenu
            menu = this->getChildByType<CCMenu>(0);
        }
        if (!menu) return true;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_replayBtn_001.png");
        if (!sprite) {
            sprite = ButtonSprite::create("G");
        }
        sprite->setScale(0.9f);

        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(GCPauseLayer::onOpenGhostMenu)
        );
        btn->setID("ghost-clone-button"_spr);
        btn->setPosition({-2.f, -2.f}); // корректируется автоматически layout'ом меню

        auto castMenu = typeinfo_cast<CCMenu*>(menu);
        if (castMenu) {
            castMenu->addChild(btn);
            castMenu->updateLayout();
        }

        return true;
    }

    void onOpenGhostMenu(CCObject*) {
        auto pl = PlayLayer::get();
        int levelID = (pl && pl->m_level) ? pl->m_level->m_levelID.value() : 0;
        GhostMenuPopup::create(levelID)->show();
    }
};
