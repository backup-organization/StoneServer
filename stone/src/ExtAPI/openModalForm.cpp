#include "common.h"
#include <interface/modal_form.h>
#include <interface/player_list.h>
#include <map>
#include <minecraft/ModalForm.h>

#include "../patched/ScriptInterface.h"

static_assert(sizeof(int) == sizeof(void *), "Only works in 32bit");

namespace ExtAPI {
using namespace interface;

struct ReqTicket {
  uint32_t id;
  AutoReleasePersistent<Promise::Resolver> resolver;
  ReqTicket(uint32_t id, AutoReleasePersistent<Promise::Resolver> &&resolver)
      : id(id)
      , resolver(std::move(resolver)) {}
  ReqTicket(ReqTicket &&ticket)
      : id(ticket.id)
      , resolver(std::move(ticket.resolver)) {}
  uint32_t get_id() { return id; }
  void resolve(std::string const &data) {
    auto &core = Locator<ScriptApi::V8CoreInterface>();
    auto iso   = patched::V8Isolate[*core];
    HandleScope scope{ iso };
    Isolate::Scope isos{ iso };
    auto ctx = patched::V8Context[*core].Get(iso);
    Context::Scope ctx_scope{ ctx };
    resolver.Get(iso)->Resolve(ctx, ToJS(data));
  }
  void reject(std::string const &data) {
    auto &core = Locator<ScriptApi::V8CoreInterface>();
    auto iso   = patched::V8Isolate[*core];
    HandleScope scope{ iso };
    Isolate::Scope isos{ iso };
    auto ctx = patched::V8Context[*core].Get(iso);
    Context::Scope ctx_scope{ ctx };
    resolver.Get(iso)->Reject(ctx, ToJS(data));
  }
  ~ReqTicket() {
    if (resolver) reject("Canceled");
  }
};

static std::map<Player *, ReqTicket> reqCache;

static void openModalFormCallback(FunctionCallbackInfo<Value> const &info) {
  auto iso = info.GetIsolate();
  Isolate::Scope isos{ iso };
  auto &core = Locator<ScriptApi::V8CoreInterface>();
  auto ctx   = patched::V8Context[*core].Get(iso);
  if (info.Length() != 2) {
    iso->ThrowException(Exception::TypeError(ToJS(strformat("openModalForm requires 2 arguments(current: %d)", info.Length()))));
    return;
  }
  if (!info[0]->IsObject() || !info[1]->IsString()) {
    iso->ThrowException(Exception::TypeError(ToJS("openModalForm requires (player, string)")));
    return;
  }
  auto actor = fromJS<Actor *>(iso, info[0]);
  auto data  = fromJS<std::string>(iso, info[1]);
  if (!actor || *(void **)actor != BUILD_HELPER(GetAddress, void, 0x8, "_ZTV12ServerPlayer").Address()) {
    iso->ThrowException(Exception::TypeError(ToJS("openModalForm requires (player, string)")));
    return;
  }
  auto player   = (Player *)actor;
  auto resolver = Promise::Resolver::New(ctx);
  AutoReleasePersistent pres{ iso, resolver };
  auto id = (uint32_t)rand();
  reqCache.emplace(player, ReqTicket{ id, std::move(pres) });
  info.GetReturnValue().Set(resolver->GetPromise());
  Locator<ModalForm>()->send(player, id, data);
}

static Register reg{ "registerComponent", "openModalForm", &openModalFormCallback, +[](Isolate *iso) {
                      Locator<PlayerList>()->onPlayerRemoved >> [](Player &player) { reqCache.erase(&player); };
                      Locator<ModalForm>()->recv >> [](unsigned id, Player *player, std::string const &data) {
                        auto req_it = reqCache.find(player);
                        if (req_it == reqCache.end()) return;
                        if (req_it->second.get_id() == id) {
                          req_it->second.resolve(data);
                          reqCache.erase(req_it);
                        }
                      };
                    } };

} // namespace ExtAPI