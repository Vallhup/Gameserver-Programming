#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <charconv>
#include <sstream>
#include <iostream>
#include <array>
#include <string_view>
#include <algorithm>

constexpr int MAP_WIDTH = 2000;
constexpr int MAP_HEIGHT = 2000;

inline std::vector<std::vector<int>> load_tilemap(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "파일 열기 실패: " << filename << '\n';
        std::exit(EXIT_FAILURE);
    }

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::string_view view{ json };
    auto data_pos = view.find("\"data\": [");
    if (data_pos == std::string_view::npos) {
        std::cerr << "data 배열을 찾을 수 없습니다.\n";
        std::exit(EXIT_FAILURE);
    }

    data_pos += 8; // "data":[ 의 길이
    auto data_end = view.find("]", data_pos);
    if (data_end == std::string_view::npos) {
        std::cerr << "data 배열 종료를 찾을 수 없습니다.\n";
        std::exit(EXIT_FAILURE);
    }

    std::string_view data_view = view.substr(data_pos, data_end - data_pos);

    std::vector<std::vector<int>> map(MAP_HEIGHT, std::vector<int>(MAP_WIDTH, 0));

    int x = 0, y = 0;
    size_t i = 0;

    while (i < data_view.size() && y < MAP_HEIGHT) {
        // 공백, 쉼표 무시
        if (!std::isdigit(data_view[i])) {
            ++i;
            continue;
        }

        int val = 0;
        auto [ptr, ec] = std::from_chars(&data_view[i], &data_view[data_view.size()], val);
        if (ec != std::errc{}) {
            std::cerr << "숫자 파싱 오류\n";
            std::exit(EXIT_FAILURE);
        }

        map[y][x++] = val;

        if (x == MAP_WIDTH) {
            x = 0;
            ++y;
        }

        i = ptr - data_view.data();
    }

    if (y < MAP_HEIGHT) {
        std::cerr << "맵 데이터가 부족합니다. 행: " << y << '\n';
        std::exit(EXIT_FAILURE);
    }

    return map;
}