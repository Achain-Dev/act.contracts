#include "dice.hpp"
#include <act.token.hpp>    

#include <tuple>
using namespace eosio;
using namespace std;
/********************************* bet *********************************/
dice::dice(eosio::name self, eosio::name code, datastream<const char*> ds )
                    : eosio::contract(self, code, ds),
                     _code(code),
                     _bets(self, self.value),
                     _globals(self, self.value),
                     _high_odds_bets(self, self.value),
                     _large_act_bets(self, self.value),
                     _trades(self, get_self().value),
                     _playerlists(self, get_self().value),
                     _ranklists(self, get_self().value),
                     _dailylists(self, get_self().value),
                     _luckers(self, get_self().value),
                     _luckyrewards(self, get_self().value) {
  
}
/// @abi action
ACTION dice::setactive(bool active) {
      require_auth(get_self());

      auto pos = _globals.find(GLOBAL_ID_ACTIVE);
      if (pos == _globals.end()) {
        _globals.emplace(get_self(), [&](auto& a) {
          a.id = GLOBAL_ID_ACTIVE;
          a.val = active;
        });
      } else {
        _globals.modify(pos, same_payer, [&](auto& a) {
          a.val = active;
        });
      }
}

/// @abi action
ACTION dice::init(){
  require_auth(get_self());

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_BET;
    a.val = 0;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_HISTORY_INDEX;
    a.val = 0;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_LARGE_HISTORY_INDEX;
    a.val = 0;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_HIGH_ODDS_HISTORY_INDEX;
    a.val = 0;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_STATIC_TOTAL;
    a.val = 0;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_STATIC_DAILY;
    a.val = 0;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_MIN_JACKPOT_BET;
    a.val = 2500;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_JACKPOT_ACTIVE;
    a.val = 1;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_DIVIDEND_PERCENT;
    a.val = 25;
  });

  _globals.emplace(get_self(), [&](auto &a) {
    a.id = GLOBAL_ID_MAX_BET_PER;
    a.val = 100;
  });

  init_all_trade();
}

void dice::init_all_trade() {
  init_trade_token(ACT_SYMBOL, TOKEN_CONTRACT);
  init_trade_token(GAME_SYMBOL, GAME_TOKEN_CONTRACT);
}

void dice::init_trade_token(eosio::symbol sym, eosio::name contract)
{
  _trades.emplace(get_self(), [&](auto &a) {
    a.name = sym;
    a.contract = contract;
    a.in = 0;
    a.out = 0;
    a.protect = 0;
    a.times = 0;
    a.divi_time = current_time();
    a.divi_balance = 0;
  });
}


/// @abi action
ACTION dice::setluckrwd(uint64_t id, uint64_t number, eosio::asset reward) {
  require_auth(get_self());
  auto iter = _luckyrewards.find(id);
  if (iter == _luckyrewards.end()) {
    _luckyrewards.emplace(get_self(), [&](auto &a) {
      a.id = id;
      a.number = number;
      a.reward = reward;
    });
  } else {
    _luckyrewards.modify(iter, same_payer, [&](auto &a) {
      a.number = number;
      a.reward = reward;
    });
  }
}

/// @abi action
ACTION dice::setglobal(uint64_t id, uint64_t value) {
  require_auth(get_self());
  auto iter = _globals.find(id);
  if (iter == _globals.end()) {
    _globals.emplace(get_self(), [&](auto &a) {
      a.id = id;
      a.val = value;
    });
  } else {
    _globals.modify(iter, same_payer, [&](auto &a) {
      a.val = value;
    });
  }
}

void dice::to_bonus_bucket(eosio::symbol sym_name) {
  //eosio::symbol sym_name = eosio::symbol(sym).name();
  auto trade_iter = _trades.find(sym_name.raw());
  
  auto divi_time = trade_iter->divi_time;
  auto ct = current_time();

  if (ct < (divi_time + to_bonus_bucket_interval)) {
    return;
  }
  
  uint64_t last_divi_balance = trade_iter->divi_balance;
  eosio::name bet_token(trade_iter->contract);
  auto balance = eosio::token::get_balance(bet_token,get_self(), sym_name.code());
  uint64_t dividends = 0;

  if (last_divi_balance < balance.amount) {
    auto iter = _globals.find(GLOBAL_ID_DIVIDEND_PERCENT);
    
    uint64_t dividend_pecent = iter->val;
    if (dividend_pecent >= 75) {
      dividend_pecent = 75;
    }
    dividends = (balance.amount - last_divi_balance) * dividend_pecent / 100;
  }

  if (dividends <= 10000) {
    return;
  }

  _trades.modify(trade_iter, same_payer, [&](auto &a) {
    a.divi_balance = balance.amount - dividends;
    a.divi_time = ct;
  });
  //todo
  //INLINE_ACTION_SENDER(eosio::token, transfer)(trade_iter->contract, {get_self(),active_permission}, {get_self(), DIVI_ACCOUNT, eosio::asset(dividends, sym_name), "To ACT.Win Bonus Pool [https://act.win/dice]"} );
}

