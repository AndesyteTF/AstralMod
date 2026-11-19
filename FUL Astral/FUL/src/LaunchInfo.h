#pragma once
#include <string>

struct LaunchInfo
{
	bool Silent = false;   
    bool Offline = false;  
    bool Secure = false;   
    bool NoBypass = false; 
    bool UseLL = false;
    bool Debug = false;
    bool Cache = false;

    std::wstring File;
    std::wstring URL;

    int Product = 0;     
};
