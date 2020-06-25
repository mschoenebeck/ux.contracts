#include <eosio.info/eosio.info.hpp>

namespace eosio {

ACTION info::addkeytype(const name key, std::string definition, const bool user)
{
  require_auth(_self);

  infokeytypes_table table(_self, _self.value);

  auto itr = table.find(key.value);
  check(itr == table.end(), "key already exists");

  table.emplace(_self, [&](auto& t) {
    t.key = key;
    t.definition = definition;
    t.user = user;
  });
}

ACTION info::adduserver(const name user, const name verification_key)
{
  require_auth(_self);

  infokeytypes_table kt_table(_self, _self.value);
  auto kt_itr = kt_table.find(verification_key.value);
  check(kt_itr != kt_table.end(), "verification_key not permitted");
//  check(!t_itr->user, "verification_key is user key");

  infouservers_table table(_self, _self.value);
  auto ckey = composite_key(user.value, verification_key.value);
  auto idx = table.get_index<"ckey"_n>();
  auto itr = table.find(ckey);

  check(itr == table.end(), "user verification already present");

  table.emplace(user, [&](auto& t) {
    t.id = table.available_primary_key();
    t.ckey = ckey;
    t.user = user;
    t.verification_key = verification_key;
  });
}

ACTION info::setuserkey(const name user, const name key, const std::string memo)
{
  require_auth(user);

  check(memo.length() <= 1024, "memo exceeds permitted length of 1024 chars");

  infokeytypes_table kt_table(_self, _self.value);
  auto kt_itr = kt_table.find(key.value);
  check(kt_itr != kt_table.end(), "verification_key not recognised");
//  check(t_itr->user, "verification_key not user key");

  infouserkeys_table table(_self, _self.value);
  auto ckey = composite_key(user.value, key.value);
  auto idx = table.get_index<"ckey"_n>();
  auto itr = table.find(ckey);

  if (itr == table.end()) {
    table.emplace(user, [&](auto& t) {
      t.id = table.available_primary_key();
      t.ckey = ckey;
      t.user = user;
      t.key = key;
      t.memo = memo;
    });
  } else {
    table.modify(*itr, user, [&]( auto& t) {
      t.memo = memo;
    });
  }
}

ACTION info::deluserkey(const name user, const name key)
{
  require_auth(user);

  infouserkeys_table table(_self, _self.value);
  auto ckey = composite_key(user.value, key.value);
  auto idx = table.get_index<"ckey"_n>();
  auto itr = table.find(ckey);

  check(itr != table.end(), "user key not present");

  table.erase(itr);
}

} /// namespace eosio