void dice::rewardlucky(name bettor, eosio::name inviter, eosio::asset bet_asset) {
  require_auth(get_self());
  auto pos = _globals.find(GLOBAL_ID_TOKEN_MULTIPLE);
  uint64_t multiple = 1;
  if (pos != _globals.end()) {
    multiple = pos->val;
  }

  auto num = bet_asset.amount / 2 * multiple;
  num = bet_asset.amount % 2 == 0 ? num : num + 1;
  if (num <= 0) {
    return;
  }
  auto reward = eosio::asset(num, GAME_SYMBOL);

  auto balance = eosio::token::get_balance(_game_token,GAME_TOKEN_CONTRACT, GAME_SYMBOL.code());
  reward = reward > balance ? balance : reward;

  auto to_better = reward * BETTOR_TOKEN_FEE / 10;
  auto to_team   = reward - to_better;
  if (inviter != TEAM_ACCOUNT) {
    auto to_inviter = reward * INVITER_TOKEN_FEE / 10;
    if (to_inviter.amount > 0) {
      to_team.set_amount(to_team.amount - to_inviter.amount);

      if (is_account(inviter)) {
        INLINE_ACTION_SENDER(eosio::token, transfer)(GAME_TOKEN_CONTRACT, {GAME_TOKEN_CONTRACT,active_permission}, {GAME_TOKEN_CONTRACT, inviter, to_inviter, "LUCKY token for inviter [https://eos.win]"} );
      }
    }
  }

  if (to_better.amount > 0) {
    INLINE_ACTION_SENDER(eosio::token, transfer)(GAME_TOKEN_CONTRACT, {GAME_TOKEN_CONTRACT,active_permission}, {GAME_TOKEN_CONTRACT, bettor, to_better, "LUCKY token for player [https://eos.win]"} );
  }

  if (to_team.amount > 0) {
    INLINE_ACTION_SENDER(eosio::token, transfer)(GAME_TOKEN_CONTRACT, {GAME_TOKEN_CONTRACT,active_permission}, {GAME_TOKEN_CONTRACT, TEAM_ACCOUNT, to_team, "LUCKY token for team [https://eos.win]"} );
  }
}


vector<string> dice::split(const string& str, const string& delim) {
    vector<string> tokens;
    size_t prev = 0, pos = 0;

    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

void dice::transfer(eosio::name from, eosio::name to, eosio::asset quantity, string memo) {
  if (from == get_self() || to != get_self()) { return;  }

  if (from == stake_account || from == "tptvotepools"_n) { return; }

  auto tx_size = transaction_size();
  char tx[tx_size];
  auto read_size = read_transaction(tx, tx_size);
  check( tx_size == read_size, "read_transaction failed");
  auto trx = eosio::unpack<eosio::transaction>( tx, read_size );
  eosio::action first_action = trx.actions.front();
  check(first_action.name == transfer_action && first_action.account == _code, "wrong transaction");

  check_symbol_code(quantity);
  check(quantity.is_valid(), "Invalid transfer amount.");
  
  int64_t amt = quantity.amount;
  check(amt > 0, "Transfer amount not positive");
  
  eosio::symbol sym_name = quantity.symbol;
  auto trade_iter = _trades.find(sym_name.raw());
  eosio::name bet_token(trade_iter->contract);
  eosio::asset balance = eosio::token::get_balance(bet_token, get_self(), sym_name.code());

  if (memo == "deposit") {
    if (trade_iter != _trades.end()) {
        _trades.modify(trade_iter, same_payer, [&](auto& info) {
            info.divi_balance += quantity.amount;
        });
    }
  } else {
    auto active_pos = _globals.find(GLOBAL_ID_ACTIVE);
    check(active_pos != _globals.end() && active_pos->val, "Maintaining ...");

    int64_t max = (balance.amount * SINGLE_BET_MAX_PERCENT / 100);
    check(amt <= max, "Bet amount exceeds max amount.");

    auto trade_iter = _trades.find(sym_name.raw());
    check(balance.amount >= trade_iter->protect, "Game under maintain, stay tuned.");
    
    check(memo.empty() == false, "Memo is for dice info, cannot be empty.");

    auto pieces = split(memo, ",");

    check(pieces[0].empty() == false, "Roll type cannot be empty!");
    check(pieces[1].empty() == false, "Roll prediction cannot be empty!");

    uint8_t roll_type = atoi( pieces[0].c_str() );
    uint64_t roll_border = atoi( pieces[1].c_str() );

    name inviter;
    if (pieces[2].empty() == true) {
      inviter = TEAM_ACCOUNT;
    } else {
      inviter = eosio::name(pieces[2].c_str());
    }
    check(from != inviter, "Inviter can't be self");

    auto max_bet_percent_pos = _globals.find(GLOBAL_ID_MAX_BET_PER);
    uint64_t max_bet_percent = max_bet_percent_pos->val;

    int64_t max_reward = get_bet_reward(roll_type, roll_border, amt);
    float max_bet_token = (amt * (balance.amount / max_bet_percent) / max_reward) / 10000.0;
    float min_bet_token = (trade_iter->min_bet) / 10000.0;
    char str[128];
    sprintf(str, "Bet amount must between %f and %f", min_bet_token, max_bet_token);
    check(amt >= trade_iter->min_bet && max_reward <= (balance.amount / max_bet_percent), str);

    check(roll_border >= ROLL_BORDER_MIN && roll_border <= ROLL_BORDER_MAX, "Bet border must between 2 to 97");

    eosio::transaction r_out;
    auto t_data = make_tuple(from, quantity, roll_type, roll_border, inviter);
    r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "start"_n, t_data);
    r_out.delay_sec = 1;
    r_out.send(from.value, get_self());
  }
}

