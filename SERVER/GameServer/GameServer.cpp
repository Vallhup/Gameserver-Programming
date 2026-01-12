#include "pch.h"
#include "Service.h"

int main()
{
	setlocale(LC_ALL, "korean");

	Logger::Init();
	Logger::SetLevel(LogLevel::Error);

	IocpCorePtr iocpCore = std::make_shared<IocpCore>();
	ServicePtr service = Service::Create(iocpCore, MAX_USER);

	service->Start("2021182017_GameServer_DB", "mapdata.txt");

	std::cout << "종료하려면 Enter 키를 누르세요...\n";
	std::cin.get();

	service->CloseService();

	Logger::Shutdown();
}