#include "common.h"
#include <interface/chat.h>
#include <minecraft/MinecraftCommands.h>

#include "../custom_command.h"

namespace ExtAPI {
using namespace interface;

static void invokeCommandCallback(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  if (info.Length() == 1) {
    if (!info[0]->IsString()) {
      Log::error("Scripting", "invokeCommand requires (string)");
      return;
    }
    if (!current_orig) {
      Log::error("Scripting", "invokeCommand need be invoked inside custom command execution context");
      return;
    }
    auto command = fromJS<std::string>(iso, info[0]);
    auto result  = patched::withCommandOutput([&] {
      auto commandOrigin = current_orig->clone();
      Locator<MinecraftCommands>()->requestCommandExecution(std::move(commandOrigin), command, 4, true);
    });
    info.GetReturnValue().Set(toJS(iso, result));
  } else if (info.Length() == 2) {
    if (!info[0]->IsObject() || !info[1]->IsString()) {
      Log::error("Scripting", "invokeCommand requires (object, string)");
      return;
    }
    auto actor   = fromJS<Actor *>(iso, info[0]);
    auto command = fromJS<std::string>(iso, info[1]);
    if (!actor || *(void **)actor != BUILD_HELPER(GetAddress, void, 0x8, "_ZTV12ServerPlayer").Address()) {
      Log::error("Scripting", "invokeCommand requires (player, string)");
      return;
    }
    auto origin = std::make_unique<PlayerCommandOrigin>((Player &)*actor);
    auto result = patched::withCommandOutput([&] { Locator<MinecraftCommands>()->requestCommandExecution(std::move(origin), command, 4, true); });
    info.GetReturnValue().Set(toJS(iso, result));
  } else {
    Log::error("Scripting", "invokeCommand requires 1 or 2 arguments(current: %d)", info.Length());
    return;
  }
}

static Register reg{ "registerComponent", "invokeCommand", &invokeCommandCallback };

} // namespace ExtAPI