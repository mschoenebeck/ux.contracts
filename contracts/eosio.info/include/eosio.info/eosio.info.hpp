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
    * eosio.info contract defines the structures and actions that allows user KYC metadata to be stored and efficiently accessed
    */
   class [[eosio::contract("eosio.info")]] info : public contract {
      public:
         using contract::contract;

         /**
          * Adds a key/verfication type definition, called by KYC provider
          *
          * @param key - the name of the verification or user info key
          * @param definition - a text string containing what the key refers to
          * @param user - whether the key refers to user info (false = verification key)
          *
          * @pre key must not already exist in infovertype table
          */
         ACTION addkeytype(const name key, std::string definition, const bool user);

         /**
          * Removes a key/verfication definition, called by KYC provider
          *
          * @param key - the name of the verification or user info key
          *
          * @pre key must be present in infovertype table
          */
         //ACTION delkeytype(const name key);

         /**
          * Adds a user verfication record, called by KYC provider
          *
          * @param user - the name of the user account
          * @param verification_type - the name of the verification key
          *
          * @pre verification_type must be a key present in infovertype table
          */
         ACTION adduserver(const name user, const name verification_type);

         /**
          * Removes a user verification, called by KYC provider
          *
          * @param user - the name of the user account
          * @param verification_type - the name of the verification key
          *
          * @pre matching record must be present in infouserver table
          */
         // ACTION deluserver(const name user, const name verification_type);

         /**
          * Allows a user to make a key/verfication record available, called by KYC user
          *
          * @param user - the name of the user account
          * @param verification_type - the name of the verification key
          * @param memo - a text string of up to 1024 characters
          *
          * @pre verification_type must be a key present in infovertype table
          * @pre memo length must not exceed 1024 characters
          */
         ACTION setuserkey(const name user, const name verification_type, const std::string memo);

         /**
          * Deletes a key/verfication offer, called by KYC user
          *
          * @param user - the name of the user account
          * @param verification_type - the name of the verification key
          *
          * @pre record must be present in infouserkey table
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
          * stores the keys/verifications provided by user
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
