#pragma once
#include <eosio/crypto.hpp>
#include <eosio/system.h>
#include <eosio/system.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>

#include <vector>
#include <string>

#define GLOBAL_ID_BET                     101
#define GLOBAL_ID_HISTORY_INDEX           104
#define GLOBAL_ID_LARGE_HISTORY_INDEX     105
#define GLOBAL_ID_HIGH_ODDS_HISTORY_INDEX 106
#define GLOBAL_ID_ACTIVE                  109        

#define GLOBAL_ID_ACCU_START_TIME        102 
#define GLOBAL_ID_ACCU_STOP_TIME         103 
#define GLOBAL_ID_STATIC_TOTAL           107   
#define GLOBAL_ID_STATIC_DAILY           108   
#define GLOBAL_ID_LUCKNUM_START_TIME     110       
#define GLOBAL_ID_LUCKNUM_STOP_TIME      111        
#define GLOBAL_ID_LUCKNUM_START_BETNUMS  112        
#define GLOBAL_ID_LUCKNUM_STOP_BETNUMS   113        
#define GLOBAL_ID_LUCK_DRAW_TOTAL        114        
#define GLOBAL_ID_LUCK_DRAW_ACTIVE       115        
#define GLOBAL_ID_LUCK_DRAW_LUCKY        116        
#define GLOBAL_ID_TOKEN_MULTIPLE         118       
#define GLOBAL_ID_MIN_JACKPOT_BET        119
#define GLOBAL_ID_JACKPOT_ACTIVE         120
#define GLOBAL_ID_DIVIDEND_PERCENT       121
#define GLOBAL_ID_MAX_BET_PER            122

#define BET_HISTORY_LEN 40
#define WONDER_HIGH_ODDS 20
#define FEE 2 
#define SINGLE_BET_MAX_PERCENT 1.5
#define BETTOR_TOKEN_FEE 8      
#define INVITER_TOKEN_FEE 1
#define ROLL_BORDER_MIN 2
#define ROLL_BORDER_MAX 97
#define ROLL_TYPE_SMALL 1 
#define ROLL_TYPE_BIG 2 
#define BET_MAX_NUM 100 
#define INVITE_BONUS 0.005 
#define JACKPOT_BONUS 0.005 
#define DAY_SECONDS 86400
#define HOUR_SECONDS 3600
#define LUCK_DRAW_MAX 10001

using namespace eosio;
using namespace std;
/********* random ***********/
namespace eoswin {  
    class random {
    public:
      template<class T>
      TABLE data {
        T content;
        int block;
        int prefix;

        data(T t) {
          content   = t;
          block  = tapos_block_num();
          prefix = tapos_block_prefix();
        }
      };

      TABLE st_seeds {
        checksum256 seed1;
        checksum256 seed2;
      };

    public:
      template<class T>
      checksum256 create_sys_seed(T mixed) const;

      void seed(checksum256 sseed, checksum256 useed);

      void mixseed(checksum256& sseed, checksum256& useed, checksum256& result) const;

      // generator number ranged [0, max-1]
      uint64_t generator(uint64_t max = 101);

      uint64_t gen(checksum256& seed, uint64_t max = 101) const;

      checksum256 get_sys_seed() const;
      checksum256 get_user_seed() const;
      checksum256 get_mixed() const;
      checksum256 get_seed() const;
    private:
      checksum256 _sseed;
      checksum256 _useed;
      checksum256 _mixed;
      checksum256 _seed;
  };

  template<class T>
  checksum256 random::create_sys_seed(T mixed) const {
    ;
    data<T> mixed_block(mixed);
    const char *mixed_char = reinterpret_cast<const char *>(&mixed_block);
    checksum256 result = sha256((char *)mixed_char, sizeof(mixed_block));
    return result;
  }

  void random::seed(checksum256 sseed, checksum256 useed) {
    _sseed = sseed;
    _useed = useed;
    mixseed(_sseed, _useed, _mixed);
    _seed  = _mixed;
  }

  void random::mixseed(checksum256& sseed, checksum256& useed, checksum256& result) const {
    st_seeds seeds;
    seeds.seed1 = sseed;
    seeds.seed2 = useed;
    result = sha256( (char *)&seeds.seed1, sizeof(seeds.seed1) * 2);
  }

  uint64_t random::generator(uint64_t max) {
    mixseed(_mixed, _seed, _seed);
    uint64_t r = gen(_seed, max);
    return r;
  }

  uint64_t random::gen(checksum256& seed, uint64_t max) const {
    if (max <= 0) {
        return 0;
    }
    const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&seed);
    uint64_t r = p64[1] % max;
    return r;
  }

  checksum256 random::get_sys_seed() const {
    return _sseed;   
  }

  checksum256 random::get_user_seed() const {
    return _useed;
  }

  checksum256 random::get_mixed() const {
    return _mixed;
  }

  checksum256 random::get_seed() const {
    return _seed;
  }
}
/********************************* bet *********************************/

