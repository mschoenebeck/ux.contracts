#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>


namespace eosiosystem {

    // function for converting checksum256 into string
    template<typename CharT>
    static std::string to_hex(const CharT* d, uint32_t s) {
      std::string r;
      const char* to_hex="0123456789abcdef";
      uint8_t* c = (uint8_t*)d;
      for( uint32_t i = 0; i < s; ++i ) {
        (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
      }
      return r;
    }

    // check if calling account is a qualified oracle
    bool is_oracle(const name owner){
      producers_table ptable("eosio"_n, name("eosio").value);
      auto p_idx = ptable.get_index<"prototalvote"_n>();
      auto p_itr = p_idx.begin();
      uint64_t count = 0;
      while (p_itr != p_idx.end()) {
        if (p_itr->owner==owner) return true;
        p_itr++;
        count++;
        if (count==21) break;
      }
      return false;
    }

    // called from settotalusg 
    // todo - calculate payment for oracles in future version
    void system_contract::set_total(uint64_t total_cpu_us, uint64_t total_net_words, time_point_sec period_start)
    {
        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});

        system_usage_history_table u_t(get_self(), get_self().value);
        auto itr = u_t.end();
        itr--;

        // Initial Inflation
        double MP = _resource_config_state.max_pay_constant;

        uint64_t draglimit = _resource_config_state.emadraglimit;
        uint64_t day_count = itr->daycount;

        // restrict inflation to the first 3 years after resource model deployment (2 normal years + 1 leap year)
        check( day_count < 1096, "inflation period has ended");

        double VT = pow(2, -(static_cast<double>(day_count) / 365)) * _resource_config_state.initial_value_transfer_rate;

        double previousAverageCPU = itr->ma_cpu;
        double previousAverageNET = itr->ma_net;


        uint64_t system_max_cpu = static_cast<uint64_t>(_gstate.max_block_cpu_usage) * 2 * 60 * 60 * 24;
        check( total_cpu_us <= system_max_cpu, "measured cpu usage is greater than system total");
        double usage_cpu = static_cast<double>(total_cpu_us) / system_max_cpu;
        print("usage_cpu:: ", std::to_string(usage_cpu), "\n");

        if(usage_cpu < 0.00000000001) {
            usage_cpu = 0.00000000001;
        }
        print("adjusted usage_cpu:: ", std::to_string(usage_cpu), "\n");

        uint64_t system_max_net = static_cast<uint64_t>(_gstate.max_block_net_usage) * 2 * 60 * 60 * 24;
        check( total_net_words * 8 <= system_max_net, "measured net usage is greater than system total");
        double usage_net = static_cast<double>(total_net_words * 8) / system_max_net;

        if(usage_net < 0.00000000001) {
            usage_net = 0.00000000001;
        }
        print("adjusted usage_net:: ", std::to_string(usage_cpu), "\n");

        double net_percent_total = usage_net / (usage_net + usage_cpu);
        double cpu_percent_total = usage_cpu / (usage_net + usage_cpu);

        print("system_max_net:: ", std::to_string(system_max_net), "\n");
        print("system_max_cpu:: ", std::to_string(system_max_cpu), "\n");

        double ma_cpu_total = 0.0;
        double ma_net_total = 0.0;

        itr = u_t.end();
        for (int i = 1; i < draglimit; i++)
        {
            itr--;
            ma_cpu_total += itr->use_cpu;
            ma_net_total += itr->use_net;

            if (itr == u_t.begin())
            {
                break;
            }
        }

        // calculate period for moving averages during bootstrap period
        uint8_t period = day_count < draglimit ? day_count + 1 : draglimit;

        print("period:: ", std::to_string(period), "\n");

        double UTIL_CPU_MA = ux::calcMA(ma_cpu_total, period, usage_cpu);
        double UTIL_NET_MA = ux::calcMA(ma_net_total, period, usage_net);

        double raw_util_cpu_ema;
        double raw_util_net_ema;

