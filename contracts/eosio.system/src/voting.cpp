#include <eosio/crypto.hpp>
#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/privileged.hpp>
#include <eosio/serialize.hpp>
#include <eosio/singleton.hpp>

#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

#include <algorithm>
#include <cmath>

namespace eosiosystem {

   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::indexed_by;
   using eosio::microseconds;
   using eosio::singleton;

   void system_contract::regproducer( const name& producer, const eosio::public_key& producer_key, const std::string& url, uint16_t location ) {
      check( url.size() < 512, "url too long" );
      check( producer_key != eosio::public_key(), "public key should not be the default value" );
      require_auth( producer );

      auto prod = _producers.find( producer.value );
      const auto ct = current_time_point();

      if ( prod != _producers.end() ) {
         _producers.modify( prod, producer, [&]( producer_info& info ){
            info.producer_key = producer_key;
            info.is_active    = true;
            info.url          = url;
            info.location     = location;
            if ( info.last_claim_time == time_point() )
               info.last_claim_time = ct;
         });

         auto prod2 = _producers2.find( producer.value );
         if ( prod2 == _producers2.end() ) {
            _producers2.emplace( producer, [&]( producer_info2& info ){
               info.owner                     = producer;
               info.last_votepay_share_update = ct;
            });
            update_total_votepay_share( ct, 0.0, prod->total_votes );
            // When introducing the producer2 table row for the first time, the producer's votes must also be accounted for in the global total_producer_votepay_share at the same time.
         }
      } else {
         _producers.emplace( producer, [&]( producer_info& info ){
            info.owner           = producer;
            info.total_votes     = 0;
            info.producer_key    = producer_key;
            info.is_active       = true;
            info.url             = url;
            info.location        = location;
            info.last_claim_time = ct;
         });
         _producers2.emplace( producer, [&]( producer_info2& info ){
            info.owner                     = producer;
            info.last_votepay_share_update = ct;
         });
      }

   }

   void system_contract::unregprod( const name& producer ) {
      require_auth( producer );

      const auto& prod = _producers.get( producer.value, "producer not found" );
      _producers.modify( prod, same_payer, [&]( producer_info& info ){
         info.deactivate();
      });
   }

   void system_contract::update_elected_producers( const block_timestamp& block_time ) {
      _gstate.last_producer_schedule_update = block_time;

      auto idx = _producers.get_index<"prototalvote"_n>();

      std::vector< std::pair<eosio::producer_key,uint16_t> > top_producers;
      top_producers.reserve(21);

      for ( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
         top_producers.emplace_back( std::pair<eosio::producer_key,uint16_t>({{it->owner, it->producer_key}, it->location}) );
      }

      if ( top_producers.size() == 0 || top_producers.size() < _gstate.last_producer_schedule_size ) {
         return;
      }

      /// sort by producer name
      std::sort( top_producers.begin(), top_producers.end() );

      std::vector<eosio::producer_key> producers;

      producers.reserve(top_producers.size());
      for( const auto& item : top_producers )
         producers.push_back(item.first);

      if( set_proposed_producers( producers ) >= 0 ) {
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
      }
   }

   double stake2vote( int64_t staked ) {
      /// TODO subtract 2080 brings the large numbers closer to this decade
      double weight = int64_t( (current_time_point().sec_since_epoch() - (block_timestamp::block_timestamp_epoch / 1000)) / (seconds_per_day * 7) )  / double( 52 );
      return double(staked) * std::pow( 2, weight );
   }

   double system_contract::update_total_votepay_share( const time_point& ct,
                                                       double additional_shares_delta,
                                                       double shares_rate_delta )
   {
      double delta_total_votepay_share = 0.0;
      if( ct > _gstate.last_vpay_state_update ) {
         delta_total_votepay_share = _gstate.total_vpay_share_change_rate
                                       * double( (ct - _gstate.last_vpay_state_update).count() / 1E6 );
      }

      delta_total_votepay_share += additional_shares_delta;
      if( delta_total_votepay_share < 0 && _gstate.total_producer_votepay_share < -delta_total_votepay_share ) {
         _gstate.total_producer_votepay_share = 0.0;
      } else {
         _gstate.total_producer_votepay_share += delta_total_votepay_share;
      }

      if( shares_rate_delta < 0 && _gstate.total_vpay_share_change_rate < -shares_rate_delta ) {
         _gstate.total_vpay_share_change_rate = 0.0;
      } else {
         _gstate.total_vpay_share_change_rate += shares_rate_delta;
      }

      _gstate.last_vpay_state_update = ct;

      return _gstate.total_producer_votepay_share;
   }

