#include "DXUtils.h"
#include <comdef.h>

#include <array>
#include <system_error>
#include <iostream>

static IDXGIFactory1* sharedDXGIFactory() {
    static IDXGIFactory1* factoryPtr = nullptr;
    if(factoryPtr) {
        return factoryPtr;
    }

    auto result = CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factoryPtr));
    if(FAILED(result)) {
        throw std::system_error{
            result, std::system_category(),
            _com_error{ result }.ErrorMessage()
        };
    }
    return factoryPtr;
}

static std::wstring to_wstring(D3D_FEATURE_LEVEL level) {
    switch(level) {
        case D3D_FEATURE_LEVEL_1_0_CORE:
            return L"Direct3D Core";
        case D3D_FEATURE_LEVEL_9_1:
            return L"Direct3D 9.1";
        case D3D_FEATURE_LEVEL_9_2:
            return L"Direct3D 9.2";
        case D3D_FEATURE_LEVEL_9_3:
            return L"Direct3D 9.3";
        case D3D_FEATURE_LEVEL_10_0:
            return L"Direct3D 10.0";
        case D3D_FEATURE_LEVEL_10_1:
            return L"Direct3D 10.1";
        case D3D_FEATURE_LEVEL_11_0:
            return L"Direct3D 11.0";
        case D3D_FEATURE_LEVEL_11_1:
            return L"Direct3D 11.1";
        case D3D_FEATURE_LEVEL_12_0:
            return L"Direct3D 12.0";
        case D3D_FEATURE_LEVEL_12_1:
            return L"Direct3D 12.1";
    }
    return L"Undefined";
}

bool TDW::Utils::operator==(const LUID& lhs, const LUID& rhs) {
    return (lhs.HighPart == rhs.HighPart) && (lhs.LowPart == rhs.LowPart);
}

TDW::Utils::adapter_desc_feature_map TDW::Utils::supportedDXVersions() {
    constexpr std::array featureLevels{
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };
  
    const auto deviceFactory = sharedDXGIFactory();
    adapter_desc_feature_map result;
    IDXGIAdapter* currentAdapter;
    for(UINT i = 0;
        deviceFactory->EnumAdapters(i, &currentAdapter) != DXGI_ERROR_NOT_FOUND;
        ++i) {
        DXGI_ADAPTER_DESC adapterDescription;
        auto adapterDescriptionResult = currentAdapter->GetDesc(&adapterDescription);
        if(FAILED(adapterDescriptionResult)) {
            std::wcerr << "Could not obtain description for video adapter: "
                << _com_error{ adapterDescriptionResult }.ErrorMessage()
                << std::endl;
            continue;
        }

        auto supportedLevel = D3D_FEATURE_LEVEL_1_0_CORE;
        for(decltype(featureLevels)::size_type shift = 0;
            shift < featureLevels.size(); ++shift) {
            const auto createDeviceResult = D3D11CreateDevice(
                currentAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                featureLevels.data() + shift,
                static_cast<UINT>(featureLevels.size() - shift),
                D3D11_SDK_VERSION, nullptr, &supportedLevel, nullptr
            );       

            if(createDeviceResult == E_INVALIDARG) {
                continue;
            }

            if(FAILED(createDeviceResult)) {
                std::wcerr << "Could not find supported DX level for video adapter: "
                    << adapterDescription.Description
                    << "; Error: " << _com_error{ createDeviceResult }.ErrorMessage()
                    << std::endl;
            }

            // API found
            break;
        }

        result[adapterDescription] = supportedLevel;
    }

    return result;

}

#include <sstream>
#include <iomanip>
#include <limits>

std::wstring TDW::Utils::printSupportedDXVersions(const adapter_desc_feature_map& supportedVersions) {
    std::wostringstream stream;
    stream << std::endl << std::endl << std::endl
        << "|==============DirectX API support report==============|"
        << std::endl << std::endl;
    auto insertMemoryString = [](std::wostringstream& wos, std::wstring name, SIZE_T bytes) {
        std::wios formatStorage{ nullptr };
        formatStorage.copyfmt(wos);

        constexpr auto bytesPerKiB = 1024.f;
        constexpr auto bytesPerMiB = bytesPerKiB * 1024.f;
        constexpr auto bytesPerGiB = bytesPerMiB * 1024.f;

        const auto floatBytes = static_cast<float>(bytes);
        wos << name << std::setprecision(1) << std::fixed;
        if(floatBytes > bytesPerGiB / 2.f) {
            wos << floatBytes / bytesPerGiB << " GiB";
        } else if(floatBytes > bytesPerMiB / 2.f) {
            wos << floatBytes / bytesPerMiB << " MiB";
        } else if(floatBytes > bytesPerKiB / 2.f) {
            wos << floatBytes / bytesPerKiB << " KiB";
        } else if(floatBytes > 0) {
            wos << floatBytes << " B";
        } else {
            wos << 0;
        }
        wos << std::endl;
        wos.copyfmt(formatStorage);
    };

    for(const auto& record : supportedVersions) {
        stream << "- Adapter: " << record.first.Description << std::endl;
        stream << "- Vendor PCI ID: " << record.first.VendorId << std::endl;
        stream << "- Device PCI ID: " << record.first.DeviceId << std::endl;
        insertMemoryString(stream, L"- Shared System Memory: ", record.first.SharedSystemMemory);
        insertMemoryString(stream, L"- Dedicated System Memory: ", record.first.DedicatedSystemMemory);
        insertMemoryString(stream, L"- Dedicated Video Memory: ", record.first.DedicatedVideoMemory);
        stream << "- Supported Direct3D Version: " << to_wstring(record.second) << std::endl;
        stream << "--------------------------------------------------------" << std::endl << std::endl;
    }

    std::wstring reportData = stream.str();
    std::wcout << reportData;
    return reportData;
}
