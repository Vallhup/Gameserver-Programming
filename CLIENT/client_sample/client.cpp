#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <Windows.h>
#include <chrono>
#include <string>
#include <limits>
#include <WS2tcpip.h>
#include <imm.h>

using namespace std;

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "imm32.lib")

#include "ChatUI.h"
#include "QuestUI.h"
#include "../../SERVER/ServerCore/protocol.h"

sf::TcpSocket s_socket;

constexpr auto SCREEN_WIDTH = 20;
constexpr auto SCREEN_HEIGHT = 20;

constexpr auto TILE_WIDTH = 50;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH ;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_HEIGHT * TILE_WIDTH;

constexpr int halfW = SCREEN_WIDTH / 2;
constexpr int halfH = SCREEN_HEIGHT / 2;

constexpr int MAP_WIDTH = 2000;
constexpr int MAP_HEIGHT = 2000;

bool g_map[MAP_HEIGHT][MAP_WIDTH];

void load_map(const std::string& filename)
{
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "맵 파일 열기 실패: " << filename << "\n";
		std::exit(1);
	}

	std::string line;
	int y = 0;
	while (std::getline(file, line)) {
		for (int x = 0; x < MAP_WIDTH && x < line.length(); ++x) {
			g_map[y][x] = (line[x] == '1');
		}
		++y;
		if (y >= MAP_HEIGHT) break;
	}
	if (y != MAP_HEIGHT) {
		std::cerr << "맵 줄 수 부족: " << y << " / " << MAP_HEIGHT << "\n";
		std::exit(1);
	}
}

int gId = 1;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font g_font;

sf::Text g_hpText;

std::string g_name;

// Quest
static QuestUI g_questUI;

// Inventory
bool g_inventoryVisible = false;
sf::Texture g_itemTexture;
std::unordered_map<char, int> g_inventory; // itemId → count

// attack
std::vector<std::pair<int, int>> g_attackArea;
std::chrono::steady_clock::time_point g_attackEndTime;

// chat
static ChatUI* g_chatUI = nullptr;
static WNDPROC g_prevWndProc = nullptr;
LRESULT CALLBACK ImeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// party state
bool g_partyInviteActive = false;
int g_partyInviteFrom = -1;
int g_partyId = -1;

// client.cpp 맨 위, 기존 g_partyMembers 대신
struct PartyMemberInfo {
	int      id;
	char	 state;
};

static std::vector<PartyMemberInfo> g_partyMembers;
static bool                         g_inParty = false;

sf::RectangleShape g_acceptBtn;
sf::RectangleShape g_declineBtn;
sf::Text		   g_inviteText;

enum ObjectType : char {
	PLAYER,
	MONSTER,
	NPC
};

class OBJECT {
private:
	bool m_showing;
	sf::Sprite m_sprite;

	sf::Text m_name;
	sf::Text m_chat;
	chrono::system_clock::time_point m_mess_end_time;
	char m_questSymbol{ 0 };

public:
	ObjectType type{ ObjectType::MONSTER };
	int id;
	int m_x, m_y;
	std::string name;
	int	m_hp;
	int	m_max_hp;
	int	m_exp;
	int	m_level;
	std::chrono::high_resolution_clock::time_point last_move_time;
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		float scaleX = float(TILE_WIDTH) / float(x2);
		float scaleY = float(TILE_WIDTH) / float(y2);
		m_sprite.setScale(scaleX, scaleY);
		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
	}
	OBJECT() {
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = float(m_x - g_left_x) * float(TILE_WIDTH) + 1.f;
		float ry = float(m_y - g_top_y) * float(TILE_WIDTH) + 1.f;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);

		if (m_questSymbol == 1 || m_questSymbol == 2) {
			sf::Text symbolText;
			symbolText.setFont(g_font);
			symbolText.setCharacterSize(24);
			symbolText.setStyle(sf::Text::Bold);
			symbolText.setFillColor(sf::Color::Yellow);
			symbolText.setString(m_questSymbol == 1 ? "!" : "?");
			symbolText.setPosition(rx + TILE_WIDTH / 2 - 4, ry - 30); // NPC 위에 출력
			g_window->draw(symbolText);
		}

		auto size = m_name.getGlobalBounds();
		if (m_mess_end_time < chrono::system_clock::now()) {
			m_name.setPosition(rx + 32 - size.width / 2, ry - 10);
			g_window->draw(m_name);
		}
		else {
			m_chat.setPosition(rx + 32 - size.width / 2, ry - 10);
			g_window->draw(m_chat);
		}
	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);

		if (id == g_myid) m_name.setFillColor(sf::Color::White);
		else if (id < MAX_USER) m_name.setFillColor(sf::Color::White);
		else m_name.setFillColor(sf::Color(255, 128, 0)); 

		m_name.setStyle(sf::Text::Bold);
	}

	void set_chat(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setCharacterSize(10);
		if (id == g_myid) m_chat.setFillColor(sf::Color::White);
		else if (id < MAX_USER) m_chat.setFillColor(sf::Color::White);
		else m_chat.setFillColor(sf::Color(255, 128, 0));

		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}

	void SetQuestSymbol(char symbol) { m_questSymbol = symbol; }
};

