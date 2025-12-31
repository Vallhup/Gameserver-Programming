#pragma once

void InitializeNetwork();
void GetPointCloud(int* size, float** points);

extern std::string gServerIp;

extern int global_delay;
extern std::atomic_int active_clients;