#pragma once

#include <string>
#include <unordered_map>
#include <SFML/Graphics.hpp>

struct QuestEntry {
    int questId;
    std::string title;
    std::string description;
    std::string progress;
};

class QuestUI {
public:
    void Init(sf::Font& font);
    void Accept(int questId, const std::string& title, const std::string& description, const std::string& progress);
    void Update(int questId, const std::string& progress);
    void Complete(int questId);
    void ToggleVisible();
    void Draw(sf::RenderWindow& window);

    float Width() const;

private:
    bool _visible = true;
    std::unordered_map<int, QuestEntry> _quests;
    sf::Font* _font = nullptr;
};

