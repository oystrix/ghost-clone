#pragma once
#include "GhostFrame.hpp"
#include "GhostRecorder.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Отвечает за создание клона-призрака и его плавное воспроизведение
// поверх записанных кадров.
class GhostPlayback {
public:
    // Пытается загрузить лучшую запись для уровня. Возвращает true, если есть что играть.
    bool load(int levelID) {
        m_recording = GhostRecording{};
        m_time = 0.0;
        m_frameIndex = 0;

        auto path = GhostRecorder::ghostFilePath(levelID);
        if (!std::filesystem::exists(path)) {
            return false;
        }

        auto readRes = file::readString(path);
        if (readRes.isErr()) return false;

        auto parsed = matjson::parse(readRes.unwrap());
        if (parsed.isErr()) return false;

        m_recording = GhostRecording::fromJson(parsed.unwrap());
        return !m_recording.frames.empty();
    }

    bool hasGhost() const { return !m_recording.frames.empty(); }
    float bestPercent() const { return m_recording.bestPercent; }

    // Создаёт визуальный клон, копируя иконки/цвета из настоящего игрока.
    void createClone(PlayLayer* pl) {
        if (!hasGhost() || !pl || !pl->m_player1) return;

        // PlayerObject::create(iconID, colorID, layer, parent, useSecondColor)
        // Используем те же значения, что и у оригинального игрока —
        // так клон гарантированно выглядит идентично.
        auto real = pl->m_player1;
        m_ghost = PlayerObject::create(real->m_playerIconType, 1, pl, pl, false);
        if (!m_ghost) return;

        m_ghost->setID("ghost-clone-player"_spr);
        m_ghost->setPosition(real->getPosition());
        m_ghost->m_isDead = false;

        // Копируем игровой скин целиком (иконки по режимам, цвета, UFO-режим и т.д.)
        GameManager* gm = GameManager::get();
        m_ghost->updatePlayerFrame(gm->getPlayerFrame());
        m_ghost->updatePlayerShipFrame(gm->getPlayerShip());
        m_ghost->updatePlayerBallFrame(gm->getPlayerBall());
        m_ghost->updatePlayerBirdFrame(gm->getPlayerBird());
        m_ghost->updatePlayerDartFrame(gm->getPlayerDart());
        m_ghost->updatePlayerRobotFrame(gm->getPlayerRobot());
        m_ghost->updatePlayerSpiderFrame(gm->getPlayerSpider());
        m_ghost->updatePlayerSwingFrame(gm->getPlayerSwing());
        m_ghost->updatePlayerGlowColor();

        applySettings();

        // Клон не должен участвовать в физике/коллизиях уровня —
        // он чисто визуальный и рисуется поверх обычных объектов игрока.
        m_ghost->setTouchEnabled(false);
        m_ghost->m_hardStreak->setVisible(false);
        m_ghost->m_waveTrail->setVisible(false);

        pl->m_objectLayer->addChild(m_ghost, real->getZOrder() - 1);
    }

    // Применяет прозрачность/цвет из настроек мода (можно вызывать повторно из меню)
    void applySettings() {
        if (!m_ghost) return;
        float opacity = Mod::get()->getSettingValue<double>("ghost-opacity");
        auto color = Mod::get()->getSettingValue<ccColor3B>("ghost-color");
        auto op = static_cast<GLubyte>(std::clamp(opacity, 0.0f, 1.0f) * 255.f);

        m_ghost->setOpacity(op);
        m_ghost->setColor(color);
        m_ghost->m_playerSpeedIcon->setOpacity(op);
    }

    // Продвигает воспроизведение на dt секунд и обновляет позицию клона
    // с линейной интерполяцией между соседними записанными кадрами.
    void update(float dt) {
        if (!m_ghost || m_recording.frames.empty()) return;

        m_time += dt;
        double frameDuration = 1.0 / m_recording.fps;
        double exactIndex = m_time / frameDuration;

        size_t i0 = static_cast<size_t>(std::floor(exactIndex));
        size_t i1 = i0 + 1;
        float t = static_cast<float>(exactIndex - static_cast<double>(i0));

        auto& frames = m_recording.frames;
        if (i1 >= frames.size()) {
            // Запись закончилась — прячем призрака, чтобы он не "застыл" на финише
            m_ghost->setVisible(false);
            return;
        }

        auto const& a = frames[i0];
        auto const& b = frames[i1];

        float x = a.x + (b.x - a.x) * t;
        float y = a.y + (b.y - a.y) * t;
        float rot = a.rotation + (b.rotation - a.rotation) * t;

        m_ghost->setVisible(!a.isHidden);
        m_ghost->setPosition({x, y});
        m_ghost->setRotation(rot);
        m_ghost->setFlipX(a.flipX);
        m_ghost->setFlipY(a.flipY);

        syncGamemode(a.gamemode);
    }

    // Перематывает воспроизведение в начало (вызывается при рестарте попытки)
    void restart() {
        m_time = 0.0;
        m_frameIndex = 0;
        if (m_ghost) m_ghost->setVisible(true);
    }

    void setEnabled(bool enabled) {
        m_enabled = enabled;
        if (m_ghost) m_ghost->setVisible(enabled && hasGhost());
    }

    bool isEnabled() const { return m_enabled; }

    void destroy() {
        if (m_ghost) {
            m_ghost->removeFromParent();
            m_ghost = nullptr;
        }
        m_recording = GhostRecording{};
    }

    ~GhostPlayback() { destroy(); }

private:
    // Переключает визуальный режим клона (куб/корабль/...), если он изменился
    // относительно текущего кадра. Дёшево, так как сравнивается с последним состоянием.
    void syncGamemode(GhostGamemode gm) {
        if (!m_ghost || gm == m_lastGamemode) return;
        m_lastGamemode = gm;

        m_ghost->m_isShip = gm == GhostGamemode::Ship;
        m_ghost->m_isBall = gm == GhostGamemode::Ball;
        m_ghost->m_isBird = gm == GhostGamemode::Ufo;
        m_ghost->m_isDart = gm == GhostGamemode::Wave;
        m_ghost->m_isRobot = gm == GhostGamemode::Robot;
        m_ghost->m_isSpider = gm == GhostGamemode::Spider;
        m_ghost->m_isSwing = gm == GhostGamemode::Swing;

        m_ghost->updatePlayerScale();
        m_ghost->updateJumpVariables();
    }

    PlayerObject* m_ghost = nullptr;
    GhostRecording m_recording;
    double m_time = 0.0;
    size_t m_frameIndex = 0;
    bool m_enabled = true;
    GhostGamemode m_lastGamemode = GhostGamemode::Cube;
};