OBJECT avatar;
unordered_map <int, OBJECT> players;

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	if (!g_itemTexture.loadFromFile("item_hp_potion.png")) {
		std::cerr << "아이템 텍스처 로드 실패\n";
		exit(-1);
	}
	if (false == g_font.loadFromFile("nanumgothic.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	avatar.move(4, 4);

	g_hpText.setFont(g_font);
	g_hpText.setCharacterSize(24);
	g_hpText.setFillColor(sf::Color::Blue);
	g_hpText.setPosition(10.f, 10.f);

	// party logic
	g_acceptBtn.setSize({ 100, 40 });
	g_acceptBtn.setPosition(WINDOW_WIDTH / 2 - 110, WINDOW_HEIGHT / 2 + 10);
	g_acceptBtn.setFillColor(sf::Color(0, 200, 0, 200));

	g_declineBtn.setSize({ 100, 40 });
	g_declineBtn.setPosition(WINDOW_WIDTH / 2 + 10, WINDOW_HEIGHT / 2 + 10);
	g_declineBtn.setFillColor(sf::Color(200, 0, 0, 200));

	g_inviteText.setFont(g_font);
	g_inviteText.setCharacterSize(20);
	g_inviteText.setFillColor(sf::Color::White);

	// Quest
	g_questUI.Init(g_font);
}

void client_finish()
{
	players.clear();
	delete board;
	delete pieces;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		avatar.id = g_myid;
		avatar.move(packet->x, packet->y);
		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.set_name(std::to_string(gId).c_str());
		avatar.show();

		printf("LOGIN: name = %s id = %d (MAX_USER = %d)\n", avatar.name.c_str(), avatar.id, MAX_USER);
	}
	break;
	case SC_LOGIN_FAIL: {
		std::cout << "ID[" << gId << "]는 DB에 없는 ID입니다." << std::endl;
		exit(-1);
		break;
	}
	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;
		char symbol = static_cast<char>(my_packet->questSymbol);
		int monsterType = my_packet->monsterType;

		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
			players[id].type = static_cast<ObjectType>(my_packet->objType);
			avatar.show();
		}
		else if (id < MAX_USER) {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };
			players[id].id = id;
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(to_string(my_packet->id).c_str());
			players[id].type = static_cast<ObjectType>(my_packet->objType);
			players[id].SetQuestSymbol(symbol);
			players[id].show();
		}
		else {
			if (my_packet->objType == NPC) {
				players[id] = OBJECT{ *pieces, 192, 0, 64, 64 };
			}

			else {
				if (monsterType == 1) {
					players[id] = OBJECT{ *pieces, 64, 0, 64, 64 };
				}

				else if (monsterType == 2) {
					players[id] = OBJECT{ *pieces, 128, 0, 64, 64 };
				}

				else if (monsterType == 3) {
					players[id] = OBJECT{ *pieces, 256, 0, 64, 64 };
				}

				else if (monsterType == 4) {
					players[id] = OBJECT{ *pieces, 320, 0, 64, 64 };
				}

				else {
					break;
				}
			}

			players[id].id = id;
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].type = static_cast<ObjectType>(my_packet->objType);
			if (players[id].type == NPC) {
				players[id].SetQuestSymbol(symbol);
			}
			players[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
		}
		else {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			players.erase(other_id);
		}
		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* p = reinterpret_cast<SC_CHAT_PACKET*>(ptr);

		int other_id = p->id;
		int targetId = p->targetId;

		// targetId == -1이면 전투 메시지
		// 그 외에는 채팅

		// 채팅
		if (targetId != -1) {
			std::string msg(p->message, strnlen(p->message, sizeof(p->message)));
			if (nullptr != g_chatUI) {
				g_chatUI->addMessage(p->id, msg);
			}
		}

		// 전투메시지
		else {
			std::string msg(p->message, strnlen(p->message, sizeof(p->message)));
			if (nullptr != g_chatUI) {
				g_chatUI->addMessage(-1, msg);
			}
		}
		
		break;
	}
	case SC_ATTACK_NOTIFY:
	{
		auto pkt = reinterpret_cast<SC_ATTACK_NOTIFY_PACKET*>(ptr);
		g_attackArea.clear();
		for (int i = 0; i < 4; ++i)
			g_attackArea.emplace_back(pkt->x[i], pkt->y[i]);

		g_attackEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
		break;
	}
	case SC_PARTY_REQUEST: {
		auto pkt = reinterpret_cast<SC_PARTY_REQUEST_PACKET*>(ptr);
		g_partyInviteFrom = pkt->fromId;
		g_partyInviteActive = true;
		// 텍스트 갱신
		g_inviteText.setString("Party invite from " + std::to_string(pkt->fromId));
		// 중앙 정렬
		auto bb = g_inviteText.getLocalBounds();
		g_inviteText.setPosition(
			(WINDOW_WIDTH - bb.width) / 2,
			(WINDOW_HEIGHT - bb.height) / 2 - 20);
		break;
	}
	case SC_PARTY_RESULT: {
		auto pkt = reinterpret_cast<SC_PARTY_RESULT_PACKET*>(ptr);
		if (pkt->acceptFlag) {
			std::cout << "Player " << pkt->targetId << " accepted your invite\n";
		}
		else {
			std::cout << "Player " << pkt->targetId << " declined your invite\n";
		}
		break;
	}
	case SC_PARTY_UPDATE: {
		// 업데이트 패킷 해석
		auto upd = reinterpret_cast<SC_PARTY_UPDATE_PACKET*>(ptr);
		int memberCount = upd->memberCount;
		int partyId = upd->partyId;

		// payload 오프셋 계산
		size_t totalSize = upd->size;
		size_t infosBytes = sizeof(MemberInfo) * memberCount;
		size_t headerSize = totalSize - infosBytes;

		auto infos = reinterpret_cast<MemberInfo*>(ptr + headerSize);

		// 전역에 partyId 저장(옵션)
		g_partyId = partyId;

		g_partyMembers.clear();
		for (int i = 0; i < memberCount; ++i) {
			auto& info = infos[i];
			PartyMemberState st = static_cast<PartyMemberState>(info.state);
			g_partyMembers.push_back({ info.id, st });
		}
		g_inParty = true;
		break;
	}
	case SC_STAT_CHANGE: {
		// 업데이트 패킷 해석
		auto upd = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);

		avatar.m_exp = upd->exp;
		avatar.m_hp = upd->hp;
		avatar.m_max_hp = upd->max_hp;
		avatar.m_level = upd->level;
		break;
	}
	case SC_ADD_ITEM: {
		// 업데이트 패킷 해석
		auto upd = reinterpret_cast<SC_ADD_ITEM_PACKET*>(ptr);
		g_inventory[upd->itemId] += upd->count;
		break;
	}
	case SC_USE_ITEM_OK: {
		// 업데이트 패킷 해석
		auto upd = reinterpret_cast<SC_USE_ITEM_OK_PACKET*>(ptr);
		g_inventory[upd->itemId] -= 1;
		if (g_inventory[upd->itemId] <= 0) {
			g_inventory.erase(upd->itemId);
		}
		break;
	}
	case SC_USE_ITEM_FAIL: {
		break;
	}
	case SC_QUEST_ACCEPT: {
		auto pkt = reinterpret_cast<SC_QUEST_ACCEPT_PACKET*>(ptr);

		std::string safeTitle(pkt->title, strnlen(pkt->title, sizeof(pkt->title)));
		std::string safeDescription(pkt->description, strnlen(pkt->description, sizeof(pkt->description)));
		std::string safeProgress(pkt->progressText, strnlen(pkt->progressText, sizeof(pkt->progressText)));

		g_questUI.Accept(pkt->questId, safeTitle, safeDescription, safeProgress);
		break;
	}
	case SC_QUEST_UPDATE: {
		auto pkt = reinterpret_cast<SC_QUEST_UPDATE_PACKET*>(ptr);
		std::string safeProgress(pkt->progressText, strnlen(pkt->progressText, sizeof(pkt->progressText)));
		g_questUI.Update(pkt->questId, safeProgress);
		break;
	}
	case SC_QUEST_COMPLETE: {
		auto pkt = reinterpret_cast<SC_QUEST_COMPLETE_PACKET*>(ptr);
		g_questUI.Complete(pkt->questId);
		break;
	}
	case SC_QUEST_SYMBOL_UPDATE: {
		auto pkt = reinterpret_cast<SC_QUEST_SYMBOL_UPDATE_PACKET*>(ptr);
		int id = pkt->npcId;

		if (players.contains(id)) {
			players[id].SetQuestSymbol(pkt->questSymbol);
		}

		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (in_packet_size == 0) {
			if (io_byte < 1) return;

			in_packet_size = static_cast<unsigned char>(ptr[0]);

			if (in_packet_size < 2 || in_packet_size > BUF_SIZE) {
				in_packet_size = 0;
				saved_packet_size = 0;
				return;
			}
		}
		else {
			if (in_packet_size > BUF_SIZE) {
				in_packet_size = 0;
				saved_packet_size = 0;
				return;
			}
		}
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void send_logout_on_exit();

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv 에러!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		wcout << L"Disconnected\n";
		send_logout_on_exit();
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (tile_x >= 0 && tile_y >= 0 && tile_x < MAP_WIDTH && tile_y < MAP_HEIGHT) {
				if (g_map[tile_y][tile_x]) {
					white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					white_tile.a_draw();
				}
				else {
					black_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
					black_tile.a_draw();
				}
			}
		}
	avatar.draw();
	for (auto& pl : players) pl.second.draw();
	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
	text.setString(buf);
	text.setPosition(10.f, 10.f);
	g_window->draw(text);
	
	sf::Text statText;
	statText.setFont(g_font);
	statText.setCharacterSize(20);
	statText.setFillColor(sf::Color::Cyan);
	statText.setPosition(10.f, 45.f);

	std::string statStr = "HP: " + std::to_string(avatar.m_hp) +
		" LV: " + std::to_string(avatar.m_level) +
		" EXP: " + std::to_string(avatar.m_exp);
	statText.setString(statStr);
	g_window->draw(statText);

	// 초대 대화상자
	if (g_partyInviteActive) {
		// 반투명 배경
		sf::RectangleShape overlay({ (float)WINDOW_WIDTH,(float)WINDOW_HEIGHT });
		overlay.setFillColor(sf::Color(0, 0, 0, 150));
		g_window->draw(overlay);

		// 텍스트
		g_window->draw(g_inviteText);

		// 버튼
		g_window->draw(g_acceptBtn);
		g_window->draw(g_declineBtn);

		// 버튼 라벨
		sf::Text t1("O: Accept", g_font, 18);
		t1.setPosition(g_acceptBtn.getPosition() + sf::Vector2f(10, 8));
		g_window->draw(t1);
		 
		sf::Text t2("X: Decline", g_font, 18);
		t2.setPosition(g_declineBtn.getPosition() + sf::Vector2f(10, 8));
		g_window->draw(t2);
	}

	if (g_inParty && !g_partyMembers.empty()) {
		// 1) 배경
		sf::RectangleShape panel;
		panel.setSize({ 250.f, 80.f });
		panel.setPosition(WINDOW_WIDTH - 260.f, 10.f);
		panel.setFillColor(sf::Color(0, 0, 0, 180));
		g_window->draw(panel);

		// 2) 제목
		std::string titleStr = "Party " + std::to_string(g_partyId) + " (P: Leave)";
		sf::Text title(titleStr, g_font, 16);
		title.setFillColor(sf::Color::White);
		title.setPosition(WINDOW_WIDTH - 255.f, 15.f);
		g_window->draw(title);

		// 3) 멤버별로 사각형만 그리기
		for (size_t i = 0; i < g_partyMembers.size(); ++i) {
			auto& m = g_partyMembers[i];

			sf::Color boxColor;
			switch (m.state) {
			case ONLINE_ALIVE: boxColor = sf::Color::Green;   break;
			case ONLINE_DEAD:  boxColor = sf::Color::Red;     break;
			case OFFLINE:      boxColor = sf::Color(128, 128, 128); // 회색
				break;
			default:           boxColor = sf::Color::White;   break;
			}

			// 상태 박스
			sf::RectangleShape box({ 15.f, 15.f });
			box.setPosition(WINDOW_WIDTH - 250.f + i * 20.f, 64.f);
			box.setFillColor(boxColor);
			g_window->draw(box);
		}
	}

	// 공격 범위 시각화
	if (std::chrono::steady_clock::now() < g_attackEndTime) {
		for (const auto& [x, y] : g_attackArea) {
			if ((x >= g_left_x) && (x < g_left_x + SCREEN_WIDTH) &&
				(y >= g_top_y) && (y < g_top_y + SCREEN_HEIGHT)) {

				float rx = float(x - g_left_x) * float(TILE_WIDTH) + 5.f;
				float ry = float(y - g_top_y) * float(TILE_WIDTH) + 5.f;

				sf::Text circle;
				circle.setFont(g_font);
				circle.setString("O");
				circle.setCharacterSize(20);
				circle.setFillColor(sf::Color::Red);
				circle.setStyle(sf::Text::Bold);
				circle.setPosition(rx, ry);
				g_window->draw(circle);
			}
		}
	}

	// 인벤토리
	if (g_inventoryVisible) {
		// 배경 패널
		sf::RectangleShape bg;
		bg.setPosition(20 + g_questUI.Width(), 100);
		bg.setSize({ 220, 100 });
		bg.setFillColor(sf::Color(0, 0, 0, 180));
		g_window->draw(bg);

		// 슬롯 네모 + 아이템 아이콘
		int idx = 0;
		for (auto& [itemId, count] : g_inventory) {
			sf::RectangleShape slot;
			slot.setSize({ 40, 40 });
			slot.setPosition(24 + g_questUI.Width() + idx * 50, 104);
			slot.setFillColor(sf::Color(100, 100, 100));
			g_window->draw(slot);

			sf::Sprite itemIcon;
			itemIcon.setTexture(g_itemTexture);
			itemIcon.setPosition(24 + g_questUI.Width() + idx * 50 + 4, 104 + 4);
			float slotSize = 35.f;
			sf::Vector2u texSize = g_itemTexture.getSize();
			itemIcon.setScale(slotSize / texSize.x, slotSize / texSize.y);
			g_window->draw(itemIcon);

			sf::Text countText;
			countText.setFont(g_font);
			countText.setCharacterSize(12);
			countText.setFillColor(sf::Color::Black);
			countText.setString(std::to_string(count));
			countText.setPosition(24 + g_questUI.Width() + idx * 50 + 30, 104 + 25);
			g_window->draw(countText);

			idx++;
		}
	}
	
	// Quest
	g_questUI.Draw(*g_window);
}

