#pragma once
#include "../RuntimeModule.hpp"
#include <memory>
class CwCrystal;

// CW owns only its crystal worker; other features have independent modules.
class CW final : public RuntimeModule {
public:
    CW();
    ~CW() override;
private:
    void Tick() override;
    std::unique_ptr<CwCrystal> g_crystal;
};
