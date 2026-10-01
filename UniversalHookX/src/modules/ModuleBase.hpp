#pragma once

#include <string>



class ModuleBase {
public:
    virtual ~ModuleBase() = default;
    virtual void Update( ) = 0;
    virtual void RenderOverlay( ) = 0;
    virtual void RenderHud( ) = 0;

    virtual void RenderMenu( ) = 0;

    virtual std::string GetName( ) = 0;
    virtual std::string GetCategory( ) = 0;
    virtual int GetKey( ) = 0;

    virtual bool IsEnabled( ) = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void Toggle( ) = 0;
private:
    std::string m_name;
    std::string m_category;
};
