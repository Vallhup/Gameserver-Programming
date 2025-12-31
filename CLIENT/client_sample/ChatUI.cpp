#define NOMINMAX

#include <iostream>
#include <Windows.h>
#include <algorithm>

#include "ChatUI.h"
#include "../../SERVER/ServerCore/protocol.h"

extern sf::RenderWindow* g_window;

ChatUI::ChatUI(sf::Font& font, unsigned int charSize, std::size_t maxLines)
    : _charSize(charSize), _maxLines(maxLines), _inInput(false)
{
    // 반투명 배경
    _background.setSize({ 800.f, 200.f });
    _background.setFillColor(sf::Color(0, 0, 0, 150));
    _background.setPosition(10.f, 1000.f - 210.f);

    // 대화 히스토리 텍스트
    _historyText.setFont(font);
    _historyText.setCharacterSize(_charSize);
    _historyText.setFillColor(sf::Color::White);
    _historyText.setPosition(20.f, 1000.f - 200.f);

    // 입력란 텍스트
    _inputText.setFont(font);
    _inputText.setCharacterSize(_charSize);
    _inputText.setFillColor(sf::Color::Cyan);
    _inputText.setPosition(20.f, 1000.f - 30.f);
}

void ChatUI::handleTextEntered(sf::Uint32 unicode)
{
    if (not _inInput or unicode < 0x20) {
        return;
    }

    char32_t c = static_cast<char32_t>(unicode);

    // 영문·숫자·기호만 걸러 넣고 싶으면
    if(c < 128) {
        processChar(c);
    }
}

void ChatUI::handleKeyPressed(const sf::Event::KeyEvent& key)
{
    if (key.code == sf::Keyboard::Enter) {
        if (!_inInput) {
            _inInput = true;
        }
        else {
            // 만약 조합 중인 문자열이 있으면 우선 확정
            if (!_composition32.empty()) {
                commitComposition(_composition32);
            }
            // 그 뒤에야 메시지 전송
            if (!_inputBuffer32.empty()) {
                std::string utf8msg = utf32_to_utf8(_inputBuffer32);
                _outgoingPackets.push_back(makeChatPacket(utf8msg));
                _inputBuffer32.clear();
            }
            _inInput = false;
        }
    }
    // ChatUI.cpp 에 onBackspace 또는 handleKeyPressed 백스페이스 처리 시
    else if (key.code == sf::Keyboard::BackSpace and _inInput) {
        // 1) 조합 중 문자열이 남아 있으면
        if (!_composition32.empty()) {
            _composition32.clear();
            return;
        }
         
        // 2) 조합 중이 없으면 확정된 버퍼에서만 pop
        else if (!_inputBuffer32.empty()) {
            _inputBuffer32.pop_back();
        }
    }
}

void ChatUI::addMessage(int senderId, const std::string& text)
{
    std::string all8;

    if (senderId == -1) {
        all8 = text;
    }

    else {
        all8 = "[" + std::to_string(senderId) + "] " + text;
    }

    _history.push_back(utf8_to_utf32(all8));
    if (_history.size() > _maxLines) {
        _history.pop_front();
    }
}

void ChatUI::draw(sf::RenderWindow& window)
{
    // 1) 배경
    window.draw(_background);

    // 2) 히스토리: 뒤쪽 kHistoryLimit개만 출력
    {
        std::u32string combined32;
        std::size_t total = _history.size();
        std::size_t start = (total > _maxLines) ? (total - _maxLines) : 0;

        for (std::size_t i = start; i < total; ++i) {
            combined32 += _history[i] + U'\n';
        }

        _historyText.setString(utf32_to_sf(combined32));
        window.draw(_historyText);
    }

    // 3) 입력란 (커서)
    if (_inInput) {
        // 1. 입력 문자열 출력
        std::u32string combined32 = _inputBuffer32 + _composition32;
        sf::String display = utf32_to_sf(combined32);

        _inputText.setString(display);
        window.draw(_inputText);
        
        // 2. 커서 위치 계산
        unsigned int caretIndex = static_cast<unsigned int>(combined32.size());
        sf::Vector2f pos = _inputText.findCharacterPos(caretIndex);

        // 3. 페이드 α 계산: 0.0 ~ 1.0 사이
        float t = _caretTimer.getElapsedTime().asSeconds();
        float phase = std::fmod(t, _blinkPeriod) / _blinkPeriod;      // 0~1
        float alpha = std::abs(std::cos(phase * 2 * 3.14159f));      // 부드럽게 깜빡임

        // 4. 캐럿 그리기
        sf::RectangleShape caret;
        float ch = _charSize * _caretHeightRatio;
        caret.setSize({ _caretWidth, ch });
        caret.setPosition(pos.x, pos.y + (ch * 0.1f));
        caret.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(alpha * 255.f)));
        window.draw(caret);
    }
}

bool ChatUI::popOutgoing(std::vector<char>& outPacket)
{
    if (_outgoingPackets.empty()) {
        return false;
    }

    outPacket = std::move(_outgoingPackets.front());
    _outgoingPackets.erase(_outgoingPackets.begin());

    return true;
}

std::vector<char> ChatUI::makeChatPacket(const std::string& msg)
{
    CS_CHAT_PACKET pkt;
    pkt.size = sizeof(pkt);
    pkt.type = CS_CHAT;

    std::memset(pkt.message, 0, sizeof(pkt.message));
    std::memcpy(pkt.message, msg.c_str(), std::min(msg.size(), sizeof(pkt.message) - 1));

    std::vector<char> out(sizeof(pkt));
    std::memcpy(out.data(), &pkt, sizeof(pkt));

    return out;
}
