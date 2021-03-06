#include "common.h"
#include <interface/policy.h>

#include <minecraft/Actor.h>
#include <minecraft/BlockSource.h>
#include <minecraft/Player.h>

#include "../custom_command.h"
#include "../patched/ScriptInterface.h"

namespace ExtAPI {
using namespace interface;
using namespace patched;

template <typename F> auto checkFn(FunctionCallbackInfo<Value> const &info, F f) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  if (info.Length() != 1 || !info[0]->IsFunction()) {
    iso->ThrowException(Exception::TypeError(ToJS(strformat("checks requires (function)"))));
    return;
  }
  f(iso);
}

template <typename F> auto handleFn(Isolate *iso, bool &result, F f) {
  if (!Locator<ScriptApi::V8CoreInterface>()) return;
  HandleScope scope{ iso };
  auto &core = Locator<ScriptApi::V8CoreInterface>();
  auto ctx   = V8Context[*core].Get(iso);
  Context::Scope ctx_scope{ ctx };
  TryCatch ex{ iso };

  auto ret = f();
  if (ex.HasCaught()) {
    Log::error("Scripting", "Uncaught exception from checks: %s", ex.Message()->Get() >> V8Str >> CStr);
    return;
  }
  if (ret.isNothing()) return;
  auto real_ret = ret.fromJust();
  if (real_ret->IsTrue()) result = true;
  if (real_ret->IsFalse()) result = false;
}

static Register regs[] = {
  { "registerComponent", "checkAbility",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkAbility >> [iso, origin = Persistent<Value>{ iso, info.This() },
                                            callback = Persistent<Function>{ iso, info[0] }](Player *player, std::string const &name, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("ability"));
            obj->Set(ToJS("ability"), ToJS(name));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkDestroy",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkDestroy >> [iso, origin = Persistent<Value>{ iso, info.This() },
                                            callback = Persistent<Function>{ iso, info[0] }](Player *player, BlockPos const &pos, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("destroy"));
            auto &region = player->getRegion();
            obj->Set(ToJS("block"), ToJS(region.getBlock(pos)));
            obj->Set(ToJS("blockpos"), ToJS(pos));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkBuild",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkBuild >> [iso, origin = Persistent<Value>{ iso, info.This() },
                                          callback = Persistent<Function>{ iso, info[0] }](Player *player, BlockPos const &pos, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("build"));
            obj->Set(ToJS("blockpos"), ToJS(pos));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkUse",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkUse >> [iso, origin = Persistent<Value>{ iso, info.This() },
                                        callback = Persistent<Function>{ iso, info[0] }](Player *player, ItemStack &item, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("use"));
            obj->Set(ToJS("item"), ToJS(&item));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkUseBlock",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkUseBlock >> [iso, origin = Persistent<Value>{ iso, info.This() }, callback = Persistent<Function>{ iso, info[0] }](
                                                Player *player, Block &block, BlockPos const &pos, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("use_block"));
            obj->Set(ToJS("block"), ToJS(&block));
            obj->Set(ToJS("blockpos"), ToJS(pos));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkUseOn",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkUseOn >> [iso, origin = Persistent<Value>{ iso, info.This() }, callback = Persistent<Function>{ iso, info[0] }](
                                             Player *player, ItemStack &item, BlockPos const &pos, Vec3 const &vec, Block &block, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("use_on"));
            obj->Set(ToJS("block"), ToJS(&block));
            obj->Set(ToJS("item"), ToJS(&item));
            obj->Set(ToJS("blockpos"), ToJS(pos));
            obj->Set(ToJS("position"), ToJS(vec));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkInteract",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkInteract >> [iso, origin = Persistent<Value>{ iso, info.This() }, callback = Persistent<Function>{ iso, info[0] }](
                                                Player *player, Actor &target, Vec3 const &pos, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("interact"));
            obj->Set(ToJS("target"), ToJS(&target));
            obj->Set(ToJS("blockpos"), ToJS(pos));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
  { "registerComponent", "checkAttack",
    +[](FunctionCallbackInfo<Value> const &info) {
      checkFn(info, [&](Isolate *iso) {
        Locator<Policy>()->checkAttack >> [iso, origin = Persistent<Value>{ iso, info.This() },
                                           callback = Persistent<Function>{ iso, info[0] }](Player *player, Actor &target, bool &result) {
          handleFn(iso, result, [&]() {
            auto obj = Object::New(iso);
            obj->Set(ToJS("type"), ToJS("attack"));
            obj->Set(ToJS("target"), ToJS(&target));
            return callback.Get(iso)->Call(origin.Get(iso), iso, player, obj, result);
          });
        };
      });
    } },
};

} // namespace ExtAPI