        uint64_t pk = u_t.available_primary_key();

        // use simple moving average until we reach draglimit samples
        if (pk >= draglimit)
        {
            raw_util_cpu_ema = ux::calcEMA(previousAverageCPU, draglimit, usage_cpu);
            raw_util_net_ema = ux::calcEMA(previousAverageNET, draglimit, usage_net);
        }
        else
        {
            raw_util_cpu_ema = UTIL_CPU_MA;
            raw_util_net_ema = UTIL_NET_MA;
        }
        double UTIL_CPU_EMA = raw_util_cpu_ema / 2;
        double UTIL_NET_EMA = raw_util_net_ema / 2;

        double UTIL_TOTAL_EMA = (UTIL_CPU_EMA + UTIL_NET_EMA);

        if(UTIL_TOTAL_EMA < 0.00000000001) {
            UTIL_CPU_EMA = UTIL_CPU_EMA / UTIL_TOTAL_EMA * 0.00000000001;
            UTIL_NET_EMA = UTIL_NET_EMA / UTIL_TOTAL_EMA * 0.00000000001;
            UTIL_TOTAL_EMA = 0.00000000001;
        }

        if(UTIL_TOTAL_EMA > 0.99999999999) {
            UTIL_CPU_EMA = UTIL_CPU_EMA / UTIL_TOTAL_EMA * 0.99999999999;
            UTIL_NET_EMA = UTIL_NET_EMA / UTIL_TOTAL_EMA * 0.99999999999;
            UTIL_TOTAL_EMA = 0.99999999999;
        }

        double inflation = (1 - UTIL_TOTAL_EMA) / (1 - UTIL_TOTAL_EMA - ux::get_c(UTIL_TOTAL_EMA) * VT) - 1;

        double BP_U = MP * ux::get_c(UTIL_TOTAL_EMA);
        double Upaygross = pow((1 + inflation), (1 - BP_U)) - 1;
        double Bppay = inflation - Upaygross;

        print("UTIL_CPU_MA:: ", std::to_string(UTIL_CPU_MA), "\n");
        print("UTIL_NET_MA:: ", std::to_string(UTIL_NET_MA), "\n");
        print("UTIL_TOTAL_EMA:: ", std::to_string(UTIL_TOTAL_EMA), "\n");

        print("inflation:: ", std::to_string(inflation), "\n");

        print("BP_U:: ", std::to_string(BP_U), "\n");

        print("Upaygross:: ", std::to_string(Upaygross), "\n");

        print("Bppay:: ", std::to_string(Bppay), "\n");

        const asset token_supply = eosio::token::get_supply(token_account, core_symbol().code());

        // Inflation waterfall
        double Min_Upaynet = inflation * UTIL_TOTAL_EMA;

        print("Min_Upaynet:: ", std::to_string(Min_Upaynet), "\n");

        double Waterfall_bp = inflation * (1 - UTIL_TOTAL_EMA);

        print("Waterfall_bp:: ", std::to_string(Waterfall_bp), "\n");

        double Bppay_final = fmin(Bppay, Waterfall_bp);

        print("Bppay_final:: ", std::to_string(Bppay_final), "\n");

        double Uppaynet = inflation - Bppay_final;

        print("Uppaynet:: ", std::to_string(Uppaynet), "\n");

        double Daily_i_U = pow(1 + inflation, static_cast<double>(1) / 365) - 1;

        print("Daily_i_U:: ", std::to_string(Daily_i_U), "\n");

        double utility_daily = (Uppaynet / inflation) * Daily_i_U;                               //allocate proportionally to Utility
        print("utility_daily:: ", std::to_string(utility_daily), "\n");

        double CPU_Pay = utility_daily * (UTIL_CPU_EMA / UTIL_TOTAL_EMA);
        print("CPU_Pay:: ", std::to_string(CPU_Pay), "\n");

        double NET_pay = utility_daily - CPU_Pay;
        print("NET_pay:: ", std::to_string(NET_pay), "\n");