int64_t dice::get_bet_reward(uint8_t roll_type, uint64_t roll_border, int64_t amount) {
  uint8_t fit_num;
  if (roll_type == ROLL_TYPE_SMALL)
  {
    fit_num = roll_border;
  }
  else if (roll_type == ROLL_TYPE_BIG)
  {
    fit_num = BET_MAX_NUM - 1 - roll_border;
  }

  int64_t reward_amt = amount * (100 - FEE) / fit_num;

  return reward_amt;
}

uint64_t dice::get_random(uint64_t max) {
  // auto g = _globals.get(GLOBAL_ID_BET, "global is missing");
  auto sseed = _random.create_sys_seed(0);

  auto s = read_transaction(nullptr, 0);
  char *tx = (char *)malloc(s);
  read_transaction(tx, s);
  checksum256 txid = sha256(tx, s); //todo
  printhex(&txid, sizeof(txid));

  _random.seed(sseed, txid);
  uint64_t roll_value = _random.generator(max);
  _seed = _random.get_seed();

  return roll_value;
}

uint64_t dice::get_bet_nums() {
  uint64_t total = 0;
  for(auto iter = _trades.begin(); iter != _trades.end(); ) 
  {
    total += iter->times;
    iter++;
  }
  return total;
}


bool dice::is_lucknum_open() {
  auto start_itr = _globals.find(GLOBAL_ID_LUCKNUM_START_TIME);
  uint64_t start_time = 0;
  if (start_itr != _globals.end()) {
    start_time = start_itr->val;
  }

  auto stop_itr = _globals.find(GLOBAL_ID_LUCKNUM_STOP_TIME);
  uint64_t stop_time = 0;
  if (stop_itr != _globals.end()) {
    stop_time = stop_itr->val;
  }

  eosio::time_point_sec cur_time = eosio::time_point_sec( now() );

  if (eosio::time_point_sec(start_time) <= cur_time &&  cur_time <= eosio::time_point_sec(stop_time)) {
    return true;
  }

  uint64_t total_bet_nums = get_bet_nums();
  auto start_betnums_itr = _globals.find(GLOBAL_ID_LUCKNUM_START_BETNUMS);
  uint64_t start_betnums = 0;
  if (start_betnums_itr != _globals.end()) {
    start_betnums = start_betnums_itr->val;
  }

  auto stop_betnums_itr = _globals.find(GLOBAL_ID_LUCKNUM_STOP_BETNUMS);
  uint64_t stop_betnums = 0;
  if (stop_betnums_itr != _globals.end()) {
    stop_betnums = stop_betnums_itr->val;
  }
  
  if (start_betnums <= total_bet_nums && total_bet_nums <= stop_betnums) {
    return true;
  }
  return false;
}

/// @abi action
ACTION dice::start(name bettor, eosio::asset bet_asset, uint8_t roll_type, uint64_t roll_border, eosio::name inviter){
  require_auth(get_self());
  eosio::transaction r_out;
  auto t_data = make_tuple(bettor, bet_asset, roll_type, roll_border, inviter);
  r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "resolved"_n, t_data);
  r_out.delay_sec = 0;
  r_out.send(bettor.value, get_self());
}