void send_packet(void *packet)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(packet);
	size_t sent = 0;
	s_socket.send(packet, p[0], sent);
}

std::string gServerIp = "127.0.0.1";

bool isValidIp(const std::string& s) {
	sockaddr_in sa{};
	return inet_pton(AF_INET, s.c_str(), &sa.sin_addr) == 1;
}

void send_logout_on_exit()
{
	CS_LOGOUT_PACKET logout{ sizeof(logout), CS_LOGOUT };
	send_packet(&logout);
}

bool g_questAccept{ false };

int main()
{
	wcout.imbue(locale("korean"));

	std:cout << (int)SC_USE_ITEM_FAIL << std::endl;

	std::string line;
	while (true) {
		std::cout << "서버 IP 주소를 입력하세요 (기본 127.0.0.1): ";
		if (not std::getline(std::cin, line)) {
			std::cin.clear();
			std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
			continue;
		}

		if (line.empty()) {
			gServerIp = "127.0.0.1";
			break;
		}

		if (isValidIp(line)) {
			gServerIp = line;
			break;
		}

		std::cout << "올바른 IPv4 형식(예: 127.0.0.1)이 아닙니다. 다시 시도하세요.\n";
	}

	std::atexit(send_logout_on_exit);

	sf::Socket::Status status = s_socket.connect(gServerIp, PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	std::cout << "접속할 ID를 입력하세요: ";
	int id{ 1 };
	std::cin >> id;
	gId = id;

	load_map("mapdata.txt");

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;
	p.id = gId;

	g_name = "P" + to_string(GetCurrentProcessId());
	
	strcpy_s(p.name, g_name.c_str());
	send_packet(&p);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	// Win32 IME 컨텍스트 붙이기
	HWND hwnd = g_window->getSystemHandle();
	HIMC hImc = ImmGetContext(hwnd);
	ImmAssociateContext(hwnd, hImc);
	ImmReleaseContext(hwnd, hImc);

	g_prevWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)ImeWndProc);

	// ChatUI 초기화
	ChatUI chatUI(g_font, 18, ChatUI::MAX_HISTORY);
	g_chatUI = &chatUI;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed) {
				CS_LOGOUT_PACKET logout{ sizeof(logout), CS_LOGOUT };
				send_packet(&logout);
					
				window.close();
			}

			if (event.type == sf::Event::TextEntered) {
				g_chatUI->handleTextEntered(event.text.unicode);
			}

			else if (event.type == sf::Event::KeyPressed) {
				g_chatUI->handleKeyPressed(event.key);

				int direction = -1;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					direction = 2;
					break;
				case sf::Keyboard::Right:
					direction = 3;
					break;
				case sf::Keyboard::Up:
					direction = 0;
					break;
				case sf::Keyboard::Down:
					direction = 1;
					break;
				case sf::Keyboard::Escape: {
					CS_LOGOUT_PACKET logout{ sizeof(logout), CS_LOGOUT };
					send_packet(&logout);

					window.close();
					break;
				}
				case sf::Keyboard::A: {
					CS_ATTACK_PACKET p;
					p.size = sizeof(p);
					p.type = CS_ATTACK;
					p.attack_time = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now().time_since_epoch()).count());

					send_packet(&p);

					// 공격 범위 계산 (4방향)
					int x = avatar.m_x;
					int y = avatar.m_y;
					g_attackArea.clear();
					g_attackArea.emplace_back(x - 1, y);
					g_attackArea.emplace_back(x + 1, y);
					g_attackArea.emplace_back(x, y - 1);
					g_attackArea.emplace_back(x, y + 1);
					g_attackEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(300); // 0.3초 동안만 표시

					break;
				}
				case sf::Keyboard::I: {
					g_inventoryVisible = not g_inventoryVisible;
					break;
				}
				case sf::Keyboard::P: {
					if (g_inParty) {
						CS_PARTY_LEAVE_PACKET pkt;
						pkt.size = sizeof(pkt);
						pkt.type = CS_PARTY_LEAVE;
						send_packet(&pkt);
						// 클라이언트에서 즉시 UI 사라지게
						g_partyMembers.clear();
						g_inParty = false;
					}
					break;
				}
				//case sf::Keyboard::Q: {
				//	CS_QUEST_ACCEPT_PACKET packet{};
				//	packet.size = sizeof(packet);
				//	packet.type = CS_QUEST_ACCEPT;
				//	packet.questId = 1;  // 예시: 퀘스트 ID 1
				//	send_packet(&packet);
				//	break;
				//}
				case sf::Keyboard::Num1: {
					CS_USE_ITEM_PACKET packet;
					packet.size = sizeof(packet);
					packet.type = CS_USE_ITEM;
					packet.itemId = ItemId::HpPotion;
					send_packet(&packet);
					break;
				}
				case sf::Keyboard::Tab: {
					g_questUI.ToggleVisible();
					break;
				}
				}

				if (-1 != direction) {
					avatar.last_move_time = std::chrono::high_resolution_clock::now();

					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
					p.move_time = static_cast<unsigned>(duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());

					send_packet(&p);
				}

			}

			if (event.mouseButton.button == sf::Mouse::Left) {
				// 화면 좌표 → 맵 타일 좌표
				int col = event.mouseButton.x / TILE_WIDTH + g_left_x;
				int row = event.mouseButton.y / TILE_WIDTH + g_top_y;

				// players 맵에서 클릭한 아바타 찾기
				for (auto& [id, obj] : players) {
					if (id == g_myid) continue;
					if (obj.m_x == col && obj.m_y == row) {
						if (obj.type == ObjectType::NPC) {
							CS_QUEST_ACCEPT_PACKET packet{};
							packet.size = sizeof(packet);
							packet.type = CS_QUEST_ACCEPT;
							packet.questId = 1;  // 예시: 퀘스트 ID 1
							send_packet(&packet);
							
							if (g_questAccept) {
								CS_TALK_TO_NPC_PACKET pkt;
								pkt.size = sizeof(pkt);
								pkt.type = CS_TALK_TO_NPC;
								pkt.npcId = id;

								send_packet(&pkt);
							}

							else {
								g_questAccept = true;
							}

							break;
						}

						else if ((obj.type == ObjectType::PLAYER) and (not g_partyInviteActive)) {
							// 초대 보내기
							CS_PARTY_REQUEST_PACKET req;
							req.size = sizeof(req);
							req.type = CS_PARTY_REQUEST;
							req.targetId = id;
							cout << req.targetId << endl;
							send_packet(&req);

							break;
						}
					}
				}
			}

			// ───────── 초대 다이얼로그 클릭 ─────────
			if (g_partyInviteActive && event.mouseButton.button == sf::Mouse::Left)
			{
				sf::Vector2f mp(event.mouseButton.x, event.mouseButton.y);
				// 수락 버튼
				if (g_acceptBtn.getGlobalBounds().contains(mp)) {
					CS_PARTY_RESPONSE_PACKET res;
					res.size = sizeof(res);
					res.type = CS_PARTY_RESPONSE;
					res.acceptFlag = 1;
					send_packet(&res);
					g_partyInviteActive = false;
				}
				// 거절 버튼
				else if (g_declineBtn.getGlobalBounds().contains(mp)) {
					CS_PARTY_RESPONSE_PACKET res;
					res.size = sizeof(res);
					res.type = CS_PARTY_RESPONSE;
					res.acceptFlag = 0;
					send_packet(&res);
					g_partyInviteActive = false;
				}
			}
		}

		std::vector<char> outPkt;
		if (g_chatUI->popOutgoing(outPkt)) {
			std::size_t sent = 0;
			s_socket.send(outPkt.data(), outPkt.size(), sent);
		}

		window.clear();
		
		g_left_x = avatar.m_x - halfW;
		g_top_y = avatar.m_y - halfH;

		g_left_x = std::clamp(g_left_x, 0, 2000 - SCREEN_WIDTH);
		g_top_y = std::clamp(g_top_y, 0, 2000 - SCREEN_HEIGHT);

		client_main();
		chatUI.draw(window);
		window.display();
	}
	client_finish();

	return 0;
}