        double bppay_daily = (Bppay_final / inflation) * Daily_i_U;                            //allocate proportionally to BPs
        print("bppay_daily:: ", std::to_string(bppay_daily), "\n");

        double Final_BP_daily = bppay_daily + NET_pay;
        print("Final_BP_daily:: ", std::to_string(Final_BP_daily), "\n");

        // calculate inflation amount
        auto utility_tokens = asset(static_cast<int64_t>( (CPU_Pay * double(token_supply.amount))), core_symbol() );
        auto bppay_tokens = asset(static_cast<int64_t>( ((Final_BP_daily) * double(token_supply.amount))), core_symbol() );

        print("utility_tokens:: ", utility_tokens.to_string(), "\n");
        print("bppay_tokens:: ", bppay_tokens.to_string(), "\n");

        u_t.emplace(get_self(), [&](auto &h) {
            h.id = pk;
            h.timestamp = period_start;
            h.daycount = day_count+1;
            h.total_cpu_us = total_cpu_us;
            h.total_net_words = total_net_words;
            h.net_percent_total = net_percent_total;
            h.cpu_percent_total = cpu_percent_total;
            h.use_cpu = usage_cpu;
            h.use_net = usage_net;
            h.ma_cpu = UTIL_CPU_MA;
            h.ma_net = UTIL_NET_MA;
            h.ema_cpu = raw_util_cpu_ema;
            h.ema_net = raw_util_net_ema;
            h.ema_util_total = UTIL_TOTAL_EMA;
            h.utility = Upaygross;
            h.utility_daily = utility_daily;
            h.bppay = Bppay;
            h.bppay_daily = bppay_daily;
            h.inflation = inflation;
            h.inflation_daily = Daily_i_U;
            h.utility_tokens = utility_tokens;
            h.bppay_tokens = bppay_tokens;
        });