/// @abi action
ACTION dice::resolved(eosio::name bettor, eosio::asset bet_asset, 
               uint8_t roll_type, uint64_t roll_border, eosio::name inviter)
{
  require_auth(get_self());
  eosio::symbol sym_name = bet_asset.symbol;
  auto trade_iter = _trades.find(sym_name.raw());

  uint64_t roll_value = get_random(BET_MAX_NUM);
  auto global_itr = _globals.find(GLOBAL_ID_BET);
  check(global_itr != _globals.end(), "Unknown global id");
  uint64_t last_bet_id = global_itr->val;
  uint64_t cur_bet_id = last_bet_id + 1;

  _globals.modify(global_itr, same_payer, [&](auto& a) {
    a.val = cur_bet_id;
  });

  vector<eosio::asset> payout_list;
  eosio::asset payout;

  uint32_t _now = now();

  bool is_win = (roll_type == ROLL_TYPE_SMALL && roll_value < roll_border) || (roll_type == ROLL_TYPE_BIG && roll_value > roll_border);
  if ( is_win )
  {
    int64_t reward_amt = get_bet_reward(roll_type, roll_border, bet_asset.amount);
    payout = eosio::asset(reward_amt, bet_asset.symbol);

    _trades.modify(trade_iter, same_payer, [&](auto& a) {
      a.out += reward_amt;
    });
  }
  else
  {
    payout = eosio::asset(0, bet_asset.symbol);
  }
  
  payout_list.push_back(payout);
  
  to_jackpot(bettor, bet_asset);
  if (bet_asset.symbol == ACT_SYMBOL && is_lucknum_open())
  {
    for(auto lucky_iter = _luckyrewards.begin(); lucky_iter != _luckyrewards.end(); ) 
    {
      if ( (roll_value == lucky_iter->number) && lucky_iter->reward.amount > 0) 
      {
        eosio::symbol lucky_sym_name = lucky_iter->reward.symbol;
        auto lucky_trade_iter = _trades.find(lucky_sym_name.raw());
        
        char str[128];
        sprintf(str, "Hit magic number! You got extra bonus! https://eos.win");
        eosio::print(string(str).c_str());
        //todo
        //INLINE_ACTION_SENDER(eosio::token, transfer)(lucky_trade_iter->contract, {get_self(), active_permission}, {get_self(), bettor, lucky_iter->reward, string(str)} );
        
        payout_list.push_back(lucky_iter->reward);
      }
      lucky_iter++;
    }
  }

  if (bet_asset.symbol == ACT_SYMBOL) {
    auto lucker_pos = _luckers.find(bettor.value);
    if (lucker_pos == _luckers.end()) {
      lucker_pos = _luckers.emplace(get_self(), [&](auto& info) {
        info.account = bettor;
        info.last_bet_time_sec = _now;
        info.draw_time = 0;
        info.roll_value   = 0;
      });
    } else {
      _luckers.modify(lucker_pos, same_payer, [&](auto& info) {
        info.last_bet_time_sec = _now;
      });
    }
  }

  eosio::time_point_sec time = eosio::time_point_sec( _now );
  
  save_rank_list(bettor, bet_asset, payout, _now);
  save_daily_list(bettor, bet_asset, payout, _now);

  save_bet(cur_bet_id, bettor, inviter, bet_asset, payout_list, roll_type, roll_border, roll_value, _seed, time);
  save_highodds_bet(cur_bet_id, bettor, inviter, bet_asset, payout_list, roll_type, roll_border, roll_value, _seed, time);
  save_large_bet(cur_bet_id, bettor, inviter, bet_asset, payout_list, roll_type, roll_border, roll_value, _seed, time);
  
  _trades.modify(trade_iter, same_payer, [&](auto& a) {
    a.in += bet_asset.amount;
    a.times += 1;
  });

  eosio::transaction r_out;
  
  if (inviter != TEAM_ACCOUNT && is_account(inviter)) {
    eosio::asset inviter_reward = eosio::asset(bet_asset.amount * INVITE_BONUS, bet_asset.symbol);

    char str[128];
    sprintf(str, "Referral reward from ACT.Win! Player: %s, Bet ID: %lld", eosio::name{bettor}.to_string().c_str(), cur_bet_id);
    r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, 
                            trade_iter->contract, 
                            transfer_action, 
                            make_tuple(get_self(), inviter, inviter_reward, string(str)));
  }

  if (bet_asset.symbol == ACT_SYMBOL) {
    r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "rewardlucky"_n, make_tuple(bettor, inviter, bet_asset));
  }

  to_bonus_bucket(bet_asset.symbol);

  if ( is_win )
  {
    char str[128];
    sprintf(str, "Bet id: %lld. You win! Remember to claim your dividens with your LUCKY token! https://eos.win", cur_bet_id);
    // INLINE_ACTION_SENDER(eosio::token, transfer)(trade_iter->contract, {get_self(), active_permission}, {get_self(), bettor, payout, string(str)} );
  
    r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, 
                            trade_iter->contract, 
                            transfer_action, 
                            make_tuple(get_self(), bettor, payout, string(str)));
  }

  // INLINE_ACTION_SENDER(dice, receipt)(get_self(), {get_self(), active_permission}, {cur_bet_id, bettor, bet_asset, payout_list, _seed, roll_type, roll_border, roll_value});
  //todo
  //r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "receipt"_n, make_tuple(cur_bet_id, bettor, bet_asset, payout_list, _seed, roll_type, roll_border, roll_value));
  r_out.delay_sec = 0;
  r_out.send(bettor.value, get_self());
}

