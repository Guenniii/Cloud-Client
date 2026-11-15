#pragma once
#include "../../dependencies/jni/jni.h"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
namespace GameRuntime {
struct Details {std::string loader,version="unknown",nameSpace="mojang";bool fabric=false;};
inline std::mutex mutex;
inline jobject gameLoader=nullptr;
inline Details details;
inline bool Check(JNIEnv* e){if(!e->ExceptionCheck())return true;e->ExceptionClear();return false;}
inline jclass Load(JNIEnv* e,jobject loader,jmethodID method,const char* name){
 if(!loader||!method)return nullptr;jstring text=e->NewStringUTF(name);if(!Check(e)||!text)return nullptr;
 auto cls=static_cast<jclass>(e->CallObjectMethod(loader,method,text));e->DeleteLocalRef(text);return Check(e)?cls:nullptr;
}
inline std::string String(JNIEnv* e,jstring value){
 if(!value)return {};const char* chars=e->GetStringUTFChars(value,nullptr);if(!Check(e)||!chars)return {};
 std::string text(chars);e->ReleaseStringUTFChars(value,chars);return text;
}
inline bool Initialize(JNIEnv* e){
 if(!e||e->ExceptionCheck())return false;std::lock_guard lock(mutex);if(gameLoader)return true;
 if(e->PushLocalFrame(128)!=JNI_OK)return false;
 struct Frame{JNIEnv* e;~Frame(){e->PopLocalFrame(nullptr);}} frame{e};
 auto thread=e->FindClass("java/lang/Thread"),map=e->FindClass("java/util/Map"),set=e->FindClass("java/util/Set"),loader=e->FindClass("java/lang/ClassLoader"),type=e->FindClass("java/lang/Class");
 if(!Check(e)||!thread||!map||!set||!loader||!type)return false;
 auto all=e->GetStaticMethodID(thread,"getAllStackTraces","()Ljava/util/Map;");
 auto keys=e->GetMethodID(map,"keySet","()Ljava/util/Set;");auto array=e->GetMethodID(set,"toArray","()[Ljava/lang/Object;");
 auto context=e->GetMethodID(thread,"getContextClassLoader","()Ljava/lang/ClassLoader;");
 auto threadName=e->GetMethodID(thread,"getName","()Ljava/lang/String;");
 auto load=e->GetMethodID(loader,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
 auto own=e->GetMethodID(type,"getClassLoader","()Ljava/lang/ClassLoader;");
 auto name=e->GetMethodID(type,"getName","()Ljava/lang/String;");
 if(!Check(e)||!all||!keys||!array||!context||!load||!own||!name)return false;
 auto traces=e->CallStaticObjectMethod(thread,all);if(!Check(e)||!traces)return false;
 auto keyset=e->CallObjectMethod(traces,keys);if(!Check(e)||!keyset)return false;
 auto threads=static_cast<jobjectArray>(e->CallObjectMethod(keyset,array));if(!Check(e)||!threads)return false;
 for(int pass=0;pass<3&&!gameLoader;++pass) for(jsize i=0;i<e->GetArrayLength(threads);++i){
  if(e->PushLocalFrame(32)!=JNI_OK)break;
  auto t=e->GetObjectArrayElement(threads,i);auto candidate=t?e->CallObjectMethod(t,context):nullptr;
  if(Check(e)&&candidate){
   auto candidateType=e->GetObjectClass(candidate);
   const auto candidateName=String(e,static_cast<jstring>(e->CallObjectMethod(candidateType,name)));
   const auto currentName=String(e,static_cast<jstring>(e->CallObjectMethod(t,threadName)));
   const bool knot=candidateName.find(".knot.")!=std::string::npos;
   const int priority=knot?0:(currentName=="Render thread"||currentName=="Client thread")?1:2;
   if(priority!=pass){e->PopLocalFrame(nullptr);continue;}
   auto mc=Load(e,candidate,load,"net.minecraft.client.Minecraft");
   auto actual=mc?e->CallObjectMethod(mc,own):nullptr;
   if(Check(e)&&actual){
    auto player=Load(e,actual,load,"net.minecraft.client.player.LocalPlayer");
    auto items=Load(e,actual,load,"net.minecraft.world.item.Items");
    if(player&&items){
     gameLoader=e->NewGlobalRef(actual);
     auto loaderType=e->GetObjectClass(actual);details.loader=String(e,static_cast<jstring>(e->CallObjectMethod(loaderType,name)));
     auto fabric=Load(e,actual,load,"net.fabricmc.loader.api.FabricLoader");details.fabric=fabric!=nullptr;
     auto shared=Load(e,actual,load,"net.minecraft.SharedConstants");
     if(shared){auto version=e->GetStaticMethodID(shared,"getCurrentVersion","()Lnet/minecraft/WorldVersion;");
      if(Check(e)&&version){auto object=e->CallStaticObjectMethod(shared,version);if(Check(e)&&object){auto versionClass=e->GetObjectClass(object);auto id=e->GetMethodID(versionClass,"id","()Ljava/lang/String;");if(Check(e)&&id)details.version=String(e,static_cast<jstring>(e->CallObjectMethod(object,id)));Check(e);}}
     }
     if(fabric){auto instance=e->GetStaticMethodID(fabric,"getInstance","()Lnet/fabricmc/loader/api/FabricLoader;");
      if(Check(e)&&instance){auto object=e->CallStaticObjectMethod(fabric,instance);if(Check(e)&&object){auto get=e->GetMethodID(fabric,"getMappingResolver","()Lnet/fabricmc/loader/api/MappingResolver;");if(Check(e)&&get){auto resolver=e->CallObjectMethod(object,get);if(Check(e)&&resolver){auto resolverClass=e->GetObjectClass(resolver);auto ns=e->GetMethodID(resolverClass,"getCurrentRuntimeNamespace","()Ljava/lang/String;");if(Check(e)&&ns)details.nameSpace=String(e,static_cast<jstring>(e->CallObjectMethod(resolver,ns)));Check(e);}}}}
     }
    }
   }
  }
  e->PopLocalFrame(nullptr);if(gameLoader)break;
 }
 if(gameLoader)std::printf("[Runtime] %s Minecraft %s; namespace=%s; loader=%s\n",details.fabric?"Fabric":"Vanilla",details.version.c_str(),details.nameSpace.c_str(),details.loader.c_str());
 return gameLoader!=nullptr;
}
inline jobject Loader(JNIEnv* e){if(!Initialize(e))return nullptr;std::lock_guard lock(mutex);return e->NewLocalRef(gameLoader);}
inline jclass FindClass(JNIEnv* e,const char* name){
 if(!e||!name||e->ExceptionCheck())return nullptr;
 if(std::string_view(name).starts_with("net/minecraft/")||std::string_view(name).starts_with("com/mojang/")){
  auto loader=Loader(e);if(loader){auto cls=e->FindClass("java/lang/ClassLoader");auto load=e->GetMethodID(cls,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
   std::string dotted(name);std::replace(dotted.begin(),dotted.end(),'/','.');auto found=Load(e,loader,load,dotted.c_str());e->DeleteLocalRef(cls);e->DeleteLocalRef(loader);return found;}
 }
 auto cls=e->FindClass(name);return Check(e)?cls:nullptr;
}
inline Details Info(){std::lock_guard lock(mutex);return details;}
inline void Shutdown(JNIEnv* e){std::lock_guard lock(mutex);if(e&&gameLoader)e->DeleteGlobalRef(gameLoader);gameLoader=nullptr;details=Details{};}
}
