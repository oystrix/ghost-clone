#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/cocos.hpp>

using namespace geode::prelude;

// Красивое всплывающее окно управления призраком в стиле стандартных
// GD-попапов (GJ_square01.png + заголовок + кнопки-плашки).
class GhostMenuPopup : public geode::Popup<int> {
protected:
    int m_levelID = 0;
    CCMenuItemToggler* m_enableToggle = nullptr;
    Slider* m_opacitySlider = nullptr;
    CCLabelBMFont* m_opacityLabel = nullptr;
    ScrollLayer* m_recordingsList = nullptr;

    bool setup(int levelID) override;

    void onToggleEnabled(CCObject* sender);
    void onOpacityChanged(CCObject* sender);
    void onPickColor(CCObject* sender);
    void onDeleteRecording(CCObject* sender);

    void rebuildRecordingsList();
    CCArray* collectAllRecordings();

public:
    static GhostMenuPopup* create(int currentLevelID);
};
