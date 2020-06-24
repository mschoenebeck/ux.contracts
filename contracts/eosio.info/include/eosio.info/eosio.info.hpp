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
    * eosio.info contract defines the structures and actions that allows user KYC metadata to be stored and easily accessed
    */
   class [[eosio::contract("eosio.info")]] info : public contract {
      public:
         using contract::contract;

         /**
          * adds a key/verfication definition, called by KYC provider
          */
         ACTION addkeytype(const name key, std::string definition, const bool user);

         /**
          * removes a key/verfication definition, called by KYC provider
          */
         //ACTION delkeytype(const name key);

         /**
          * adds a user verfication, called by KYC provider
          */
         ACTION adduserver(const name user, const name verification_type);

         /**
          * removes a user verification, called by KYC provider
          */
         // ACTION deluserver(const name user, const name verification_type);

         /**
          * sets a key/verfication offer, called by KYC user
          */
         ACTION setuserkey(const name user, const name verification_type, const std::string memo);

         /**
          * deletes a key/verfication offer, called by KYC user
          */
         ACTION deluserkey(const name user, const name verification_type);


         static uint128_t composite_key(const uint64_t &x, const uint64_t &y) {
            return (uint128_t{x} << 64) | y;
         }

      private:

         /**
          * stores the verification definitions
          */
         struct [[eosio::table]] infovertype {
            name          key;
            std::string   definition;
            bool          user;

            uint64_t primary_key()const { return key.value; }
         };

         /**
          * stores the verifications added by the KYC provider
          */
         struct [[eosio::table]] infouserver {
            uint64_t      id;
            uint128_t     key;
            name          user;
            name          verification_type;

            uint64_t primary_key()const { return id; }
            uint128_t by_key() const {return key; }
            uint64_t by_user() const {return user.value; }
            uint64_t by_verification_type() const {return verification_type.value; }
         };

         /**
          * stores the verifications offered by user
          */
         struct [[eosio::table]] infouserkey {
            uint64_t      id;
            uint128_t     key;
            name          user;
            name          verification_type;
            std::string   memo;

            uint64_t primary_key()const { return id; }
            uint128_t by_key() const {return key; }
            uint64_t by_user() const {return user.value; }
            uint64_t by_verification_type() const {return verification_type.value; }
         };

         typedef eosio::multi_index< "infovertypes"_n, infovertype > infovertypes_table;
         typedef eosio::multi_index< "infouservers"_n, infouserver,
            indexed_by<"key"_n, const_mem_fun<infouserver, uint128_t, &infouserver::by_key> >, 
            indexed_by<"user"_n, const_mem_fun<infouserver, uint64_t, &infouserver::by_user> > > infouservers_table;
         typedef eosio::multi_index< "infouserkeys"_n, infouserkey,
            indexed_by<"key"_n, const_mem_fun<infouserkey, uint128_t, &infouserkey::by_key> >, 
            indexed_by<"user"_n, const_mem_fun<infouserkey, uint64_t, &infouserkey::by_user> > > infouserkeys_table;

   };

}
