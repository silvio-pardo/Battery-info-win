#pragma once
#define INITGUID
#include <windows.h>
#include <batclass.h>
#pragma comment (lib, "Setupapi.lib")
#include <setupapi.h>
#include <iostream>
#include <array>
#include <vector>

#define _WIN32_DCOM
#include <ole2.h>
#include <oleauto.h>
#include <comdef.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

#include <napi.h>

class BatteryService : public Napi::ObjectWrap<BatteryService>
{
public:
    BatteryService(const Napi::CallbackInfo& info);
    struct BatteryInfo {
        //error while retrieve infos of single battery
        bool errors = false;
        std::string device_name;
        ULONG designed_capacity;
        ULONG full_charged_capacity;
        ULONG cycle_count;
        ULONG voltage;
        ULONG capacity;
        ULONG power_state;
        LONG rate;
    };

    struct SystemInfo {
		bool errors = false;
		std::string model;
		std::string name;
		std::string manufacturer;
	};

    Napi::Value getBatteryList(const Napi::CallbackInfo&);
    Napi::Value getSystemInfo(const Napi::CallbackInfo&);

    static Napi::Function GetClass(Napi::Env);

protected:
    BatteryInfo getBatteryInfo(const HDEVINFO& hd_ev, SP_DEVICE_INTERFACE_DATA& sp_device);
    SystemInfo getSystemData();
};
