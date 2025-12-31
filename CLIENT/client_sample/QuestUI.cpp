#include "QuestUI.h"
#include "ChatUI.h"

void QuestUI::Init(sf::Font& font) {
    _font = &font;
}

void QuestUI::Accept(int questId, const std::string& title, const std::string& description, const std::string& progress) {
    _quests[questId] = QuestEntry{ questId, title, description, progress };
}

void QuestUI::Update(int questId, const std::string& progress) {
    if (_quests.contains(questId)) {
        _quests[questId].progress = progress;
    }
}

void QuestUI::Complete(int questId) {
    _quests.erase(questId);
}

void QuestUI::ToggleVisible() {
    _visible = !_visible;
}

void QuestUI::Draw(sf::RenderWindow& window) {
    if (!_visible || _quests.empty() || !_font) return;

    sf::RectangleShape bg;
    bg.setSize({ 330.f, static_cast<float>(_quests.size() * 60 + 10) }); // 60: 두 줄 공간 확보
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    bg.setPosition(10, 100);
    window.draw(bg);

    int y = 110;
    for (const auto& [id, q] : _quests) {
        // 첫 줄: [title]
        sf::Text title("[" + q.title + "]", *_font, 16);
        title.setFillColor(sf::Color::White);
        title.setPosition(15, static_cast<float>(y));
        window.draw(title);

        // 둘째 줄: description - progress
        sf::Text desc(q.description + " - " + q.progress, *_font, 14);
        desc.setFillColor(sf::Color(200, 200, 200)); // 회색 톤
        desc.setPosition(20, static_cast<float>(y + 20));
        window.draw(desc);

        y += 60; // 줄간격
    }
}

float QuestUI::Width() const
{
    return 330.f;
}
