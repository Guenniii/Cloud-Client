#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/utility_suite.hpp"

namespace {
class AssistModule final : public RuntimeModule {
    void Tick() override { UtilitySuite::Publish(p_jni?p_jni->GetEnv():nullptr); }
public:
    AssistModule(const char* name,const char* category,std::atomic<bool>& enabled):RuntimeModule(name,category,enabled) {}
    ~AssistModule() override { UtilitySuite::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateAutoArmor() { return std::make_unique<AssistModule>("Auto Armor","Utility",AutoArmor_Enabled); }
std::unique_ptr<ModuleBase> RuntimeModules::CreateRefill() { return std::make_unique<AssistModule>("Refill","Utility",Refill_Enabled); }
std::unique_ptr<ModuleBase> RuntimeModules::CreateHitEffect() { return std::make_unique<AssistModule>("Hit Effect","Combat",HitEffect_Enabled); }
std::unique_ptr<ModuleBase> RuntimeModules::CreatePredictDoubleHand() { return std::make_unique<AssistModule>("Predict Double Hand","Combat",PredictDoubleHand_Enabled); }
std::unique_ptr<ModuleBase> RuntimeModules::CreateShieldBreaker() { return std::make_unique<AssistModule>("Auto Shield Breaker","Combat",ShieldBreaker_Enabled); }
