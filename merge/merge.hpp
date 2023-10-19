
#ifndef FLOW_CACHE_MERGE_MERGE_MERGE_HPP
#define FLOW_CACHE_MERGE_MERGE_MERGE_HPP



#include "kautil/algorithm/btree_search/btree_search.hpp"
#include "kautil/region/region.hpp"


namespace kautil{
namespace range{


template<typename preference>
struct merge{
    
    using value_type = typename preference::value_type;
    using offset_type = typename preference::offset_type;
    
    merge(preference * pref) : pref(pref){}
    ~merge(){}
    
    void set_buffer_size(offset_type v){ buffer_size = v; }
    void set_diff(value_type v){ diff = v; }
    int exec(value_type from,value_type to){
        
        using bt_type = kautil::algorithm::btree_search<preference>;
        auto is_contained = [](offset_type pos , int direction)->bool{
            return bool(
                !direction
                +(2 == ( bool(pos % (sizeof(value_type)*2)) + (direction<0)) )
                +(2 == (!bool(pos % (sizeof(value_type)*2)) + (direction>0)) )
            );
        };
        
        auto is_same_section = [](typename bt_type::btree_search_result const& btres0,typename  bt_type::btree_search_result const& btres1)->int{
            
            // if  nearest_pos is same, then
                // return -1 (case in the same vacant)
                    // 1) !nearest_pos % 16 &&  both direction < 0 && (both is not exact)
                    // 2) nearest_pos % 16 &&  both direction > 0 && (both is not exact)
                // return 1 (case in the same block)
                    // 1) !nearest_pos % 16 &&  both direction > 0
                    // 2) nearest_pos % 16 &&  both direction < 0
            // otherwise return 0 (two exist in different block) 
            
            auto is_same_pos = btres0.nearest_pos == btres1.nearest_pos; 
            auto is_even = !(btres0.nearest_pos%(sizeof(value_type)*2) ); 
            
            // -2 <= directions <= 2  direction==0 is counted as contained. 
            auto cond_exact = !btres0.direction+!btres1.direction;
            auto directions = btres0.direction+btres1.direction+cond_exact;
            
            return is_same_pos *(
                  bool((is_even * directions==-2) +(!is_even * directions==2))*!bool(cond_exact)*-1   
                + bool( is_even * directions==2 +!is_even * directions==-2)*1  
            );
        };
        
                        
        auto adjust_np_ovf = [](typename bt_type::btree_search_result const& i0,offset_type pos_min,offset_type  pos_max)->offset_type{
                // if upper overflow then nearest_pos = max_size   
                // if lower overflow then nearest_pos = mix_size   
                return 
                      i0.overflow*((i0.direction > 0)*pos_max + (i0.direction < 0)*pos_min)
                    +!i0.overflow*i0.nearest_pos;
        };
        
        // adjust direction with specified diff
        auto is_adjust_diff = [](typename bt_type::btree_search_result const& i0,value_type diff,value_type input)->bool{
            auto i0_diff=
                  (i0.nearest_value > input)*(i0.nearest_value-input)
                +!(i0.nearest_value > input)*(-i0.nearest_value+input);
            return (i0_diff <= diff);
        };
    
        if(false==from<to) return 1;

        auto fsize=  pref->size();
        if(!fsize){
            //printf("write block : from,to(%lld,%lld)",from,to);
            value_type new_block[2]= {from,to};
            auto new_block_ptr = &new_block;
            pref->write(0,(void**)&new_block_ptr,sizeof(new_block));
            //return 0;
        }else if(!(fsize % (sizeof(value_type)*2)) ){
            auto bt = kautil::algorithm::btree_search{pref};
            auto i0 = bt.search(from,false);
            auto i1 = bt.search(to,false);
            constexpr auto kSameBlock = 1,kDifferent = 0,kSamevacant = -1;
            auto cond_section = is_same_section(i0,i1);
            if(kSameBlock !=cond_section){ 
            
                auto c0_is_contained = is_contained(i0.nearest_pos,i0.direction);
                auto c1_is_contained = is_contained(i1.nearest_pos,i1.direction);
                auto i0_is_diff_adjust = !(c0_is_contained+i0.nan)*is_adjust_diff(i0,diff,from);
                auto i1_is_diff_adjust = !(c1_is_contained+i1.nan)*is_adjust_diff(i1,diff,to);
//                auto i0_is_diff_adjust = !i0.nan*is_adjust_diff(i0,diff,from);
//                auto i1_is_diff_adjust = !i1.nan*is_adjust_diff(i1,diff,to);
                
                
                { // adjust direction with specified diff
                    i0.direction*=!i0_is_diff_adjust;
                    i0.overflow*=!i0_is_diff_adjust;
                    i0.nearest_value=
                              i0_is_diff_adjust*from
                            +!i0_is_diff_adjust*i0.nearest_value; 
                            
                    i1.direction*=!i1_is_diff_adjust;
                    i1.overflow*=!i1_is_diff_adjust;
                    i1.nearest_value=
                              i1_is_diff_adjust*to
                            +!i1_is_diff_adjust*i1.nearest_value; 
                }

                {// ajdust cond_section, it is never be kSameVacaunt when overflow.   
                    auto is_ovf_occure = bool(i0.overflow+i1.overflow);
                    cond_section= !is_ovf_occure*cond_section + is_ovf_occure*kDifferent;   
                }
                
                auto is_claim_region = bool(kSamevacant==cond_section); // newly add element to memory or file  
                auto squash_src=offset_type(0),squash_length=offset_type(0);
                auto pos_min = 0;
                auto pos_max = pref->size()-sizeof(value_type);
                
                
                // adjust nearest_pos and nearest_value
                { // adjust overflow
                    // if upper overflow then 
                        // nearest_pos = max_size   
                        // nearest_value = to
                    
                    // if lower overflow then 
                        // nearest_pos = mix_size   
                        // nearest_value = from
                        
                    i0.nearest_pos = adjust_np_ovf(i0,pos_min,pos_max);
                    i0.nearest_value = i0.overflow*from + !i0.overflow*i0.nearest_value; 
                    
                    i1.nearest_pos = adjust_np_ovf(i1,pos_min,pos_max);
                    i1.nearest_value = i1.overflow*to + !i1.overflow*i1.nearest_value;

                }
                
                
                 
                {
                    // if c0 is contained and 
                        // d < 0 then -=8
                        // d > 0 then nothing
                        // d = 0 and !(%(sizeof(value_type)*2)) then -=8
                        // d = 0 and (%(sizeof(value_type)*2)) then nothing
                    //auto c0_is_contained = bool(!i0.overflow * is_contained(i0.nearest_pos,i0.direction));
                    auto c0_adj_pos = -sizeof(value_type); 
                    auto c0_cond_d =i0.direction<0; 
                    auto c0_cond_dzero = 2==(!i0.direction+bool(i0.nearest_pos%(sizeof(value_type)*2) ));
                    //auto c0_is_contained = is_contained(i0.nearest_pos,i0.direction);
                    auto c0_is_contained_adjust = 3==(c0_is_contained+!i0.overflow+bool(c0_cond_d+c0_cond_dzero)); 
                    if(c0_is_contained_adjust){
                        i0.nearest_pos+=static_cast<offset_type>(c0_adj_pos);
                        auto ptr = &i0.nearest_value;
                        pref->read_value(i0.nearest_pos,&ptr);
                    }
                    
                    i0.nearest_value=
                            !c0_is_contained*from
                            +c0_is_contained*i0.nearest_value;
                    
                    // if c1 is contained and 
                        // d < 0 then nothing
                        // d > 0 then +=8
                        // d = 0 and !(%(sizeof(value_type)*2)) then nothing
                        // d = 0 and (%(sizeof(value_type)*2)) then +=8
                    auto c1_adj_pos = sizeof(value_type); 
                    auto c1_cond_d =i1.direction>0; 
                    auto c1_cond_dzero = 2==(!i1.direction+!bool(i1.nearest_pos%(sizeof(value_type)*2) ));
                    //auto c1_is_contained = is_contained(i1.nearest_pos,i1.direction);
                    auto c1_is_contained_adjust = 3==(c1_is_contained+!i1.overflow+bool(c1_cond_d+c1_cond_dzero)); 
                    if(c1_is_contained_adjust){
                        i1.nearest_pos+=static_cast<offset_type>(c1_adj_pos);
                        auto ptr = &i1.nearest_value;
                        pref->read_value(i1.nearest_pos,&ptr);
                    }
                    
                    i1.nearest_value=
                            !c1_is_contained*to
                            +c1_is_contained*i1.nearest_value;
                    
                    
                    auto is_same_vacant = bool(cond_section==kSamevacant);
                    i0.nearest_pos= 
                             !i0.overflow*(is_same_vacant*(i0.nearest_pos+sizeof(value_type))+!is_same_vacant*i0.nearest_pos)
                             +i0.overflow*i0.nearest_pos;
                    i1.nearest_pos= 
                             !i1.overflow*(is_same_vacant*(i0.nearest_pos+sizeof(value_type))+!is_same_vacant*i1.nearest_pos)
                             +i1.overflow*i1.nearest_pos;

                    {
                        // adjust pos of i0 when it　belongs to vacant 
                        auto i0_adj_when_vacant = offset_type(
                            bool(2==(!bool(i0.nearest_pos%(sizeof(value_type)*2))+(i0.direction<0)))*(-sizeof(value_type))
                            +bool(2==( bool(i0.nearest_pos%(sizeof(value_type)*2))+(i0.direction>0)))*(sizeof(value_type))
                        );
                        i0.nearest_pos+=!i0.overflow*!c0_is_contained*i0_adj_when_vacant;
                        
                        // express squash with i1 
                        squash_src = i1.nearest_pos; 
                        squash_length = static_cast<offset_type>(((i1.nearest_pos-i0.nearest_pos)/(sizeof(value_type)*2))*sizeof(value_type)*2);
                        i1.nearest_pos-=squash_length;
                        //i1.nearest_pos-=static_cast<offset_type>(((i1.nearest_pos-i0.nearest_pos)/(sizeof(value_type)*2))*sizeof(value_type)*2);
                    }
                }

                
                auto is_ovf_ovf_upper = (2==(i0.overflow+(i0.direction>0))); 
                auto is_ovf_ovf_lower = (2==(i1.overflow+(i1.direction<0))); 
                {// adjust when ovf_ovf
                    auto is_ovf_ovf = bool(is_ovf_ovf_upper+is_ovf_ovf_lower);
                    i0.nearest_pos=is_ovf_ovf_upper*fsize +!is_ovf_ovf_upper*i0.nearest_pos;  
                    i0.nearest_pos=/*is_ovf_ovf_lower*0+*/ !is_ovf_ovf_lower*i0.nearest_pos;  
                    i1.nearest_pos=static_cast<offset_type>(
                              is_ovf_ovf*(i0.nearest_pos+sizeof(value_type))
                            +!is_ovf_ovf*i1.nearest_pos
                            );
                    is_claim_region|=is_ovf_ovf; 
                }
                
                printf("[%ld] %lld\n",i0.nearest_pos,i0.nearest_value);fflush(stdout);
                printf("[%ld] %lld\n",i1.nearest_pos,i1.nearest_value);fflush(stdout);
                /*
                */
                
                if(is_claim_region){
                    auto claim_size = is_ovf_ovf_upper*(fsize-sizeof(value_type));
                    auto claim_res = kautil::region{pref}.claim(claim_size,sizeof(value_type)*2,buffer_size);
                    //printf("claim region (src,length)(%ld,%ld) : return(%d)\n",claim_res,claim_size,sizeof(value_type)*2);
                }else{ // squash
                    auto shrink_res = kautil::region{pref}.shrink(squash_src,squash_length,buffer_size);
                    //printf("squash (src,length)(%ld,%ld) return(%d)\n",squash_length,squash_src,shrink_res);
                }
                
                {
                    auto i0_ptr = &i0.nearest_value;
                    pref->write(i0.nearest_pos,(void**)&i0_ptr,sizeof(value_type));
                }

                {
                    auto i1_ptr = &i1.nearest_value;
                    pref->write(i1.nearest_pos,(void**)&i1_ptr,sizeof(value_type));
                }
                
                /*
                printf("output result\n");
                debug_out_file<value_type,offset_type>(stdout,pref->fd,0,100);
                printf("+++++++++++++++++++++++++++++\n");
                */
                return 0;
            }
        }else return 2;
        return 0;
    }
    
    const char* error_msg(int err){
        switch (err) {
            case 1: {
                static const char * kMsg = "error(1) : invalid input";
                return kMsg;
            }; 
            case 2:
            {
                static const char * kMsg = "error(2) : corrupted region.";
                return kMsg;
            }; 
            case 3: {
                static const char * kMsg = "error(3) :unknown error";
                return kMsg;
            }; 
            default: {
                static const char * kMsg = "not unerror";
                return kMsg;
            };
            
        }
    }
    
private:
    preference * pref;
    value_type diff = value_type(1);
    offset_type buffer_size = offset_type(512);
};


} //namespace range{
} //namespace kautil{


#endif