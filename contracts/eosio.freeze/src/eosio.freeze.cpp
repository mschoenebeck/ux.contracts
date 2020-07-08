#include <eosio/system.hpp>
#include <eosio.freeze/eosio.freeze.hpp>

namespace eosio {

void freeze::freezeacc(const name account)
{
  // requires owner permission to freeze code
  require_auth(permission_level{account, name("owner")});

  frozen_table table(get_self(), get_self().value);
  auto itr = table.find(account.value);

  check(itr == table.end(), "account already frozen");

  table.emplace(account, [&](auto &t) { t.account = account; });
}
} /// namespace eosio
