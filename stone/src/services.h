#pragma once

#include <simppl/interface.h>
#include <simppl/map.h>
#include <simppl/struct.h>
#include <simppl/pair.h>
#include <simppl/variant.h>

#include <map>
#include <string>
#include <vector>

namespace one::codehz::stone {
using namespace std;
using namespace simppl::dbus;

namespace structs {
struct AutoCompleteOption {
  using serializer_type = make_serializer<string, string, string, int, int, int>::type;
  string source, title, desc;
  int offset, eat, item;
};
struct PlayerInfo {
  using serializer_type = make_serializer<string, string, string>::type;
  string name, uuid, xuid;
  bool operator!=(PlayerInfo const &rhs) const { return name != rhs.name; }
  bool operator==(PlayerInfo const &rhs) const { return name == rhs.name; }
};
} // namespace structs

INTERFACE(CoreService) {
  Method<oneway> stop;
  Method<in<char>, in<string>, out<map<string, simppl::Variant<string, int, unsigned, double>>>> getPlayerInfo;
  Property<std::vector<structs::PlayerInfo>, ReadWrite | Notifying> players;
  Signal<structs::PlayerInfo> playerAdded;
  Signal<structs::PlayerInfo> playerRemoved;
  Signal<int, string, string> log;

  CoreService()
      : INIT(stop)
      , INIT(getPlayerInfo)
      , INIT(players)
      , INIT(playerAdded)
      , INIT(playerRemoved)
      , INIT(log) {}
};

INTERFACE(CommandService) {
  Method<in<string>, in<string>, out<string>> execute;
  Method<in<string>, in<unsigned>, out<vector<structs::AutoCompleteOption>>> complete;

  CommandService()
      : INIT(execute)
      , INIT(complete) {}
};

INTERFACE(ChatService) {
  Method<in<string>, in<string>> send;
  Signal<string, string> recv;
  Property<bool> intercept;

  ChatService()
      : INIT(send)
      , INIT(recv)
      , INIT(intercept) {}
};

} // namespace one::codehz::stone