   double system_contract::update_producer_votepay_share( const producers_table2::const_iterator& prod_itr,
                                                          const time_point& ct,
                                                          double shares_rate,
                                                          bool reset_to_zero )
   {
      double delta_votepay_share = 0.0;
      if( shares_rate > 0.0 && ct > prod_itr->last_votepay_share_update ) {
         delta_votepay_share = shares_rate * double( (ct - prod_itr->last_votepay_share_update).count() / 1E6 ); // cannot be negative
      }

      double new_votepay_share = prod_itr->votepay_share + delta_votepay_share;
      _producers2.modify( prod_itr, same_payer, [&](auto& p) {
         if( reset_to_zero )
            p.votepay_share = 0.0;
         else
            p.votepay_share = new_votepay_share;

         p.last_votepay_share_update = ct;
      } );

      return new_votepay_share;
   }

   void system_contract::voteproducer( const name& voter_name, const name& producer, const asset& skate ) {
      require_auth( voter_name );
      vote_stake_updater( voter_name );
      update_votes( voter_name, producer, skate );
      auto rex_itr = _rexbalance.find( voter_name.value );
      if( rex_itr != _rexbalance.end() && rex_itr->rex_balance.amount > 0 ) {
         check_voting_requirement( voter_name, "voter holding REX tokens must vote for at least 21 producers or for a proxy" );
      }
   }

   void system_contract::update_votes( const name& voter_name, const name& producer, const asset& stake) {
      auto voter = _voters.find( voter_name.value );
      check( voter != _voters.end(), "user must stake before they can vote" ); /// staking creates voter object
      check( voter->current_stake + stake.amount < voter->staked, "attempt to vote more votes" );

	  //刷新投票总量信息
      _gstate.total_activated_stake += stake.amount;
      if ( _gstate.total_activated_stake >= min_activated_stake && _gstate.thresh_activated_stake_time == time_point() ) {
         _gstate.thresh_activated_stake_time = current_time_point();
      }
	  if (_gstate.total_activated_stake < min_activated_stake && _gstate.thresh_activated_stake_time > 0 )
      {
         _gstate.thresh_activated_stake_time = time_point();
      }

	  //刷新voter的producer信息
	  auto producers = voter->producers;
      auto p_iter = producers.find(producer);
      if (p_iter == producers.end()){
         check( stake.amount > 0, "the producer is not in the vote list" );
         check( producers.size() <= 30, "attempt to vote for too many producers" );
	  
         producers.emplace(std::map<account_name, int64_t>::value_type(producer, stake.amount));
      }
      else{
         check(p_iter->second + stake.amount >= 0, "attempt to unvote more votes" );
      
         p_iter->second += stake.amount;
         if (p_iter->second <= 0)
         {
            producers.erase(p_iter);
         }
      }

	  //通过质押计算票数
      auto new_vote_weight = stake2vote( stake.amount );

      const auto ct = current_time_point();
      double delta_change_rate         = 0.0;
      double total_inactive_vpay_share = 0.0;
	  
      auto pitr = _producers.find( producer.value );
      if( pitr != _producers.end() ) {
         double init_total_votes = pitr->total_votes;
         _producers.modify( pitr, same_payer, [&]( auto& p ) {
            p.total_votes += new_vote_weight;
            if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
               p.total_votes = 0;
            }
            _gstate.total_producer_vote_weight += new_vote_weight;
            //check( p.total_votes >= 0, "something bad happened" );
         });
         auto prod2 = _producers2.find( producer.value );
         if( prod2 != _producers2.end() ) {
            const auto last_claim_plus_3days = pitr->last_claim_time + microseconds(3 * useconds_per_day);
            bool crossed_threshold       = (last_claim_plus_3days <= ct);
            bool updated_after_threshold = (last_claim_plus_3days <= prod2->last_votepay_share_update);
            // Note: updated_after_threshold implies cross_threshold
 
            double new_votepay_share = update_producer_votepay_share( prod2,
                                          ct,
                                          updated_after_threshold ? 0.0 : init_total_votes,
                                          crossed_threshold && !updated_after_threshold // only reset votepay_share once after threshold
                                       );
 
            if( !crossed_threshold ) {
               delta_change_rate += new_vote_weight;
            } else if( !updated_after_threshold ) {
               total_inactive_vpay_share += new_votepay_share;
               delta_change_rate -= init_total_votes;
            }
         }
      } else {
         if( new_vote_weight > 0 ) {
            check( false, ( "producer " + producer.to_string() + " is not registered" ).data() );
         }
      }

      update_total_votepay_share( ct, -total_inactive_vpay_share, delta_change_rate );

      _voters.modify( voter, same_payer, [&]( auto& av ) {
         av.producers = producers;
		 av.current_stake += stake.amount;
		 
      });
   }

} /// namespace eosiosystem