/// @abi action
ACTION dice::receipt(uint64_t bet_id, eosio::name bettor, eosio::asset bet_amt, vector<eosio::asset> payout_list, 
                checksum256 seed, uint8_t roll_type, uint64_t roll_border, uint64_t roll_value)
{
  require_auth(get_self());
  require_recipient( bettor );
}

void dice::to_jackpot(eosio::name bettor, eosio::asset bet_asset)
{
  auto pos = _globals.find(GLOBAL_ID_JACKPOT_ACTIVE);

  if (pos != _globals.end() && pos->val == 0) {
    return;
  }

  if (bet_asset.symbol == ACT_SYMBOL) {
    auto jackpot_itr = _globals.find(GLOBAL_ID_MIN_JACKPOT_BET);
    if (jackpot_itr != _globals.end() && bet_asset.amount >= jackpot_itr->val) {
      eosio::asset jackpot_reward = eosio::asset(bet_asset.amount * JACKPOT_BONUS, bet_asset.symbol);
      eosio::transaction r_out;
      auto t_data = make_tuple(get_self(), JACKPOT_ACCOUNT, jackpot_reward, bettor.to_string());
      r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, 
                      TOKEN_CONTRACT,
                      transfer_action, 
                      t_data);
      r_out.delay_sec = 1;
      r_out.send(bettor.value, get_self());
    }
  }
}

string dice::symbol_to_string(uint64_t v) const {
  v >>= 8;
  string result;
  while (v > 0) {
    char c = static_cast<char>(v & 0xFF);
    result += c;
    v >>= 8;
  }
  return result;
}

void dice::init_player(player &a, eosio::name bettor, eosio::asset betin, eosio::asset payout, eosio::time_point_sec cur_time) {
  string symbol = symbol_to_string(betin.symbol.raw());

  a.account = bettor;
  a.last_betin = betin;
  a.last_bettime = cur_time;
  a.last_payout = payout;

  uint8_t count = a.asset_items.size();
  bool finded = false;
  for (uint8_t i=0; i < count; i++)
  {
    if (a.asset_items[i].symbol == symbol)
    {
      a.asset_items[i].accu_in += betin.amount;
      a.asset_items[i].accu_out += payout.amount;
      finded = true;
    }
  }

  if (finded == false)
  {
    assetitem item;
    item.symbol = symbol;
    item.accu_in = betin.amount;
    item.accu_out = payout.amount;
    a.asset_items.push_back(item);
  }
}

void dice::clear_player_accu(player &a) {
  uint8_t count = a.asset_items.size();
  for (uint8_t i=0; i < count; i++)
  {
    a.asset_items[i].accu_in = 0;
    a.asset_items[i].accu_out = 0;
  }
}

void dice::save_player_list(eosio::name bettor, eosio::asset betin, eosio::asset payout, uint32_t now) {
  eosio::time_point_sec cur_time = eosio::time_point_sec( now );
  auto player_itr = _playerlists.find(bettor.value);

  string symbol = symbol_to_string(betin.symbol.raw());
  if (player_itr == _playerlists.end()) {
    _playerlists.emplace(get_self(), [&](auto &a) {
      init_player(a, bettor, betin, payout, cur_time);
    });
  } 
  else
  {
    _playerlists.modify(player_itr, same_payer, [&](auto& a) {
      init_player(a, bettor, betin, payout, cur_time);
    });
  }
}

void dice::save_rank_list(eosio::name bettor, eosio::asset betin, eosio::asset payout, uint32_t now) {
  auto start_itr = _globals.find(GLOBAL_ID_ACCU_START_TIME);
  auto start_time = 0;
  if (start_itr != _globals.end()) {
    start_time = start_itr->val;
  }

  auto stop_itr = _globals.find(GLOBAL_ID_ACCU_STOP_TIME);
  auto stop_time = 0;
  if (stop_itr != _globals.end()) {
    stop_time = stop_itr->val;
  }

  eosio::time_point_sec cur_time = eosio::time_point_sec( now );

  if (eosio::time_point_sec(start_time) > cur_time || cur_time > eosio::time_point_sec(stop_time)) {
    return;
  }
  auto player_itr = _ranklists.find(bettor.value);
  string symbol = symbol_to_string(betin.symbol.raw());
  if (player_itr == _ranklists.end()) {
    _ranklists.emplace(get_self(), [&](auto &a) {
      init_player(a, bettor, betin, payout, cur_time);
    });
  } 
  else
  {
    if (eosio::time_point_sec(start_time) > player_itr->last_bettime) {
      _ranklists.modify(player_itr, same_payer, [&](auto& a) {
        clear_player_accu(a);
      });
    }

    _ranklists.modify(player_itr, same_payer, [&](auto& a) {
      init_player(a, bettor, betin, payout, cur_time);
    });
  }
}

