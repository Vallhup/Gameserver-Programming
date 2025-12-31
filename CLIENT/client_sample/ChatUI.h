#pragma once

#include <SFML/Graphics.hpp>
#include <deque>
#include <string>

class ChatUI {
public:
    ChatUI(sf::Font& font, unsigned int charSize = 16, std::size_t maxLines = 10);

    // 사용자가 키 입력할 때 호출
    void handleTextEntered(sf::Uint32 unicode);
    void handleKeyPressed(const sf::Event::KeyEvent& key);

    // 네트워크에서 SC_CHAT_PACKET을 받았을 때 호출
    void addMessage(int senderId, const std::string& text);

    // 매 프레임 그리기
    void draw(sf::RenderWindow& window);

    // 보낼 패킷을 꺼내는 함수 (Enter 입력 시)
    bool popOutgoing(std::vector<char>& outPacket);

    void processChar(char32_t c)
    {
        // 1) 현재 버퍼에 있는 한글/영문 개수 세기
        std::size_t hangulCount = 0, engCount = 0;
        for (char32_t ch : _inputBuffer32) {
            if (isHangul(ch))        ++hangulCount;
            else if (isEnglish(ch))  ++engCount;
            else {
                ++hangulCount;
                ++engCount;
            }
        }

        // 2) 들어올 문자의 종류에 따라 최대치 검사
        if (isHangul(c)) {
            if (hangulCount >= MAX_HAN_INPUT)
                return;  // 한글 한도 초과
        }

        else if (isEnglish(c)) {
            if (engCount >= MAX_ENG_INPUT)
                return;  // 영문 한도 초과
        }

        else {
            if ((hangulCount >= MAX_HAN_INPUT) or (engCount >= MAX_ENG_INPUT))
                return;
        }

        _inputBuffer32.push_back(c);
    }

    // IME 조합 문자열 업데이트
    void setComposition(const std::u32string& comp)
    {
        if (_inInput) {
            _composition32 = comp;
        }
    }

    // IME 확정 문자열 커밋
    void commitComposition(const std::u32string& result32)
    {
        if (!_inInput) return;
        for (char32_t c : result32) {
            processChar(c);
        }
        _composition32.clear();
    }

    static constexpr size_t MAX_HISTORY = 8;
    static constexpr size_t MAX_HAN_INPUT = 39;
    static constexpr size_t MAX_ENG_INPUT = 80;

    bool                        _inInput;

private:
    sf::RectangleShape _background;
    sf::Text           _historyText;
    sf::Text           _inputText;

    std::u32string _inputBuffer32;
    std::u32string _composition32;

    std::deque<std::u32string>  _history;
    std::size_t                 _maxLines;
    unsigned int                _charSize;
   

    sf::Clock _caretTimer;
    float _blinkPeriod = 2.0f;
    float _caretWidth = 1.5f;
    float _caretHeightRatio = 1.0f;

private:
    // 내부 큐에 저장된 CS_CHAT_PACKET 직렬화 바이트열
    std::vector<std::vector<char>> _outgoingPackets;

    // 패킷 직렬화 헬퍼 (가령 PacketFactory::Serialize)
    std::vector<char> makeChatPacket(const std::string& msg);

    static bool isHangul(char32_t c)
    {
        return (c >= 0xAC00 && c <= 0xD7A3)  // 완성된 한글
            or (c >= 0x1100 && c <= 0x11FF)  // Hangul Jamo
            or (c >= 0x3130 && c <= 0x318F)  // Hangul Compatibility Jamo
            or (c >= 0xA960 && c <= 0xA97F)  // Hangul Jamo Extended-A
            or (c >= 0xD7B0 && c <= 0xD7FF); // Hangul Jamo Extended-B
    }

    static bool isEnglish(char32_t c) 
    {
        return ((c >= U'A' && c <= U'Z') or (c >= U'a' && c <= U'z'));
    }


public:
    // UTF-32 → UTF-8 구현
    static std::string utf32_to_utf8(const std::u32string& src) {
        std::string out;
        out.reserve(src.size() * 3); // 한글 쪽이 많아도 대략 3바이트 예상

        for (char32_t c : src) {
            if (c < 0x80) {
                // 1바이트 (ASCII)
                out.push_back(static_cast<char>(c));
            }
            else if (c < 0x800) {
                // 2바이트
                out.push_back(static_cast<char>(0xC0 | (c >> 6)));
                out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
            else if (c < 0x10000) {
                // 3바이트 (한글 음절 포함)
                out.push_back(static_cast<char>(0xE0 | (c >> 12)));
                out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
            else {
                // 4바이트 (보충 평면)
                out.push_back(static_cast<char>(0xF0 | (c >> 18)));
                out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }

        return out;
    }

    // UTF-8 → UTF-32 변환 헬퍼
    static std::u32string utf8_to_utf32(const std::string & utf8) {
        std::u32string out;
        size_t i = 0, len = utf8.size();
        while (i < len) {
            uint8_t c = static_cast<uint8_t>(utf8[i]);
            char32_t cp = 0;
            size_t n = 0;
            if ((c & 0x80) == 0) { cp = c;            n = 1; }
             else if ((c & 0xE0) == 0xC0) { cp = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F); n = 2; }
             else if ((c & 0xF0) == 0xE0) { cp = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F); n = 3; }
             else if ((c & 0xF8) == 0xF0) { cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F); n = 4; }
             else { cp = 0xFFFD;      n = 1; } // replacement
             out.push_back(cp);
            i += n;
        }

         return out;
    }

    // u32string → sf::String (Uint32 기반) 변환 헬퍼
    static sf::String utf32_to_sf(const std::u32string & src) {
        sf::String out;
        for (char32_t c : src)
            out += static_cast<sf::Uint32>(c);

        return out;
    }

    // helper: UTF-16(WCHAR) → UTF-32 변환
    static std::u32string wstr_to_u32(const std::wstring& ws) {
        std::u32string out;
        out.reserve(ws.size());
        for (wchar_t wc : ws)
            out.push_back(static_cast<char32_t>(wc));
        return out;
    }

    // UTF-32 코드포인트를 UTF-16(wchar_t)으로 변환
    static std::wstring utf32_to_utf16(const std::u32string& src) {
        std::wstring out;
        out.reserve(src.size());
        for (char32_t c : src) {
            if (c <= 0xFFFF) {
                // BMP 영역
                out.push_back(static_cast<wchar_t>(c));
            }
            else {
                // supplementary plane: 서러게이트 페어 생성
                char32_t cp = c - 0x10000;
                wchar_t high = static_cast<wchar_t>((cp >> 10) + 0xD800);
                wchar_t low = static_cast<wchar_t>((cp & 0x3FF) + 0xDC00);
                out.push_back(high);
                out.push_back(low);
            }
        }
        return out;
    }
};
