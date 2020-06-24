#include <eosio.info/eosio.info.hpp>

namespace eosio {

ACTION info::addkeytype(const name key, std::string definition, const bool user)
{
  require_auth(_self);

  infovertypes_table table(_self, _self.value);
  table.emplace(_self, [&](auto& t) {
    t.key = key;
    t.definition = definition;
    t.user = user;
  });
}

ACTION info::adduserver(const name user, const name verification_type)
{
  require_auth(_self);

  infouservers_table table(_self, _self.value);
  auto c_key = composite_key(user.value, verification_type.value);
  auto idx = table.get_index<"key"_n>();
  auto itr = table.find(c_key);

  if (itr == table.end()) {
    table.emplace(user, [&](auto& t) {
      t.id = table.available_primary_key();
      t.key = c_key;
      t.user = user;
      t.verification_type = verification_type;
    });
  }
}

ACTION info::setuserkey(const name user, const name verification_type, const std::string memo)
{
  require_auth(user);

  check(memo.length() <= 1024, "memo exceeds permitted length of 1024 chars");

  infouserkeys_table table(_self, _self.value);
  auto c_key = composite_key(user.value, verification_type.value);
  auto idx = table.get_index<"key"_n>();
  auto itr = table.find(c_key);

  if (itr == table.end()) {
    table.emplace(user, [&](auto& t) {
      t.id = table.available_primary_key();
      t.key = c_key;
      t.user = user;
      t.verification_type = verification_type;
      t.memo = memo;
    });
  } else {
    table.modify(*itr, user, [&]( auto& t) {
      t.memo = memo;
    });
  }
}

ACTION info::deluserkey(const name user, const name verification_type)
{
  require_auth(user);

  infouserkeys_table table(_self, _self.value);
  auto c_key = composite_key(user.value, verification_type.value);
  auto idx = table.get_index<"key"_n>();
  auto itr = table.find(c_key);

  if (itr != table.end()) {
    table.erase(itr);
  }
}

} /// namespace eosio
