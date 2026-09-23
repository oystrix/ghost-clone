#pragma once
#include <matjson.hpp>

// Игровые режимы игрока. Числовое значение используется для сериализации,
// поэтому порядок менять нельзя.
enum class GhostGamemode : int {
    Cube = 0,
    Ship = 1,
    Ball = 2,
    Ufo = 3,
    Wave = 4,
    Robot = 5,
    Spider = 6,
    Swing = 7
};

// Один "снимок" состояния игрока в конкретный момент времени.
// Кадры пишутся каждый tick, поэтому структура намеренно компактная.
struct GhostFrame {
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    bool flipX = false;
    bool flipY = false;
    bool isUpsideDown = false;
    bool isDashing = false;
    bool isHidden = false;      // игрок невидим (например во время смерти/телепорта)
    GhostGamemode gamemode = GhostGamemode::Cube;

    matjson::Value toJson() const {
        auto obj = matjson::Value::object();
        obj["x"] = x;
        obj["y"] = y;
        obj["r"] = rotation;
        obj["fx"] = flipX;
        obj["fy"] = flipY;
        obj["ud"] = isUpsideDown;
        obj["ds"] = isDashing;
        obj["hd"] = isHidden;
        obj["gm"] = static_cast<int>(gamemode);
        return obj;
    }

    static GhostFrame fromJson(matjson::Value const& j) {
        GhostFrame f;
        f.x = j.contains("x") ? j["x"].asDouble().unwrapOr(0.0) : 0.0;
        f.y = j.contains("y") ? j["y"].asDouble().unwrapOr(0.0) : 0.0;
        f.rotation = j.contains("r") ? j["r"].asDouble().unwrapOr(0.0) : 0.0;
        f.flipX = j.contains("fx") ? j["fx"].asBool().unwrapOr(false) : false;
        f.flipY = j.contains("fy") ? j["fy"].asBool().unwrapOr(false) : false;
        f.isUpsideDown = j.contains("ud") ? j["ud"].asBool().unwrapOr(false) : false;
        f.isDashing = j.contains("ds") ? j["ds"].asBool().unwrapOr(false) : false;
        f.isHidden = j.contains("hd") ? j["hd"].asBool().unwrapOr(false) : false;
        f.gamemode = j.contains("gm")
            ? static_cast<GhostGamemode>(j["gm"].asInt().unwrapOr(0))
            : GhostGamemode::Cube;
        return f;
    }
};

// Метаданные записи целиком: список кадров + процент прохождения,
// с которым запись была сделана.
struct GhostRecording {
    int levelID = 0;
    float bestPercent = 0.f;
    double fps = 240.0; // частота записи (кадров в секунду) для корректной интерполяции
    std::vector<GhostFrame> frames;

    matjson::Value toJson() const {
        auto obj = matjson::Value::object();
        obj["levelID"] = levelID;
        obj["bestPercent"] = bestPercent;
        obj["fps"] = fps;
        auto arr = matjson::Value::array();
        for (auto& f : frames) arr.push(f.toJson());
        obj["frames"] = arr;
        return obj;
    }

    static GhostRecording fromJson(matjson::Value const& j) {
        GhostRecording r;
        r.levelID = j.contains("levelID") ? j["levelID"].asInt().unwrapOr(0) : 0;
        r.bestPercent = j.contains("bestPercent") ? j["bestPercent"].asDouble().unwrapOr(0.0) : 0.0;
        r.fps = j.contains("fps") ? j["fps"].asDouble().unwrapOr(240.0) : 240.0;
        if (j.contains("frames") && j["frames"].isArray()) {
            for (auto& fj : j["frames"]) {
                r.frames.push_back(GhostFrame::fromJson(fj));
            }
        }
        return r;
    }
};