void dice::save_daily_list(eosio::name bettor, eosio::asset betin, eosio::asset payout, uint32_t now) {
  uint64_t cur_day = now / DAY_SECONDS;
  eosio::time_point_sec start_time = eosio::time_point_sec( cur_day * DAY_SECONDS );
  eosio::time_point_sec stop_time = eosio::time_point_sec( (cur_day + 1) * DAY_SECONDS);
  eosio::time_point_sec cur_time = eosio::time_point_sec( now );

  auto player_itr = _dailylists.find(bettor.value);
  string symbol = symbol_to_string(betin.symbol.raw());
  if (player_itr == _dailylists.end()) {
    _dailylists.emplace(get_self(), [&](auto &a) {
      init_player(a, bettor, betin, payout, cur_time);
    });
  } 
  else
  {
    if (eosio::time_point_sec(start_time) > player_itr->last_bettime) {
      _dailylists.modify(player_itr, same_payer, [&](auto& a) {
        clear_player_accu(a);
      });
    }

    _dailylists.modify(player_itr, same_payer, [&](auto& a) {
      init_player(a, bettor, betin, payout, cur_time);
    });
  }
}

void dice::init_bet(bet& a, uint64_t id, uint64_t bet_id, eosio::name contract, eosio::name bettor, eosio::name inviter,
              uint64_t bet_amt, vector<eosio::asset> payout, uint8_t roll_type, uint64_t roll_border,
              uint64_t roll_value, checksum256 seed, eosio::time_point_sec time) {
  a.id = id;
  a.bet_id = bet_id;
  a.contract = contract;
  a.bettor = bettor;
  a.inviter = inviter;
  
  a.bet_amt = bet_amt;
  a.payout = payout;
  a.roll_type = roll_type;
  a.roll_border = roll_border;
  a.roll_value = roll_value;
  a.seed = seed;
  a.time = time;
}

void dice::save_bet(uint64_t bet_id, eosio::name bettor, eosio::name inviter, 
                  eosio::asset bet_quantity, vector<eosio::asset> payout_list,
                  uint8_t roll_type, uint64_t roll_border, uint64_t roll_value,
                  checksum256 seed, eosio::time_point_sec time) {
  uint64_t bet_amt = bet_quantity.amount;
  eosio::symbol sym_name = bet_quantity.symbol;
  auto trade_iter = _trades.find(sym_name.raw());

  auto global_itr = _globals.find(GLOBAL_ID_HISTORY_INDEX);

  uint64_t history_index = global_itr->val % BET_HISTORY_LEN + 1;

  auto bet_itr = _bets.find(history_index);

  if (bet_itr != _bets.end())
  {
    // auto lambda_func = [&](auto& a) {};
    _bets.modify(bet_itr, same_payer, [&](auto& a) {
      init_bet(a, a.id, bet_id, trade_iter->contract, bettor, 
              inviter, bet_amt, payout_list, roll_type, 
              roll_border, roll_value, seed, time);
    });
  }
  else
  {
    _bets.emplace(get_self(), [&](auto &a) {
      init_bet(a, history_index, bet_id, trade_iter->contract, bettor, 
              inviter, bet_amt, payout_list, roll_type, 
              roll_border, roll_value, seed, time);
    });
  }

  _globals.modify(global_itr, same_payer, [&](auto& a) {
    a.val = history_index;
  });
}

void dice::save_highodds_bet(uint64_t bet_id, eosio::name bettor, eosio::name inviter, 
                  eosio::asset bet_quantity, vector<eosio::asset> payout_list, 
                  uint8_t roll_type, uint64_t roll_border, uint64_t roll_value, 
                  checksum256 seed, eosio::time_point_sec time) {
                
  if ((roll_type == ROLL_TYPE_SMALL && roll_value < roll_border && roll_border < WONDER_HIGH_ODDS)
    || (roll_type == ROLL_TYPE_BIG && roll_value > roll_border && roll_border > (BET_MAX_NUM - WONDER_HIGH_ODDS))) {
    uint64_t bet_amt = bet_quantity.amount;
    eosio::symbol sym_name = bet_quantity.symbol;
    auto trade_iter = _trades.find(sym_name.raw());

    auto global_itr = _globals.find(GLOBAL_ID_HIGH_ODDS_HISTORY_INDEX);

    uint64_t history_index = global_itr->val % BET_HISTORY_LEN + 1;

    auto bet_itr = _high_odds_bets.find(history_index);

    if (bet_itr != _high_odds_bets.end())
    {
      _high_odds_bets.modify(bet_itr, same_payer, [&](auto& a) {
        init_bet(a, a.id, bet_id, trade_iter->contract, bettor, 
              inviter, bet_amt, payout_list, roll_type, 
              roll_border, roll_value, seed, time);
      });
    }
    else
    {
      _high_odds_bets.emplace(get_self(), [&](auto &a) {
        init_bet(a, history_index, bet_id, trade_iter->contract, bettor, 
              inviter, bet_amt, payout_list, roll_type, 
              roll_border, roll_value, seed, time);
      });
    }

    _globals.modify(global_itr, same_payer, [&](auto& a) {
      a.val = history_index;
    });
  }
}

