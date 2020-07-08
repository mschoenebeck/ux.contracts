#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   /**
    * eosio.freeze contract defines the structures and actions that allow users to freeze their accounts making them immutable to code, abi or permission updates
    * This contract should be set to be immutable itself
    */
   class [[eosio::contract("eosio.freeze")]] freeze : public contract {
      public:
         using contract::contract;

         /**
          * Freezes the current code deployed on account, making all future code updates fail
          *
          * @param account - the account to freeze
          *
          * @pre record must not be present in frozenaccs table
          */
         ACTION freezeacc(const name account);

         /**
          * stores contracts that are frozen and cannot be updated anymore
          */
         struct [[eosio::table]] frozenaccs {
            name      account;

            uint64_t primary_key() const { return account.value; }
         };

         typedef eosio::multi_index< "frozenaccs"_n, frozenaccs > frozen_table;
   };

}