CONTRACT dice : public eosio::contract {
  public:
    using contract::contract;

    const uint64_t to_bonus_bucket_interval = 1*3600*uint64_t(1000000);
    const uint64_t luck_draw_interval = 1*3600*uint64_t(1000000);

    static constexpr eosio::name   active_permission{"active"_n};
    static constexpr eosio::name   TOKEN_CONTRACT{"act.token"_n};
    static constexpr eosio::name   stake_account{"act.stake"_n};    
    static constexpr eosio::name   GAME_TOKEN_CONTRACT{"actlucktoken"_n};
    static constexpr eosio::name   _game_token{"actlucktoken"_n};
    static constexpr eosio::name   TEAM_ACCOUNT {"luckyaddress"_n};
    static constexpr eosio::name   DIVI_ACCOUNT {"actwinbonus1"_n};
    static constexpr eosio::name   JACKPOT_ACCOUNT {"winjackpot11"_n};
    static constexpr eosio::name   transfer_action{"transfer"_n};
    
    static constexpr eosio::symbol ACT_SYMBOL = symbol(symbol_code("ACT"), 4);
    static constexpr eosio::symbol GAME_SYMBOL = symbol(symbol_code("LUCKY"), 4); 
    
    eosio::name         _code;
    eoswin::random      _random;
    checksum256         _seed;

    //@abi table activebets
    //@abi table highbets
    //@abi table largebets
    TABLE bet
    {
      uint64_t               id;
      uint64_t               bet_id;
      eosio::name            contract;
      eosio::name            bettor;
      eosio::name            inviter;
      uint64_t               bet_amt;
      vector<eosio::asset>   payout;
      uint8_t                roll_type;
      uint64_t               roll_border;
      uint64_t               roll_value;
      checksum256       seed;
      eosio::time_point_sec  time;
      uint64_t primary_key() const { return id; };
    };
    typedef eosio::multi_index<"activebets"_n, bet> bet_index;
    bet_index _bets;

    typedef eosio::multi_index<"highbets"_n, bet> high_odds_index;
    high_odds_index _high_odds_bets;

    typedef eosio::multi_index<"largebets"_n, bet> large_eos_index;
    large_eos_index _large_act_bets;
    
    //@abi table globalvars
    TABLE globalvar
    {
      uint64_t id;
      uint64_t val;
      uint64_t primary_key() const { return id; };
    };
    typedef eosio::multi_index<"globalvars"_n, globalvar> global_index;
    global_index _globals;

    //@abi table tradetokens
    TABLE tradetoken
    {
      eosio::symbol name;
      eosio::name   contract;
      uint64_t in;  
      uint64_t out; 
      uint64_t protect; 
      uint64_t times; 
      uint64_t divi_time; 
      uint64_t divi_balance; 
      uint64_t min_bet; 
      uint64_t large_bet; 
      uint64_t primary_key() const { return name.raw(); };
    };
    typedef eosio::multi_index<"tradetokens"_n, tradetoken> _tradetoken_index;
    _tradetoken_index _trades;

    TABLE assetitem
    {
      string   symbol;
      uint64_t accu_in;
      uint64_t accu_out;
    };

    //@abi table players
    //@abi table ranks
    //@abi table dailys
    TABLE player
    {
      eosio::name account;
      eosio::time_point_sec last_bettime;
      eosio::asset last_betin;
      eosio::asset last_payout;
      vector<assetitem> asset_items;
      uint64_t primary_key() const { return account.value; };
      double byeosin() const {
        for(auto it = asset_items.begin(); it != asset_items.end(); ++it) {
          if (it->symbol == "ACT") {
            return -it->accu_in;
          }
        }
        return 0;
      }
    };

    typedef eosio::multi_index<"players"_n, player> _player_index;
    _player_index _playerlists;

    typedef eosio::multi_index<"ranks"_n, player> _rank_index;
    _rank_index _ranklists;

    typedef eosio::multi_index<"dailys"_n, player> _daily_index;
    _daily_index _dailylists;

    ///@abi table luckers i64
    TABLE lucker {
      eosio::name account;
      uint64_t    last_bet_time_sec;  
      uint64_t    draw_time;
      uint64_t    roll_value;

      uint64_t primary_key() const {return account.value;}
      //EOSLIB_SERIALIZE(lucker, (account)(last_bet_time_sec)(draw_time)(roll_value))
    };
    typedef eosio::multi_index<"luckers"_n, lucker> luckers_table;
    luckers_table _luckers;

    //@abi table luckys
    TABLE luckyreward{
      uint64_t id;
      uint64_t number;
      eosio::asset reward;
      uint64_t primary_key() const { return id; };
    };
    typedef eosio::multi_index<"luckys"_n, luckyreward> _luckyreward_index;
    _luckyreward_index _luckyrewards;


    dice( eosio::name s, eosio::name code, datastream<const char*> ds );
    ~dice() {}

    /// @abi action
    ACTION setactive(bool active);

    /// @abi action
    ACTION init();

    void init_all_trade();

    void init_trade_token(eosio::symbol sym, eosio::name contract);

    /// @abi action
    ACTION setluckrwd(uint64_t id, uint64_t number, eosio::asset reward);

    /// @abi action
    ACTION setglobal(uint64_t id, uint64_t value);

    void to_bonus_bucket(eosio::symbol sym_name);

    void rewardlucky(name bettor, eosio::name inviter, eosio::asset bet_asset);
    
    void transfer(eosio::name from, eosio::name to, eosio::asset quantity, string memo);

    int64_t get_bet_reward(uint8_t roll_type, uint64_t roll_border, int64_t amount);

    uint64_t get_random(uint64_t max);
    
    uint64_t get_bet_nums();


    bool is_lucknum_open();

    /// @abi action
    ACTION start(name bettor, eosio::asset bet_asset, 
                   uint8_t roll_type, uint64_t roll_border, eosio::name inviter);

    /// @abi action
    ACTION resolved(eosio::name bettor, eosio::asset bet_asset, 
                   uint8_t roll_type, uint64_t roll_border, eosio::name inviter);

    /// @abi action
    ACTION receipt(uint64_t bet_id, eosio::name bettor, eosio::asset bet_amt, vector<eosio::asset> payout_list, 
                    checksum256 seed, uint8_t roll_type, uint64_t roll_border, uint64_t roll_value);

    void to_jackpot(eosio::name bettor, eosio::asset bet_asset);

    string symbol_to_string(uint64_t v) const;

    void init_player(player &a, eosio::name bettor, eosio::asset betin, eosio::asset payout, eosio::time_point_sec cur_time);

    void clear_player_accu(player &a);
    
    void save_player_list(eosio::name bettor, eosio::asset betin, eosio::asset payout, uint32_t now);

    void save_rank_list(eosio::name bettor, eosio::asset betin, eosio::asset payout, uint32_t now);

    void save_daily_list(eosio::name bettor, eosio::asset betin, eosio::asset payout, uint32_t now);

    void init_bet(bet& a, uint64_t id, uint64_t bet_id, eosio::name contract, eosio::name bettor, eosio::name inviter,
                  uint64_t bet_amt, vector<eosio::asset> payout, uint8_t roll_type, uint64_t roll_border,
                  uint64_t roll_value, checksum256 seed, eosio::time_point_sec time);

    void save_bet(uint64_t bet_id, eosio::name bettor, eosio::name inviter, 
                  eosio::asset bet_quantity, vector<eosio::asset> payout_list,
                  uint8_t roll_type, uint64_t roll_border, uint64_t roll_value,
                  checksum256 seed, eosio::time_point_sec time);

    void save_highodds_bet(uint64_t bet_id, eosio::name bettor, eosio::name inviter, 
                  eosio::asset bet_quantity, vector<eosio::asset> payout_list, 
                  uint8_t roll_type, uint64_t roll_border, uint64_t roll_value, 
                  checksum256 seed, eosio::time_point_sec time);

    void save_large_bet(uint64_t bet_id, eosio::name bettor, eosio::name inviter, 
                  eosio::asset bet_quantity, vector<eosio::asset> payout_list, 
                  uint8_t roll_type, uint64_t roll_border, uint64_t roll_value,
                  checksum256 seed, eosio::time_point_sec time);

    /// @abi action
    ACTION verify(checksum256 seed);

    void check_symbol(eosio::asset quantity);

    void check_symbol_code(const eosio::asset& quantity);

    /// @abi action
    ACTION setriskline(eosio::asset quantity);

    /// @abi action
    ACTION setdivi(eosio::asset quantity);

    /// @abi action
    ACTION setminbet(eosio::asset quantity);

    /// @abi action
    ACTION luck(eosio::name actor, uint64_t sub);

    /// @abi action
    ACTION lucking(eosio::name actor);

    /// @abi action
    ACTION lucked(eosio::name actor);

    eosio::asset get_luck_reward(uint64_t roll_number);

    /// @abi action
    ACTION luckreceipt(eosio::name name, checksum256 seed, uint64_t roll_value, vector<eosio::asset> rewards, uint64_t draw_time);

    /// @abi action
    ACTION luckverify(checksum256 seed);
};