void dice::save_large_bet(uint64_t bet_id, eosio::name bettor, eosio::name inviter, eosio::asset bet_quantity, vector<eosio::asset> payout_list,
              uint8_t roll_type, uint64_t roll_border, uint64_t roll_value, checksum256 seed, eosio::time_point_sec time) {
  uint64_t bet_amt = bet_quantity.amount;
  eosio::symbol sym_name = bet_quantity.symbol;
  auto trade_iter = _trades.find(sym_name.raw());
  
  if (bet_amt >= trade_iter->large_bet)
  {
    auto global_itr = _globals.find(GLOBAL_ID_LARGE_HISTORY_INDEX);

    uint64_t history_index = global_itr->val % BET_HISTORY_LEN + 1;

    auto bet_itr = _large_act_bets.find(history_index);

    if (bet_itr != _large_act_bets.end())
    {
      _large_act_bets.modify(bet_itr, same_payer, [&](auto& a) {
        init_bet(a, a.id, bet_id, trade_iter->contract, bettor, 
              inviter, bet_amt, payout_list, roll_type, 
              roll_border, roll_value, seed, time);
      });
    }
    else
    {
      _large_act_bets.emplace(get_self(), [&](auto &a) {
        init_bet(a, history_index, bet_id, trade_iter->contract, bettor, 
              inviter, bet_amt, payout_list, roll_type, 
              roll_border, roll_value, seed, time);
      });
    }

    _globals.modify(global_itr, same_payer, [&](auto& a) {
      a.val = history_index;
    });
  }
}

/// @abi action
ACTION dice::verify(checksum256 seed) {
  uint64_t r = _random.gen(seed, BET_MAX_NUM);
  string str = string("Random value ") + to_string(r);
  check(false, str.c_str());
}

void dice::check_symbol(eosio::asset quantity) {
    check(quantity.symbol == ACT_SYMBOL, "Accept ACT only!");
}

void dice::check_symbol_code(const eosio::asset& quantity) {
    check((_code == TOKEN_CONTRACT && quantity.symbol == ACT_SYMBOL),"Token do not be supported, or symbol not match with the code!");
}

/// @abi action
ACTION dice::setriskline(eosio::asset quantity) {
    require_auth(get_self());
    check_symbol(quantity);
    eosio::symbol sym_name = quantity.symbol;
    auto trade_iter = _trades.find(sym_name.raw());
    if (trade_iter != _trades.end()) {
        _trades.modify(trade_iter, same_payer, [&](auto& info) {
            info.protect = quantity.amount;
        });
    }
}

/// @abi action
ACTION dice::setdivi(eosio::asset quantity) {
  require_auth(get_self());
  check_symbol(quantity);
  eosio::symbol sym_name = quantity.symbol;
  auto trade_iter = _trades.find(sym_name.raw());
  if (trade_iter != _trades.end()) {
      _trades.modify(trade_iter, same_payer, [&](auto& info) {
          info.divi_balance = quantity.amount;
      });
  }
}

/// @abi action
ACTION dice::setminbet(eosio::asset quantity) {
    require_auth(get_self());
    check_symbol(quantity);
    eosio::symbol sym_name = quantity.symbol;
    auto trade_iter = _trades.find(sym_name.raw());
    if (trade_iter != _trades.end()) {
        _trades.modify(trade_iter, same_payer, [&](auto& info) {
            info.min_bet = quantity.amount;
            info.large_bet = 10 * info.min_bet;
        });
    }
}

/// @abi action
ACTION dice::luck(eosio::name actor, uint64_t sub) {
  require_auth(actor);

  auto g_pos = _globals.find(GLOBAL_ID_LUCK_DRAW_ACTIVE);
  check(g_pos->val > 0, "luck is not actived");

  auto ct = current_time();

  auto nt = ct/1e6;
  auto st = sub ^ 217824523;
  auto conds = (st -nt < 300) || (nt -st < 300) ;
  check(conds, "wrong sub!");

  auto luck_pos = _luckers.find(actor.value);
  if (luck_pos == _luckers.end()) {
    luck_pos = _luckers.emplace(get_self(), [&](auto& info) {
      info.account = actor;
      info.last_bet_time_sec = 0;
      info.draw_time = 0;
      info.roll_value   = 0;
    });
  }
  check(luck_pos->last_bet_time_sec + HOUR_SECONDS > nt, "do not be active player"); 
  check(luck_pos->draw_time + luck_draw_interval < ct, "draw time has not cool down yet");

  eosio::transaction r_out;
  r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "lucking"_n, actor);
  r_out.delay_sec = 1;
  r_out.send(actor.value, get_self());
}

