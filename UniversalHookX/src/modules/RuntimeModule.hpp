#pragma once
#include "ModuleBase.hpp"
#include <atomic>

// Binds a runtime module directly to the existing setting used by menu/config.
// Update is called even when disabled so the falling edge can be processed.
class RuntimeModule : public ModuleBase {
public:
    RuntimeModule(const char* name, const char* category, std::atomic<bool>& enabled)
        : name_(name), category_(category), enabled_(enabled) {}
    void Update() final;
    void RenderOverlay() override {}
    void RenderHud() override {}
    void RenderMenu() override {}
    std::string GetName() override { return name_; }
    std::string GetCategory() override { return category_; }
    int GetKey() override { return 0; }
    bool IsEnabled() override { return enabled_; }
    void SetEnabled(bool value) override { enabled_ = value; }
    void Toggle() override { enabled_ = !enabled_; }
protected:
    virtual void Tick() = 0;
private:
    const char* name_;
    const char* category_;
    std::atomic<bool>& enabled_;
    bool wasEnabled_ = false;
};