LRESULT CALLBACK ImeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_IME_COMPOSITION) {
		HIMC hImc = ImmGetContext(hwnd);

		// 1) 조합 중인 문자열(GCS_COMPSTR)
		if (lParam & GCS_COMPSTR) {
			LONG size = ImmGetCompositionStringW(hImc, GCS_COMPSTR, nullptr, 0);
			if (size > 0) {
				std::wstring ws; 
				ws.resize(size / sizeof(wchar_t));
				ImmGetCompositionStringW(hImc, GCS_COMPSTR, &reinterpret_cast<wchar_t*>(&ws[0])[0], size);
				g_chatUI->setComposition(ChatUI::wstr_to_u32(ws));
			}
		}
		// 2) 확정된 문자열(GCS_RESULTSTR)
		if (lParam & GCS_RESULTSTR) {
			LONG size = ImmGetCompositionStringW(hImc, GCS_RESULTSTR, nullptr, 0);
			if (size > 0) {
				std::wstring ws; ws.resize(size / sizeof(wchar_t));
				ImmGetCompositionStringW(hImc, GCS_RESULTSTR, &ws[0], size);
				g_chatUI->commitComposition(ChatUI::wstr_to_u32(ws));
			}
		}

		ImmReleaseContext(hwnd, hImc);
		// 메시지 처리 완료
		return 0;
	}
	// 기본 처리
	return CallWindowProc(g_prevWndProc, hwnd, msg, wParam, lParam);
}