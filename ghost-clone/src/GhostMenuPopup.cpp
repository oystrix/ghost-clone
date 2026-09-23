#include "GhostMenuPopup.hpp"
#include "GhostRecorder.hpp"
#include "GhostFrame.hpp"

// Небольшой пресет цветов — вместо полноценного color-picker'а (которого нет
// в публичном Geode UI API) кнопка цвета циклично перебирает эти значения.
// Этого достаточно для "покраски" силуэта призрака.
static const std::vector<ccColor3B> GHOST_COLOR_PRESETS = {
    {255, 255, 255}, // белый (по умолчанию)
    {0, 255, 255},   // голубой
    {0, 255, 0},      // зелёный
    {255, 0, 255},   // пурпурный
    {255, 255, 0},   // жёлтый
    {255, 100, 100}, // мягкий красный
};

GhostMenuPopup* GhostMenuPopup::create(int currentLevelID) {
    auto ret = new GhostMenuPopup();
    if (ret && ret->initAnchored(360.f, 260.f, currentLevelID, "GJ_square01.png")) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool GhostMenuPopup::setup(int levelID) {
    m_levelID = levelID;
    this->setTitle("Ghost Clone");

    auto winSize = m_mainLayer->getContentSize();

    // ---- Верхняя плашка: вкл/выкл призрака ----
    auto enableLabel = CCLabelBMFont::create("Показывать призрака", "bigFont.fnt");
    enableLabel->setScale(0.42f);
    enableLabel->setAnchorPoint({0.f, 0.5f});
    enableLabel->setPosition({22.f, winSize.height - 55.f});
    m_mainLayer->addChild(enableLabel);

    auto toggleMenu = CCMenu::create();
    toggleMenu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(toggleMenu);

    bool currentlyEnabled = Mod::get()->getSavedValue<bool>("ghost-enabled-" + std::to_string(levelID), true);
    m_enableToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(GhostMenuPopup::onToggleEnabled), 0.7f
    );
    m_enableToggle->toggle(!currentlyEnabled); // библиотека инвертирует состояние при создании
    m_enableToggle->setPosition({winSize.width - 35.f, winSize.height - 55.f});
    toggleMenu->addChild(m_enableToggle);

    // ---- Слайдер прозрачности ----
    auto opacityTitle = CCLabelBMFont::create("Прозрачность", "bigFont.fnt");
    opacityTitle->setScale(0.42f);
    opacityTitle->setAnchorPoint({0.f, 0.5f});
    opacityTitle->setPosition({22.f, winSize.height - 90.f});
    m_mainLayer->addChild(opacityTitle);

    m_opacitySlider = Slider::create(this, menu_selector(GhostMenuPopup::onOpacityChanged));
    m_opacitySlider->setPosition({winSize.width / 2.f, winSize.height - 115.f});
    m_opacitySlider->setScale(0.7f);
    float savedOpacity = Mod::get()->getSettingValue<double>("ghost-opacity");
    m_opacitySlider->setValue(std::clamp(savedOpacity, 0.f, 1.f));
    m_mainLayer->addChild(m_opacitySlider);

    m_opacityLabel = CCLabelBMFont::create(
        fmt::format("{}%", static_cast<int>(savedOpacity * 100.f)).c_str(), "bigFont.fnt"
    );
    m_opacityLabel->setScale(0.35f);
    m_opacityLabel->setPosition({winSize.width - 30.f, winSize.height - 90.f});
    m_mainLayer->addChild(m_opacityLabel);

    // ---- Кнопка выбора цвета ----
    auto colorMenu = CCMenu::create();
    colorMenu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(colorMenu);

    auto colorSprite = CCScale9Sprite::create("square02_small.png");
    colorSprite->setContentSize({120.f, 30.f});
    auto savedColor = Mod::get()->getSettingValue<ccColor3B>("ghost-color");
    colorSprite->setColor(savedColor);

    auto colorLabel = CCLabelBMFont::create("Цвет призрака", "bigFont.fnt");
    colorLabel->setScale(0.4f);
    colorLabel->setPosition(colorSprite->getContentSize() / 2.f);
    colorSprite->addChild(colorLabel);

    auto colorBtn = CCMenuItemSpriteExtra::create(
        colorSprite, this, menu_selector(GhostMenuPopup::onPickColor)
    );
    colorBtn->setPosition({winSize.width / 2.f, winSize.height - 150.f});
    colorMenu->addChild(colorBtn);

    // ---- Список сохранённых записей ----
    auto listBG = CCScale9Sprite::create("square02_small.png");
    listBG->setContentSize({winSize.width - 40.f, 75.f});
    listBG->setOpacity(120);
    listBG->setPosition({winSize.width / 2.f, 45.f});
    m_mainLayer->addChild(listBG);

    m_recordingsList = ScrollLayer::create({winSize.width - 50.f, 70.f});
    m_recordingsList->setPosition({25.f, 10.f});
    m_mainLayer->addChild(m_recordingsList);

    rebuildRecordingsList();

    return true;
}

