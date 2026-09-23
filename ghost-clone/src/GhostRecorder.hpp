#pragma once
#include "GhostFrame.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// Отвечает за запись текущей попытки прохождения.
// Кадры добавляются из хука PlayerObject::update (см. main.cpp).
class GhostRecorder {
public:
    void begin(int levelID) {
        m_levelID = levelID;
        m_frames.clear();
        m_recording = true;
    }

    void stop() {
        m_recording = false;
    }

    bool isRecording() const { return m_recording; }

    void addFrame(GhostFrame const& frame) {
        if (m_recording) {
            m_frames.push_back(frame);
        }
    }

    // Сбрасывает текущую (незавершённую) запись — вызывается при рестарте уровня
    void reset() {
        m_frames.clear();
    }

    // Сохраняет запись на диск, только если процент лучше уже сохранённого.
    // Возвращает true, если файл был перезаписан.
    bool saveIfBest(float percent) {
        if (m_frames.empty() || m_levelID == 0) return false;

        auto path = ghostFilePath(m_levelID);
        float previousBest = -1.f;

        if (std::filesystem::exists(path)) {
            auto readRes = file::readString(path);
            if (readRes.isOk()) {
                auto parsed = matjson::parse(readRes.unwrap());
                if (parsed.isOk()) {
                    previousBest = GhostRecording::fromJson(parsed.unwrap()).bestPercent;
                }
            }
        }

        if (percent <= previousBest) {
            // Старая запись всё ещё лучше — новую не сохраняем
            return false;
        }

        GhostRecording rec;
        rec.levelID = m_levelID;
        rec.bestPercent = percent;
        rec.fps = 240.0;
        rec.frames = m_frames;

        auto dir = ghostSaveDir();
        (void) file::createDirectoryAll(dir);

        auto writeRes = file::writeString(path, rec.toJson().dump());
        if (writeRes.isErr()) {
            log::warn("Ghost Clone: не удалось сохранить запись для уровня {}: {}",
                      m_levelID, writeRes.unwrapErr());
            return false;
        }

        log::info("Ghost Clone: сохранена новая лучшая запись для уровня {} ({}%)",
                   m_levelID, percent);
        return true;
    }

    static std::filesystem::path ghostSaveDir() {
        return Mod::get()->getSaveDir() / "ghosts";
    }

    static std::filesystem::path ghostFilePath(int levelID) {
        return ghostSaveDir() / fmt::format("ghost_{}.json", levelID);
    }

private:
    int m_levelID = 0;
    bool m_recording = false;
    std::vector<GhostFrame> m_frames;
};
