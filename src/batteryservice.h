#pragma once
#define INITGUID
#include <windows.h>
#include <batclass.h>
#pragma comment (lib, "Setupapi.lib")
#include <setupapi.h>
#include <iostream>
#include <array>
#include <vector>
#include <napi.h>

class BatteryService : public Napi::ObjectWrap<BatteryService>
{
public:
    BatteryService(const Napi::CallbackInfo& info);
    
    struct BatteryInfo {
        //error while retrieve infos of single battery
        bool errors;
        std::string device_name;
        ULONG designed_capacity;
        ULONG full_charged_capacity;
        ULONG cycle_count;
        ULONG voltage;
        ULONG capacity;
        ULONG power_state;
        LONG rate;
    };

    //std::vector<BatteryInfo>
    Napi::Value getBatteryList(const Napi::CallbackInfo&);

    static Napi::Function GetClass(Napi::Env);

protected:
    BatteryInfo getBatteryInfo(const HDEVINFO& hd_ev, SP_DEVICE_INTERFACE_DATA& sp_device);
};