CCArray* GhostMenuPopup::collectAllRecordings() {
    auto arr = CCArray::create();
    auto dir = GhostRecorder::ghostSaveDir();
    if (!std::filesystem::exists(dir)) return arr;

    for (auto const& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") continue;
        auto readRes = file::readString(entry.path());
        if (readRes.isErr()) continue;
        auto parsed = matjson::parse(readRes.unwrap());
        if (parsed.isErr()) continue;

        // Складываем сырую пару (levelID, percent, путь) как CCArray со строками,
        // чтобы не тащить лишние типы в Cocos-объекты
        auto rec = GhostRecording::fromJson(parsed.unwrap());
        auto info = CCDictionary::create();
        info->setObject(CCString::create(std::to_string(rec.levelID)), "levelID");
        info->setObject(CCString::create(std::to_string(rec.bestPercent)), "percent");
        info->setObject(CCString::create(entry.path().string()), "path");
        arr->addObject(info);
    }
    return arr;
}

void GhostMenuPopup::rebuildRecordingsList() {
    m_recordingsList->m_contentLayer->removeAllChildren();

    auto recordings = collectAllRecordings();
    float y = 0.f;
    float rowHeight = 26.f;

    for (int i = 0; i < recordings->count(); i++) {
        auto info = static_cast<CCDictionary*>(recordings->objectAtIndex(i));
        auto levelID = static_cast<CCString*>(info->objectForKey("levelID"))->getCString();
        auto percent = static_cast<CCString*>(info->objectForKey("percent"))->getCString();
        auto path = static_cast<CCString*>(info->objectForKey("path"))->getCString();

        auto row = CCLayer::create();
        row->setContentSize({m_recordingsList->getContentSize().width, rowHeight});
        row->setPosition({0.f, y});

        auto label = CCLabelBMFont::create(
            fmt::format("Уровень #{}  —  {}%", levelID, percent).c_str(), "chatFont.fnt"
        );
        label->setAnchorPoint({0.f, 0.5f});
        label->setScale(0.6f);
        label->setPosition({6.f, rowHeight / 2.f});
        row->addChild(label);

        auto delMenu = CCMenu::create();
        delMenu->setPosition({0.f, 0.f});
        row->addChild(delMenu);

        auto delSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
        delSprite->setScale(0.55f);
        auto delBtn = CCMenuItemSpriteExtra::create(
            delSprite, this, menu_selector(GhostMenuPopup::onDeleteRecording)
        );
        delBtn->setPosition({m_recordingsList->getContentSize().width - 14.f, rowHeight / 2.f});
        delBtn->setUserObject(CCString::create(path));
        delMenu->addChild(delBtn);

        m_recordingsList->m_contentLayer->addChild(row);
        y += rowHeight;
    }

    m_recordingsList->m_contentLayer->setContentSize(
        {m_recordingsList->getContentSize().width, std::max(y, m_recordingsList->getContentSize().height)}
    );
    m_recordingsList->moveToTop();
}

void GhostMenuPopup::onToggleEnabled(CCObject*) {
    bool enabled = !m_enableToggle->isToggled();
    Mod::get()->setSavedValue("ghost-enabled-" + std::to_string(m_levelID), enabled);
    // Актуальное состояние (включен/выключен) применится на PlayLayer через
    // GhostPlayback::setEnabled при следующем PlayLayer::update — см. main.cpp
}

void GhostMenuPopup::onOpacityChanged(CCObject*) {
    float value = m_opacitySlider->getValue();
    Mod::get()->setSettingValue("ghost-opacity", static_cast<double>(value));
    m_opacityLabel->setString(fmt::format("{}%", static_cast<int>(value * 100.f)).c_str());
}

void GhostMenuPopup::onPickColor(CCObject* sender) {
    auto current = Mod::get()->getSettingValue<ccColor3B>("ghost-color");

    // Находим текущий цвет в пресете и берём следующий по кругу
    size_t idx = 0;
    for (size_t i = 0; i < GHOST_COLOR_PRESETS.size(); i++) {
        auto& c = GHOST_COLOR_PRESETS[i];
        if (c.r == current.r && c.g == current.g && c.b == current.b) {
            idx = i;
            break;
        }
    }
    auto next = GHOST_COLOR_PRESETS[(idx + 1) % GHOST_COLOR_PRESETS.size()];
    Mod::get()->setSettingValue("ghost-color", next);

    auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
    static_cast<CCNode*>(btn->getNormalImage())->setColor(next);
}

void GhostMenuPopup::onDeleteRecording(CCObject* sender) {
    auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
    auto pathStr = static_cast<CCString*>(btn->getUserObject())->getCString();

    geode::createQuickPopup(
        "Удалить запись?",
        "Вы уверены, что хотите удалить эту запись призрака? Это действие необратимо.",
        "Отмена", "Удалить",
        [this, pathStr](auto, bool confirmed) {
            if (!confirmed) return;
            std::error_code ec;
            std::filesystem::remove(std::filesystem::path(pathStr), ec);
            rebuildRecordingsList();
        }
    );
}
