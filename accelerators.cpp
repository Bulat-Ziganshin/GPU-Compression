// Copyright 2015 Bulat Ziganshin
// Released to public domain

#include <amp.h>
#include <iostream>
#include <conio.h>
using namespace std;
using namespace concurrency;

int main()
{
  for(auto acc: accelerator::get_all())
  {
    char* cpu_access_type[] = {"auto", "none", "read", "read/write", "write"};
    std::wcout << "New accelerator: " << acc.description << std::endl;
    std::wcout << "device_path = " << acc.device_path << std::endl;
    std::wcout << "version = " << (acc.version >> 16) << '.' << (acc.version & 0xFFFF) << std::endl;
    std::wcout << "dedicated_memory = " << acc.dedicated_memory << " KB" << std::endl;
    std::wcout << "supports_cpu_shared_memory = " << ((acc.supports_cpu_shared_memory) ? "true" : "false") << std::endl;
    std::wcout << "default_cpu_access_type = " << cpu_access_type[acc.default_cpu_access_type] << std::endl;
    std::wcout << "doubles = " << ((acc.supports_double_precision) ? "true" : "false") << std::endl;
    std::wcout << "limited_doubles = " << ((acc.supports_limited_double_precision) ? "true" : "false") << std::endl;
    std::wcout << "has_display = " << ((acc.has_display) ? "true" : "false") << std::endl;
    std::wcout << "is_emulated = " << ((acc.is_emulated) ? "true" : "false") << std::endl;
    std::wcout << "is_debug = " << ((acc.is_debug) ? "true" : "false") << std::endl;
    std::cout << std::endl;
  };
  getch();
}