/// @abi action
ACTION dice::lucking(eosio::name actor) {
  require_auth(get_self());
  eosio::transaction r_out;
  r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "lucked"_n, actor);
  r_out.delay_sec = 0;
  r_out.send(actor.value, get_self());
}

/// @abi action
ACTION dice::lucked(eosio::name actor) {
  require_auth(get_self());

  auto ct = current_time();

  uint64_t roll_value = get_random(LUCK_DRAW_MAX);
 
  vector<eosio::asset> rewards;
  eosio::asset reward = get_luck_reward(roll_value);
  rewards.push_back(reward);

  auto total_pos = _globals.find(GLOBAL_ID_LUCK_DRAW_TOTAL);
  if (total_pos == _globals.end()) {
    total_pos = _globals.emplace(get_self(), [&](auto& info) {
      info.id = GLOBAL_ID_LUCK_DRAW_TOTAL;
      info.val = reward.amount;
    });
  } else {
    _globals.modify(total_pos, same_payer, [&](auto& info) {
      info.val += reward.amount;
    });
  }

  auto luck_pos = _luckers.find(actor.value);
  _luckers.modify(luck_pos, same_payer, [&](auto& info) {
    info.draw_time = ct;
    info.roll_value = roll_value;
  });

  eosio::transaction r_out;
  r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, 
                    TOKEN_CONTRACT, 
                    transfer_action, 
                    make_tuple(get_self(), actor, reward, "ACT.Win lucky draw rewards!"));

  if (roll_value == 10000) {
    auto lucky_reward = eosio::asset(6660000, GAME_SYMBOL);
    rewards.push_back(lucky_reward);
    r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, 
            GAME_TOKEN_CONTRACT, 
            transfer_action, 
            make_tuple(get_self(), actor, lucky_reward, "ACT.Win lucky draw rewards!"));

    auto lucky_pos = _globals.find(GLOBAL_ID_LUCK_DRAW_LUCKY);
    if (lucky_pos == _globals.end()) {
      lucky_pos = _globals.emplace(get_self(), [&](auto& info) {
        info.id = GLOBAL_ID_LUCK_DRAW_LUCKY;
        info.val = lucky_reward.amount;
      });
    } else {
      _globals.modify(lucky_pos, same_payer, [&](auto& info) {
        info.val += lucky_reward.amount;
      });
    }
  }

  r_out.actions.emplace_back(eosio::permission_level{get_self(), active_permission}, get_self(), "luckreceipt"_n, make_tuple(actor, _seed, roll_value, rewards, luck_pos->draw_time));
  r_out.delay_sec = 0;
  r_out.send(actor.value, get_self());
}

eosio::asset dice::get_luck_reward(uint64_t roll_number) {
  if (roll_number <= 9885) {
    return eosio::asset(6, ACT_SYMBOL);
  } else if (9886 <= roll_number && roll_number <= 9985) {
    return eosio::asset(60, ACT_SYMBOL);
  } else if (9986 <= roll_number && roll_number <= 9993) {
    return eosio::asset(600, ACT_SYMBOL);
  } else if (9994 <= roll_number && roll_number <= 9997) {
    return eosio::asset(6000, ACT_SYMBOL);
  } else if (9998 <= roll_number && roll_number <= 9999) {
    return eosio::asset(60000, ACT_SYMBOL);
  } else if (roll_number == 10000) {
    return eosio::asset(160000, ACT_SYMBOL);
  } else {
    return eosio::asset(0, ACT_SYMBOL);
  }
}

/// @abi action
ACTION dice::luckreceipt(eosio::name name, checksum256 seed, uint64_t roll_value, vector<eosio::asset> rewards, uint64_t draw_time) {
  require_auth(get_self());
  require_recipient(name);
}

/// @abi action
ACTION dice::luckverify(checksum256 seed) {
  uint64_t r = _random.gen(seed, LUCK_DRAW_MAX);
  string str = string("Random value ") + to_string(r);
  check(false, str.c_str());
}

extern "C" {
   void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (code == "act"_n.value && action == "onerror"_n.value) {
        check(code == "act"_n.value, "onerror action's are only valid from the ACT system account");
    }
    if (( code == "act.token"_n.value || code == "actlucktoken"_n.value ) && (action == "transfer"_n.value)) { 
        execute_action(name(receiver),name(code), &dice::transfer);
        return;
    }
    if (code != receiver) return; 
    switch (action) { 
        EOSIO_DISPATCH_HELPER(dice, (init)(setactive)(setglobal)(setluckrwd)(setriskline)(setdivi)(setminbet)(receipt)(rewardlucky)(verify)(start)(resolved)(luck)(lucking)(lucked)(luckreceipt)(luckverify)) 
    }; 
  } 
}


