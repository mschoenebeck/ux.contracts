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
          * Adds a kyc account to the kycaccounts table, called by eosio
          *
          * @param account - the name of the account to add
          *
          * @pre account must not already exist in kycaccounts table
          */
         ACTION addkycacc(const name account);

         /**
          * Deletes a kyc account from the kycaccounts table, called by eosio
          *
          * @param account - the name of the account to delete
          *
          * @pre account must exist in kycaccounts table
          */
         ACTION delkycacc(const name account);

         /**
          * Adds a key/verfication type definition, called by eosio
          *
          * @param key - the name of the verification or user info key
          * @param definition - a text string containing what the key refers to
          * @param user - whether the key refers to user info (true) or verification key (false)
          *
          * @pre key must not already exist in keytypes table
          */
         ACTION addkeytype(const name key, std::string definition, const bool user);

         /**
          * Updates a key/verfication definition, called by eosio
          *
          * @param key - the name of the verification or user info key
          * @param user -  whether the key refers to user info (true) or verification key (false)
          *
          * @pre key must be present in keytypes table
          */
         ACTION updkeytype(const name key, std::string definition, const bool user);

         /**
          * Adds a user verfication record, called by KYC provider
          *
          * @param kyc_account - the name of the kyc account
          * @param user - the name of the user account
          * @param verification_type - the name of the verification key
          *
          * @pre kyc_account must be a key present in kycaccounts table
          * @pre verification_key must be a key present in keytypes table
          */
         ACTION adduserver(const name kyc_account, const name user, const name verification_key);

         /**
          * Removes a user verification, called by KYC provider
          *
          * @param kyc_account - the name of the kyc account
          * @param user - the name of the user account
          * @param verification_key - the name of the verification key
          *
          * @pre kyc_account must be a key present in kycaccounts table
          * @pre matching record must be present in userverifs table
          */
         ACTION deluserver(const name kyc_account, const name user, const name verification_key);

         /**
          * Allows a user to make a key/verfication record available, called by KYC user
          *
          * @param user - the name of the user account
          * @param key - the name of the verification or user key
          * @param memo - a text string of up to 1024 characters
          *
          * @pre key must be a key present in keytypes table
          * @pre memo length must not exceed 1024 characters
          */
         ACTION setuserkey(const name user, const name key, const std::string memo);

         /**
          * Deletes a key/verfication record, called by KYC user
          *
          * @param user - the name of the user account
          * @param key - the name of the verification key
          *
          * @pre record must be present in userkeys table
          */
         ACTION deluserkey(const name user, const name key);


         static uint128_t composite_key(const uint64_t &x, const uint64_t &y) {
            return (uint128_t{x} << 64) | y;
         }

      private:

         /**
          * stores the kyc source accounts
          */
         struct [[eosio::table]] kycaccount {
            name          account;

            uint64_t primary_key() const { return account.value; }
         };

         /**
          * stores the verification definitions
          */
         struct [[eosio::table]] keytype {
            name          key;
            std::string   definition;
            bool          user;

            uint64_t primary_key() const { return key.value; }
         };

         /**
          * stores the verifications added by the KYC provider
          */
         struct [[eosio::table]] userverif {
            uint64_t      id;
            name          user;
            name          verification_key;
            time_point    timestamp;

            uint64_t primary_key() const { return id; }
            uint128_t by_ckey() const {return composite_key(user.value, verification_key.value); }
            uint64_t by_user() const {return user.value; }
            uint64_t by_verification_key() const {return verification_key.value; }
         };

         /**
          * stores the keys/verifications provided by user
          */
         struct [[eosio::table]] userkey {
            uint64_t      id;
            name          user;
            name          key;
            std::string   memo;

            uint64_t primary_key() const { return id; }
            uint128_t by_ckey() const {return composite_key(user.value, key.value); }
            uint64_t by_user() const {return user.value; }
            uint64_t by_key() const {return key.value; }
         };

         typedef eosio::multi_index< "kycaccounts"_n, kycaccount > kycaccounts_table;
         typedef eosio::multi_index< "keytypes"_n, keytype > keytypes_table;
         typedef eosio::multi_index< "userverifs"_n, userverif,
            indexed_by<"ckey"_n, const_mem_fun<userverif, uint128_t, &userverif::by_ckey> >, 
            indexed_by<"user"_n, const_mem_fun<userverif, uint64_t, &userverif::by_user> >,
            indexed_by<"key"_n, const_mem_fun<userverif, uint64_t, &userverif::by_verification_key> > > userverifs_table;
         typedef eosio::multi_index< "userkeys"_n, userkey,
            indexed_by<"ckey"_n, const_mem_fun<userkey, uint128_t, &userkey::by_ckey> >, 
            indexed_by<"user"_n, const_mem_fun<userkey, uint64_t, &userkey::by_user> > > userkeys_table;

   };

}