        _resource_config.set( _resource_config_state, get_self() );
    }

    // called from settotalusg 
    void system_contract::issue_inflation(time_point_sec period_start) {
        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});

        system_usage_history_table u_t(get_self(), get_self().value);
        auto itr_u = u_t.end();
        itr_u--;

         asset inflation = itr_u->bppay_tokens + itr_u->utility_tokens;

         // defensive measure to cap daily inflation at 1.5m UTX
         asset cap = asset(10000, core_symbol()) * 1500000;

         check(inflation <= cap, "period inflation cap exceeded");

         // get max_supply of core token so can reduce inflation tokens to reach it if close
         stats statstable( token_account, core_symbol().code().raw());
         const auto& st = statstable.get(core_symbol().code().raw());
         asset supply_remaining = st.max_supply - st.supply;
         check(supply_remaining.amount > 0, "cannot exceed max token supply");

         if (inflation > supply_remaining) {

            // reduce tokens bp and utiltity tokens proportionally
            asset new_bppay_tokens = asset(static_cast<double>(itr_u->bppay_tokens.amount) / static_cast<double>(inflation.amount) * static_cast<double>(supply_remaining.amount), core_symbol());
            asset new_utility_tokens = supply_remaining - new_bppay_tokens;

            // update table data (token values only)
            inflation = new_bppay_tokens + new_utility_tokens;
            u_t.modify(itr_u, same_payer, [&](auto &h) {
                h.bppay_tokens = new_bppay_tokens;
                h.utility_tokens = new_utility_tokens;
            });
         }

         if (inflation.amount > 0) {
            token::issue_action issue_act{token_account, {{get_self(), active_permission}}};
            issue_act.send(get_self(), inflation, "issue daily inflation");
         }

         token::transfer_action transfer_act{token_account, {{get_self(), active_permission}}};
         if (itr_u->bppay_tokens.amount > 0) {
            transfer_act.send(get_self(), bpay_account, itr_u->bppay_tokens, "producer daily");
         }
         if (itr_u->utility_tokens.amount > 0) {
            transfer_act.send(get_self(), upay_account, itr_u->utility_tokens, "usage daily");
         }

         std::vector<name> active_producers;
         auto idx = _producers.get_index<"prototalvote"_n>();
         for( auto it = idx.cbegin(); it != idx.cend() && active_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
             active_producers.emplace_back(it->owner);
         }

        // check there are enough active producers
         auto active_producer_count = active_producers.size();
         check(active_producer_count > 0, "No active producers");
         check(active_producer_count == _gstate.last_producer_schedule_size, "active_producers must equal last_producer_schedule_size");

         asset earned_pay = asset(itr_u->bppay_tokens.amount / active_producer_count, core_symbol());
         if (earned_pay.amount > 0) {
             for (const auto &p : active_producers)
             {

                auto pay_itr = _producer_pay.find(p.value);

                if (pay_itr == _producer_pay.end())
                {
                   pay_itr = _producer_pay.emplace(p, [&](auto &pay) {
                      pay.owner = p;
                      pay.balance = earned_pay;
                   });
                }
                else
                {
                   _producer_pay.modify(pay_itr, same_payer, [&](auto &pay) {
                      pay.balance += earned_pay;
                   });
                }
             }
         }

        _resource_config_state.last_period_inflation_print = period_start;
        _resource_config.set( _resource_config_state, get_self() );
    }


    ACTION system_contract::initresource(uint16_t dataset_batch_size, uint16_t oracle_consensus_threshold, time_point_sec period_start, uint32_t period_seconds, double initial_value_transfer_rate, double max_pay_constant)
    {
        require_auth(get_self());

        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});
        _resource_config_state.dataset_batch_size = dataset_batch_size;
        _resource_config_state.oracle_consensus_threshold = oracle_consensus_threshold;
        _resource_config_state.period_start = period_start;
        _resource_config_state.period_seconds = period_seconds;
        _resource_config_state.initial_value_transfer_rate = initial_value_transfer_rate;
        _resource_config_state.max_pay_constant = max_pay_constant;
        _resource_config_state.last_period_inflation_print = time_point_sec();
        _resource_config_state.active = false;

        system_usage_history_table u_t(get_self(), get_self().value);
        if (u_t.begin() == u_t.end()) {
            uint64_t pk = u_t.available_primary_key();
            u_t.emplace(get_self(), [&](auto &u) {
                u.id = pk;
                u.timestamp = period_start;
                u.use_cpu = 0;
                u.use_net = 0;
                u.daycount = 0;
                u.ma_cpu = 0;
                u.ma_net = 0;
                u.ema_cpu = 0;
                u.ema_net = 0;
                u.utility_daily = 0;
                u.bppay_daily = 0;
            });

            // only remove submission state if no history
            _resource_config_state.submitting_oracles = {};
            _resource_config_state.inflation_transferred = false;
            _resource_config_state.account_distributions_made = {};

        }
        _resource_config.set( _resource_config_state, get_self() );
    }

    // sets total resources used by system (for calling oracle)
    // this must be called by oracle before adding individual cpu usage
    ACTION system_contract::settotalusg(name source, uint64_t total_cpu_us, uint64_t total_net_words, checksum256 all_data_hash, time_point_sec period_start)
    {
        require_auth(source);
        check(is_oracle(source) == true, "not a qualified oracle");

        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});

        check(_resource_config_state.active, "resource model not active");

        check(_resource_config_state.period_start == period_start, "period_start does not match current period_start");

        // check submissions are within system limits
        uint64_t system_max_cpu = static_cast<uint64_t>(_gstate.max_block_cpu_usage) * 2 * 60 * 60 * 24;
        check( total_cpu_us <= system_max_cpu, "measured cpu usage is greater than system total");
        uint64_t system_max_net = static_cast<uint64_t>(_gstate.max_block_net_usage) * 2 * 60 * 60 * 24;
        check( total_net_words * 8 <= system_max_net, "measured net usage is greater than system total");

        system_usage_table u_t(get_self(), get_self().value);
        auto itr = u_t.find(source.value);

        check(itr == u_t.end(), "total already set");

        // hash submitted data
        std::string datatext = std::to_string(total_cpu_us) + "-" + std::to_string(total_net_words);
        checksum256 hash = sha256(datatext.c_str(), datatext.size());
        std::vector<metric> data {
            {"cpu.us"_n, total_cpu_us},
            {"net.words"_n, total_net_words}
        };

        // add data and hash to table if not already present
        datasets_table d_t(get_self(), get_self().value);
        auto dt_hash_index = d_t.get_index<"hash"_n>();
        auto dt_itr = dt_hash_index.find(hash);
        if (dt_itr->hash != hash) {
            d_t.emplace(get_self(), [&](auto& t) {
                t.id = d_t.available_primary_key();
                t.hash = hash;
                t.data = data;
            });
        }

        // add totals data
        u_t.emplace(source, [&](auto& t) {
            t.source = source;
            t.total_cpu_us = total_cpu_us;
            t.total_net_words = total_net_words;
            t.submission_hash_list.push_back(hash);
            t.all_data_hash = all_data_hash;
        });

        _resource_config_state.submitting_oracles.push_back(source);


        // distribute inflation (if it hasn't been done)
        if (!_resource_config_state.inflation_transferred) {
            std::map<checksum256, uint8_t> hash_count;
            auto oracles = _resource_config_state.submitting_oracles;
            if (oracles.size() >= _resource_config_state.oracle_consensus_threshold) {

                // count number of each hash
                system_usage_table u_t(get_self(), get_self().value);
                for (int i=0; i<oracles.size(); i++) {
                    auto ut_itr = u_t.begin();
                    ut_itr = u_t.find(oracles[i].value);
                    if (ut_itr->submission_hash_list.size() >= 1) {
                        hash_count[ut_itr->submission_hash_list[0]]++;
                    }
                }

                // establish modal hash
                checksum256 modal_hash;
                uint8_t mode_count = 0;
                for (auto const& x : hash_count) {
                    if (x.second > mode_count) {
                        modal_hash = x.first;
                        mode_count = x.second;
                    }
                }

                // find dataset corresponding to modal hash, and distribute inflation
                if (mode_count >= _resource_config_state.oracle_consensus_threshold) {
                    std::vector<metric> accounts_usage_data;
                    datasets_table d_t(get_self(), get_self().value);
                    auto dt_hash_index = d_t.get_index<"hash"_n>();
                    auto dt_itr = dt_hash_index.find(modal_hash);
                    if (dt_itr->hash == modal_hash) {
                        auto cpu_usage_us = dt_itr->data[0].u;
                        auto net_usage_words = dt_itr->data[1].u;
                        set_total(cpu_usage_us, net_usage_words, period_start);
                        issue_inflation(period_start);
                        _resource_config_state.inflation_transferred = true;
                    }

                }

            }

        } // end of inflation distribution

        _resource_config.set( _resource_config_state, get_self() );
    }

    // adds the CPU used by the accounts included (for calling oracle)
    // called after the oracle has set the total
    ACTION system_contract::addactusg(name source, uint16_t dataset_id, const std::vector<metric>& dataset, time_point_sec period_start)
    {  
        require_auth(source);
        check(is_oracle(source) == true, "not a qualified oracle");

        int length = dataset.size();

        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});

        check(_resource_config_state.active, "resource model not active");

        check(length<=_resource_config_state.dataset_batch_size, "must supply fewer dataset values");
        check(_resource_config_state.period_start == period_start, "period_start does not match current period_start");

        check(_resource_config_state.inflation_transferred == true, "inflation not yet transferred");

        system_usage_table u_t(get_self(), get_self().value);
        auto ut_itr = u_t.find(source.value);
        check(ut_itr != u_t.end(), "usage totals not set");
        check(dataset_id == ut_itr->submission_hash_list.size(), "dataset_id differs from expected value");

        std::string datatext = "";
        for (int i=0; i<length; i++) {
            auto account = dataset[i].a;
            auto cpu_usage_us = dataset[i].u;
            check(cpu_usage_us > 0, "account cpu measurement must be greater than 0");

            // add cpu to allocated amount for oracle to ensure not exceeding declared total
            auto unallocated_cpu = ut_itr->total_cpu_us - ut_itr->allocated_cpu;
            check(unallocated_cpu >= cpu_usage_us, "insufficient unallocated cpu");
            u_t.modify(ut_itr, source, [&](auto& t) {
                t.allocated_cpu += cpu_usage_us;
            });

            // append usage pair to string for hashing
            datatext += account.to_string();
            datatext += std::to_string(cpu_usage_us);
        }

        // hash submitted dataset
        checksum256 hash = sha256(datatext.c_str(), datatext.size());
        u_t.modify(ut_itr, source, [&](auto& t) {
            t.submission_hash_list.push_back(hash);
        });

        // add data and hash to table if not already present
        datasets_table d_t(get_self(), get_self().value);
        auto dt_hash_index = d_t.get_index<"hash"_n>();
        auto dt_itr = dt_hash_index.find(hash);
        if (dt_itr->hash != hash) {
            d_t.emplace(source, [&](auto& t) {
                t.id = d_t.available_primary_key();
                t.hash = hash;
                t.data = dataset;
            });
        }

        // distribute user account rewards
        std::map<checksum256, uint8_t> hash_count;
        auto oracles = _resource_config_state.submitting_oracles;
        if (oracles.size() >= _resource_config_state.oracle_consensus_threshold) {

            // count number of each hash
            system_usage_table u_t(get_self(), get_self().value);
            for (int i=0; i<oracles.size(); i++) {
                auto ut_itr = u_t.begin();
                ut_itr = u_t.find(oracles[i].value);
                if (ut_itr->submission_hash_list.size() >= (dataset_id+1)) {
                    hash_count[ut_itr->submission_hash_list[dataset_id]]++;
                }
            }

            // establish modal hash
            checksum256 modal_hash;
            uint8_t mode_count = 0;
            for (auto const& x : hash_count) {
                if (x.second > mode_count) {
                    modal_hash = x.first;
                    mode_count = x.second;
                }
            }

            // find dataset corresponding to modal hash, and distribute inflation if not done
            auto v = _resource_config_state.account_distributions_made;
            if (std::find(v.begin(), v.end(), dataset_id) == v.end()) {

                if (mode_count >= _resource_config_state.oracle_consensus_threshold) {
                    std::vector<metric> accounts_usage_data;
                    datasets_table d_t(get_self(), get_self().value);
                    auto dt_hash_index = d_t.get_index<"hash"_n>();
                    auto dt_itr = dt_hash_index.find(modal_hash);
                    if (dt_itr->hash == modal_hash) {
                        accounts_usage_data = dt_itr->data;
                    }

                    // get total_cpu from last system_usage_history record
                    system_usage_history_table suh_t(get_self(), get_self().value);
                    auto suh_itr = suh_t.end();
                    suh_itr--;
                    auto total_cpu = suh_itr->total_cpu_us;
                    auto utility_tokens_amount = suh_itr->utility_tokens.amount;

                    // expensive part (100 accounts in ~9000us)
                    auto core_sym = core_symbol();
                    account_pay_table ap_t(get_self(), get_self().value);
                    for (int i=0; i<accounts_usage_data.size(); i++) {
                        auto account = accounts_usage_data[i].a;
                        auto account_cpu = accounts_usage_data[i].u;
                        auto add_claim = (static_cast<double>(account_cpu) / total_cpu) * utility_tokens_amount;
                        asset payout = asset(add_claim, core_symbol());
                        auto ap_itr = ap_t.find(account.value);
                        if (ap_itr == ap_t.end()) {
                            ap_t.emplace(get_self(), [&](auto& t) {
                                t.account = account;
                                t.balance = payout;
                                t.timestamp = period_start;
                            });
                        } else {
                            ap_t.modify(ap_itr, get_self(), [&](auto& t) {
                                t.balance += payout;
                                t.timestamp = period_start;
                            });
                        }

                    }
                    _resource_config_state.account_distributions_made.push_back(dataset_id);
                }
            } // end of account distributions


        }

        _resource_config.set( _resource_config_state, get_self() );
    }

    // called by anyone from 48hrs after the current period start
    // clears tables and advances period start
    ACTION system_contract::nextperiod()
    {
        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});

        check(_resource_config_state.active, "resource model not active");

        auto current_seconds = current_time_point().sec_since_epoch();
        auto period_start_seconds = _resource_config_state.period_start.sec_since_epoch();
        check(current_seconds >= (period_start_seconds + (_resource_config_state.period_seconds)), "current resource period has not ended");
        if (current_seconds >= (period_start_seconds + (_resource_config_state.period_seconds))) {

            // erase records ready for next periods submissions
            datasets_table d_t(get_self(), get_self().value);
            auto dt_itr = d_t.begin();
            while (dt_itr != d_t.end()) {
                dt_itr = d_t.erase(dt_itr);
            }

            system_usage_table u_t(get_self(), get_self().value);

            auto oracles = _resource_config_state.submitting_oracles;

            // find modal all_data_hash
            std::map<checksum256, uint8_t> hash_count;
            checksum256 modal_hash;
            uint8_t mode_count = 0;
            if (oracles.size() >= _resource_config_state.oracle_consensus_threshold) {
                // count number of each hash
                for (int i=0; i<oracles.size(); i++) {
                    auto ut_itr = u_t.begin();
                    ut_itr = u_t.find(oracles[i].value);
                    hash_count[ut_itr->all_data_hash]++;
                }
                // establish modal hash
                for (auto const& x : hash_count) {
                    if (x.second > mode_count) {
                        modal_hash = x.first;
                        mode_count = x.second;
                    }
                }
            }

            // score and reward submissions based on commitment hash and modal agreement
            auto core_sym = core_symbol();
            account_pay_table ap_t(get_self(), get_self().value);

            auto oracle_full_data_mode_count = 0;

            for (int i=0; i<oracles.size(); i++) {
                auto ut_itr = u_t.begin();
                ut_itr = u_t.find(oracles[i].value);

                std::string datatext = "";
                for (int ii=0; ii<ut_itr->submission_hash_list.size(); ii++) {
                    checksum256 hash = ut_itr->submission_hash_list[ii];
                    datatext += to_hex(hash.extract_as_byte_array().data(), sizeof(hash));
                    if (ii < ut_itr->submission_hash_list.size()-1) {
                        datatext += "-";
                    }
                }

                if (mode_count >= _resource_config_state.oracle_consensus_threshold) {
                    uint64_t oracle_points = 0;
                    checksum256 commit_hash = ut_itr->all_data_hash;
                    checksum256 reveal_hash = sha256(datatext.c_str(), datatext.size());
                    if (reveal_hash == commit_hash) {
                        oracle_points = 1; // data is as declared
                        if ((commit_hash == modal_hash) && (mode_count >= _resource_config_state.oracle_consensus_threshold)) {
                            oracle_points += 9; // data is same as modal data
                            oracle_full_data_mode_count += 1;
                        }
                    }

                    // add/modify score in sources table
                    sources_table s_t(get_self(), get_self().value);
                    auto st_itr = s_t.find(oracles[i].value);
                    if (st_itr == s_t.end()) {
                        s_t.emplace(get_self(), [&](auto& t) {
                            t.account = oracles[i];
                            t.submissions_score = oracle_points;
                            t.submissions_count = 1;
                        });
                    } else {
                        s_t.modify(st_itr, get_self(), [&](auto& t) {
                            t.submissions_score += oracle_points;
                            t.submissions_count += 1;
                        });
                    }

                    // // todo - add oracle payment in future version
                    // auto amount = 0;
                    // asset payout = asset(amount, core_symbol());
                    // auto ap_itr = ap_t.find(oracles[i].value);
                    // if (ap_itr == ap_t.end()) {
                    //     ap_t.emplace(get_self(), [&](auto& t) {
                    //         t.account = oracles[i];
                    //         t.balance = payout;
                    //         t.timestamp = _resource_config_state.period_start;
                    //     });
                    // } else {
                    //     ap_t.modify(ap_itr, get_self(), [&](auto& t) {
                    //         t.balance += payout;
                    //         t.timestamp = _resource_config_state.period_start;
                    //     });
                    // }

                }
            }

            // prevent period advancing if no modal data was received
            check(oracle_full_data_mode_count >= _resource_config_state.oracle_consensus_threshold, "full modal data not received");

            // erase records ready for next periods submissions
            auto ut_itr = u_t.begin();
            while (ut_itr != u_t.end()) {
                ut_itr = u_t.erase(ut_itr);
            }

            _resource_config_state.submitting_oracles.clear();
            _resource_config_state.period_start = time_point_sec(_resource_config_state.period_start.sec_since_epoch() + _resource_config_state.period_seconds);
            _resource_config_state.inflation_transferred = false;
            _resource_config_state.account_distributions_made.clear();
        }

        _resource_config.set( _resource_config_state, get_self() );
    }

    // called by individual accounts to claim their distribution
    ACTION system_contract::claimdistrib(name account)
    {
        require_auth(account);

        account_pay_table a_t(get_self(), get_self().value);
        auto itr = a_t.find(account.value);
        check(itr != a_t.end(), "account balance not found");
        check(itr->balance != asset( 0, core_symbol() ), "no balance to claim");

        token::transfer_action transfer_act{token_account, {{upay_account, active_permission}, {account, active_permission}}};
        transfer_act.send(upay_account, account, itr->balance, "utility reward");
        itr = a_t.erase(itr);
    }

    // activate/deactivate resource model inflation
    ACTION system_contract::resactivate(bool active) {
        require_auth(get_self());

        auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});

        // check active state isn't already as requested
        if (active) {
            check(!_resource_config_state.active, "resource model already active");
        } else {
            check(_resource_config_state.active, "resource model already inactive");
        }

        _resource_config_state.active = active;
        _resource_config.set( _resource_config_state, get_self() );
    }

    #ifdef INCLUDECLEARACTIONS
        ACTION system_contract::clrresource() {
            require_auth(get_self());

            auto _resource_config_state = _resource_config.get_or_create(_self, resource_config_state{});
            check(!_resource_config_state.active, "must deactivate before clearing");

            system_usage_history_table uh_t(get_self(), get_self().value);
            auto uh_itr = uh_t.begin();
            while (uh_itr != uh_t.end()) {
                uh_itr = uh_t.erase(uh_itr);
            }

            datasets_table d_t(get_self(), get_self().value);
            auto dt_itr = d_t.begin();
            while (dt_itr != d_t.end()) {
                dt_itr = d_t.erase(dt_itr);
            }

            account_pay_table a_t(get_self(), get_self().value);
            auto a_itr = a_t.begin();
            while (a_itr != a_t.end()) {
                a_itr = a_t.erase(a_itr);
            }

            system_usage_table u_t(get_self(), get_self().value);
            auto u_itr = u_t.begin();
            while (u_itr != u_t.end()) {
                u_itr = u_t.erase(u_itr);
            }

            sources_table s_t(get_self(), get_self().value);
            auto s_itr = s_t.begin();
            while (s_itr != s_t.end()) {
                s_itr = s_t.erase(s_itr);
            }

            if (_resource_config.exists()) _resource_config.remove();
        }
    #endif

}