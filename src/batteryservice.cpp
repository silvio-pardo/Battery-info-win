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

                                        std::array<char, _MAX_PATH> buffer { NULL };
                                        
                                        int length = WideCharToMultiByte(
                                            CP_UTF8, 0,
                                            deviceName, sizeof(deviceName),
                                            buffer.data(), _MAX_PATH,
                                            NULL, NULL);

                                        if(!buffer.empty())
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

BatteryService::SystemInfo BatteryService::getSystemData() {
    BatteryService::SystemInfo returnValue;
    try {
        HRESULT hres;
        // declare the com smart pointer 
        _COM_SMARTPTR_TYPEDEF(IWbemLocator, __uuidof(IWbemLocator));
        _COM_SMARTPTR_TYPEDEF(IWbemServices, __uuidof(IWbemServices));
        _COM_SMARTPTR_TYPEDEF(IWbemClassObject, __uuidof(IWbemClassObject));
        _COM_SMARTPTR_TYPEDEF(IEnumWbemClassObject, __uuidof(IEnumWbemClassObject));
        // Initialize COM
        hres = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (FAILED(hres))
            throw std::exception("Unable to start the com");
        // Initialize COM security...
        // if is open continue;
        hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
        if (hres != RPC_E_TOO_LATE && FAILED(hres))
        {
            CoUninitialize();
            throw std::exception("Unable to set the layer security");
        }
        // Instance specific CLSID
        IWbemLocatorPtr spLocator;
        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spLocator));
        if (FAILED(hres))
        {
            CoUninitialize();
            throw std::exception("Unable to create CLSID object");
        }
        // Connect to WbemService..
        IWbemServicesPtr spServices;
        hres = spLocator->ConnectServer(_bstr_t(L"root\\cimv2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &spServices);
        if (FAILED(hres))
        {
            // Destroy the pointer
            if (bool(spLocator)) spLocator.Release();
            CoUninitialize();
            throw std::exception("Unable to connect to wbem service");
        }
        // Set Auth info for the specific connnect proxy wbem service
        hres = CoSetProxyBlanket(spServices, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL,
            RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE);
        if (FAILED(hres))
        {
            // Destroy the pointer
            if (bool(spLocator)) spLocator.Release();
            if (bool(spServices)) spServices.Release();
            CoUninitialize();
            throw std::exception("Unable to set auth info for the proxy");
        }
        // run query via WMI to obtain Win32_Computer­System object 
        IEnumWbemClassObjectPtr spEnum;
        hres = spServices->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"select * from Win32_ComputerSystem"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &spEnum);
        if (FAILED(hres))
        {
            // Destroy the pointer
            if (bool(spLocator)) spLocator.Release();
            if (bool(spServices)) spServices.Release();
            if (bool(spEnum)) spEnum.Release();
            CoUninitialize();
            throw std::exception("Unable to run the query win32_computersystem");
        }

        IWbemClassObjectPtr spObject;
        ULONG cActual;
        while (spEnum->Next(WBEM_INFINITE, 1, &spObject, &cActual)
            == WBEM_S_NO_ERROR) {
            _variant_t var;
            spObject->Get(L"Name", 0, &var, nullptr, nullptr);
            returnValue.name = var.bstrVal != NULL ? _bstr_t(var.bstrVal) : "";
            spObject->Get(L"Manufacturer", 0, &var, nullptr, nullptr);
            returnValue.manufacturer = var.bstrVal != NULL ? _bstr_t(var.bstrVal) : "";
            spObject->Get(L"Model", 0, &var, nullptr, nullptr);
            returnValue.model = var.bstrVal != NULL ? _bstr_t(var.bstrVal) : "";
            spObject->Get(L"ChassisSKUNumber", 0, &var, nullptr, nullptr);
            returnValue.chassis_skku_number = var.bstrVal != NULL ? _bstr_t(var.bstrVal) : "";
            var.Clear();
        }
        // release the old data pointer.. 
        if (bool(spEnum)) spEnum.Release();
        if (bool(spObject)) spObject.Release();
        // Run query via WMI to obtain Win32_BIOS object
        IEnumWbemClassObjectPtr spEnumBios;
        hres = spServices->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"select * from Win32_BIOS"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &spEnumBios);
        if (FAILED(hres))
        {
            // Destroy the pointer
            if (bool(spLocator)) spLocator.Release();
            if (bool(spServices)) spServices.Release();
            if (bool(spEnumBios)) spEnumBios.Release();
            CoUninitialize();
            throw std::exception("Unable to run the query win32_bios");
        }

        IWbemClassObjectPtr spObjectBios;
        ULONG cActualBios;
        while (spEnumBios->Next(WBEM_INFINITE, 1, &spObjectBios, &cActualBios)
            == WBEM_S_NO_ERROR) {
            _variant_t var;
            spObjectBios->Get(L"SerialNumber", 0, &var, nullptr, nullptr);
            returnValue.serial_number = var.bstrVal != NULL ? _bstr_t(var.bstrVal) : "";
            var.Clear();
        }

        //flush all..
        if (bool(spLocator)) spLocator.Release();
        if (bool(spServices)) spServices.Release();
        if (bool(spEnumBios)) spEnumBios.Release();
        if (bool(spObjectBios)) spObjectBios.Release();
        CoUninitialize();
        
    }
    catch (std::exception ex) {
        //managed exception
        returnValue.errors = true;
    }
    catch (...) {
        //unmanaged
        returnValue.errors = true;
    }
    return returnValue;
}

Napi::Value BatteryService::getSystemInfo(const Napi::CallbackInfo& info){
    Napi::Env env = info.Env();
    try 
    {
        Napi::Object resultObject = Napi::Object::New(env);
        BatteryService::SystemInfo resData = this->getSystemData();
        if(resData.errors)
            throw std::exception("errore");
        
        resultObject.Set("Model", resData.model);
        resultObject.Set("Name", resData.name);
        resultObject.Set("Manufacturer", resData.manufacturer);
        resultObject.Set("ChassisSkku", resData.chassis_skku_number);
        resultObject.Set("SerialNumber", resData.serial_number);

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
          BatteryService::InstanceMethod("getSystemInfo", &BatteryService::getSystemInfo)
      });
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  Napi::String name = Napi::String::New(env, "BatteryService");
  exports.Set(name, BatteryService::GetClass(env));
  return exports;
}

NODE_API_MODULE(addon, Init)