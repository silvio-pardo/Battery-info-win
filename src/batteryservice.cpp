#include "batteryservice.h"
using namespace Napi;

BatteryService::BatteryService(const Napi::CallbackInfo& info) : ObjectWrap(info){};

BatteryService::BatteryInfo BatteryService::getBatteryInfo(const HDEVINFO& hd_ev,
    SP_DEVICE_INTERFACE_DATA& sp_device) {
    
    BatteryService::BatteryInfo returnValue;
    returnValue.errors = false;
    try {
        DWORD cbRequired = 0;
        SetupDiGetDeviceInterfaceDetail(hd_ev, &sp_device, 0, 0, &cbRequired, 0);

        if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
        {
            PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, cbRequired);

            if (pdidd)
            {
                pdidd->cbSize = sizeof(*pdidd);

                if (SetupDiGetDeviceInterfaceDetail(hd_ev, &sp_device, pdidd, cbRequired, &cbRequired, 0))
                {
                    HANDLE hBattery = CreateFile(pdidd->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

                    if (INVALID_HANDLE_VALUE != hBattery)
                    {
                        BATTERY_QUERY_INFORMATION batteryQueryInfo = { 0 };

                        DWORD dwWait = 0;
                        DWORD dwOut;
                        //retrieve the battery tag for perform other call
                        if (DeviceIoControl(hBattery,
                            IOCTL_BATTERY_QUERY_TAG,
                            &dwWait,
                            sizeof(dwWait),
                            &batteryQueryInfo.BatteryTag,
                            sizeof(batteryQueryInfo.BatteryTag),
                            &dwOut,
                            NULL)
                            && batteryQueryInfo.BatteryTag) {
                            //battery information for retrieve the full charge capacity and other infos
                            BATTERY_INFORMATION batteryInfo{};
                            batteryQueryInfo.InformationLevel = BatteryInformation;
                            if (DeviceIoControl(hBattery,
                                IOCTL_BATTERY_QUERY_INFORMATION,
                                &batteryQueryInfo,
                                sizeof(batteryQueryInfo),
                                &batteryInfo,
                                sizeof(batteryInfo),
                                &dwOut,
                                NULL))
                            {
                                // check is not an ups battery
                                if (batteryInfo.Capabilities & BATTERY_SYSTEM_BATTERY) {
                                    returnValue.designed_capacity = batteryInfo.DesignedCapacity;
                                    returnValue.cycle_count = batteryInfo.CycleCount;
                                    returnValue.full_charged_capacity = batteryInfo.FullChargedCapacity;
                                    
                                    // get the current battery capacity
                                    BATTERY_WAIT_STATUS bws = { 0 };
                                    bws.BatteryTag = batteryQueryInfo.BatteryTag;

                                    BATTERY_STATUS bs{};
                                    if (DeviceIoControl(hBattery,
                                        IOCTL_BATTERY_QUERY_STATUS,
                                        &bws,
                                        sizeof(bws),
                                        &bs,
                                        sizeof(bs),
                                        &dwOut,
                                        NULL)) {
                                        returnValue.capacity = bs.Capacity;
                                        returnValue.voltage = bs.Voltage;
                                        returnValue.power_state = bs.PowerState;
                                        returnValue.rate = bs.Rate;
                                    }

                                    // get the current battery name
                                    wchar_t deviceName[_MAX_PATH];
                                    batteryQueryInfo.InformationLevel = BatteryDeviceName;
                                    if (DeviceIoControl(hBattery,
                                        IOCTL_BATTERY_QUERY_INFORMATION,
                                        &batteryQueryInfo,
                                        sizeof(batteryQueryInfo),
                                        &deviceName,
                                        _MAX_PATH,
                                        &dwOut,
                                        NULL)) {

                                        std::array<char, _MAX_PATH> buffer;
                                        
                                        int length = WideCharToMultiByte(
                                            CP_UTF8, 0,
                                            deviceName, sizeof(deviceName),
                                            buffer.data(), _MAX_PATH,
                                            NULL, NULL);

                                        returnValue.device_name = std::string(buffer.data());
                                    }
                                }
                                else
                                {
                                    returnValue.errors = true;
                                }
                            }
                        }
                        CloseHandle(hBattery);
                    }
                }
                LocalFree(pdidd);
            }
        }
        return returnValue;
    }
    catch (...) {
        std::printf("error");
        returnValue.errors = true;
        return returnValue;
    }
}

Napi::Value BatteryService::getBatteryList(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    //return a vector of battery and catch all ex
    try 
    {
        Napi::Object resultObject = Napi::Object::New(env);
        HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVICE_BATTERY, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        SP_DEVICE_INTERFACE_DATA did = { 0 };
        did.cbSize = sizeof(did);
        int counterIndex = 0;
        while(SetupDiEnumDeviceInterfaces(hdev, 0, &GUID_DEVICE_BATTERY, counterIndex, &did)) {
            BatteryService::BatteryInfo batteryData = this->getBatteryInfo(hdev, did);
            if(!batteryData.errors){
                //temporary object
                Napi::Object resultObjectTemp = Napi::Object::New(env);
                resultObjectTemp.Set("DeviceName",batteryData.device_name);
                resultObjectTemp.Set("DesignedCapacity",batteryData.designed_capacity);
                resultObjectTemp.Set("FullChargedCapacity",batteryData.full_charged_capacity);
                resultObjectTemp.Set("CycleCount",batteryData.cycle_count);
                resultObjectTemp.Set("Voltage",batteryData.voltage);
                resultObjectTemp.Set("Capacity",batteryData.capacity);
                resultObjectTemp.Set("PowerState",batteryData.power_state);
                resultObjectTemp.Set("Rate",batteryData.rate);
                //save into result object
                resultObject.Set(counterIndex, resultObjectTemp);
            }
            counterIndex++;
        }
        return resultObject;
    }
    catch (...) {
        Napi::TypeError::New(env, "System Error!").ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Function BatteryService::GetClass(Napi::Env env) {
  return DefineClass(
      env,
      "BatteryService",
      {
          BatteryService::InstanceMethod("getBatteryList", &BatteryService::getBatteryList),
      });
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  Napi::String name = Napi::String::New(env, "BatteryService");
  exports.Set(name, BatteryService::GetClass(env));
  return exports;
}

NODE_API_MODULE(addon, Init)