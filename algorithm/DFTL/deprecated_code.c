
uint32_t demand_map_assign(request *req, KEYT *_lba, KEYT *_physical, uint32_t *prefetching_info){
	uint8_t i=0;
	demand_param *dp;
	assign_param_ex *mp;

	uint32_t gtd_idx;
	uint32_t trans_offset;
	GTD_entry *etr;
	uint32_t old_ppa;
	KEYT *lba=NULL, *physical=NULL;

	if(!req->param){
		dp=(demand_param*)calloc(1, sizeof(demand_param));
		dp_status_update(dp, NONE);
		lba=_lba; physical=_physical;
		gtd_idx=GETGTDIDX(lba[i]);
		trans_offset=TRANSOFFSET(lba[i]);
		etr=dp->etr=&dmm.GTD[gtd_idx];

		mp=(assign_param_ex*)malloc(sizeof(assign_param_ex));
		cpy_keys(&mp->lba,_lba);
		cpy_keys(&mp->physical, _physical);
		mp->prefetching_info=(uint32_t*)malloc(sizeof(uint32_t)*L2PGAP);
		memcpy(mp->prefetching_info, prefetching_info, sizeof(uint32_t)*L2PGAP);
		i=mp->idx=0;
		dp->param_ex=(void*)mp;
		dp->is_hit_eviction=false;
		req->param=(void*)dp;
	}
	else{
		dp=(demand_param*)req->param;
		mp=(assign_param_ex*)dp->param_ex;

		i=mp->idx;
		lba=mp->lba;
		physical=mp->physical;
		etr=dp->etr;
	}
	

	GTD_entry *target_etr;
	for(;i<L2PGAP; i++){
		gtd_idx=GETGTDIDX(lba[i]);
		trans_offset=TRANSOFFSET(lba[i]);
		etr=&dmm.GTD[gtd_idx];

		dp->etr=etr;
		mp->idx=i;
		mapping_entry *target=&dp->target;
		target->lba=lba[i];
		target->ppa=physical[i];
retry:
		switch(dp->status){
			case EVICTIONW:
				if(dp->is_hit_eviction){
					dp_status_update(dp, HIT);
					goto retry;
				}
				dp_status_update(dp,MISSR);
				if(etr->status==EMPTY){
					if(dmm.cache->type==FINE){
						dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, lba[i], dmm.eviction_hint, false);
					}
					dp_status_update(dp, HIT);
					goto retry;
				}

				if(dmm.cache->type==COARSE){
					//target entry can be populated while it processes eviction
					if(dmm.cache->exist(dmm.cache, lba[i])){
						dp_status_update(dp, HIT);
						goto retry;
					}
				}

				if(map_read_wrapper(etr, req, dmm.li, dp, dp->target.lba)){
					if(dmm.cache->type==COARSE){
						dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, lba[i], dmm.eviction_hint, false);
					}
					//printf("write flying hit! %d %u\n", ++MISSR_read_flying_check, dmm.eviction_hint);
					return 1;
				}
				else{
					return 0;
				}
			case NONE:
				if(!dmm.cache->exist(dmm.cache, lba[i])){
					if(flying_request_hit_check(req, lba[i], dp, true)){
						return 1;
					}
					DMI.miss_num++;
					DMI.write_miss_num++;
					if(dmm.cache->is_needed_eviction(dmm.cache, lba[i], &mp->prefetching_info[i], 
								&dmm.eviction_hint)){
eviction_path:
						if(dmm.cache->is_eviction_hint_full(dmm.cache, dmm.eviction_hint)){
							/*When using Asynchronous I/O the request may be delay since the number of evicting requests is over cache size*/
							dp_status_update(dp, NONE);
							if(!inf_assign_try(req)){
								abort();
							}
							return 1;
						}
						DMI.eviction_cnt++;
						dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, lba[i], dmm.eviction_hint, true);

						if(dmm.cache->type==COARSE){
							dp->et.gtd=dmm.cache->get_eviction_GTD_entry(dmm.cache, target->lba);
							if(!dp->et.gtd){
								DMI.clean_eviction++;
								dp_status_update(dp, EVICTIONW);
							}
							else{
								dp_status_update(dp, EVICTIONR);
								DMI.dirty_eviction++;		
							}
							goto retry;
						}
						else{
							dp->et.mapping=dmm.cache->get_eviction_mapping_entry(dmm.cache, target->lba);
							if(!dp->et.mapping){
								DMI.clean_eviction++;
								dp_status_update(dp, EVICTIONW);
								goto retry;
							}
							else{
								DMI.dirty_eviction++;							
							}
							target_etr=&dmm.GTD[GETGTDIDX(dp->et.mapping->lba)];

							if(target_etr->physical_address==UINT32_MAX){
								dp_status_update(dp, EVICTIONR);
								fdriver_lock(&target_etr->lock);
								list_insert(target_etr->pending_req, (void*)req);
								fdriver_unlock(&target_etr->lock);
								goto retry;
							}
							else{
								dp_status_update(dp, EVICTIONR);
								return map_read_wrapper(target_etr, req, dmm.li, dp, dp->et.mapping->lba);
							}
						}
					}
					else{
						dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, lba[i], 
								dmm.eviction_hint, true);
						DMI.cold_miss_num++;
					}

					if(etr->status!=EMPTY){
						dp->now_eviction_hint=dmm.eviction_hint;
						dp_status_update(dp, MISSR);
						return map_read_wrapper(etr, req, dmm.li, dp, dp->target.lba);
					}
					else{/*cold miss and direct inserting*/
						dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, lba[i], dmm.eviction_hint, false);
						dp_status_update(dp, HIT);
					}
				}
				else{
					if(dmm.cache->entry_type==DYNAMIC &&
							dmm.cache->is_hit_eviction(dmm.cache, etr, lba[i], physical[i])){
						DMI.hit_eviction++;
						dmm.cache->force_put_mru(dmm.cache, etr, target, lba[i]);
						dp->is_hit_eviction=true;
						goto eviction_path;
					}
					DMI.hit_num++;
					DMI.write_hit_num++;
					dp_status_update(dp, HIT);
				}
			case HIT:
				old_ppa=dmm.cache->update_entry(dmm.cache, etr, target->lba, target->ppa, &dmm.eviction_hint);

				if(old_ppa!=UINT32_MAX){
#ifdef DFTL_DEBUG
					printf("HIT - lba:%u ppa:%u -> %u, read_mapping:%u\n", target->lba, old_ppa, target->ppa, dmm.cache->get_mapping(dmm.cache, target->lba));
#endif
					invalidate_ppa(old_ppa);
				}
				dp->is_hit_eviction=false;
				dp_status_update(dp, NONE);
				dp_prev_init(dp);
				break;
			case EVICTIONR:
				if(dmm.cache->type==COARSE){
					dmm.cache->update_eviction_target_translation(dmm.cache,target->lba, dp->et.gtd, NULL,  req->value->value);
					target_etr=dp->et.gtd;
				}
				else{
					target_etr=&dmm.GTD[GETGTDIDX(dp->et.mapping->lba)];
					demand_map_fine_type_pending(req, dp->et.mapping, req->value->value, &mp->prefetching_info[i]);
				}

				if(dp->is_hit_eviction){
					dp_status_update(dp, HIT);
				}
				else if(dmm.cache->entry_type==DYNAMIC && dmm.cache->need_more_eviction(dmm.cache,lba[i], &mp->prefetching_info[i], &dmm.eviction_hint)){
					dp_status_update(dp, NONE);
				}
				else{
					dp_status_update(dp, EVICTIONW);
				}
				write_updated_map(req, target_etr, dp);
				return 1;
			case MISSR:
				if(dmm.cache->type==FINE){
					if(demand_map_fine_type_pending(req, target, req->value->value, &mp->prefetching_info[i])==1){
						return 1;
					}
				}
				else if(demand_map_coarse_type_pending(req, etr, req->value->value)==1){
					return 1;
				}
				dp_status_update(dp, NONE);
				break;
		}
	}
	
	if(i!=L2PGAP){
		return 0;
	}
	if(i==L2PGAP){
		if(req->param){
			free(mp->lba);
			free(mp->physical);
			free(mp->prefetching_info);
			free(mp);
			free(req->param);
		}
		req->end_req(req);
		return 1;
	}
	return 0;
}


uint32_t demand_page_read(request *const req){
	demand_param *dp;
	mapping_entry target;
	uint32_t gtd_idx, trans_offset;
	uint32_t ppa;
	GTD_entry *etr;

	if(!req->param){
		dp=(demand_param*)calloc(1, sizeof(demand_param));
		dp->param_ex=NULL;
		dp_status_update(dp, NONE);
		gtd_idx=GETGTDIDX(req->key);
		trans_offset=TRANSOFFSET(req->key);
		etr=dp->etr=&dmm.GTD[gtd_idx];
		dp->is_hit_eviction=false;
		req->param=(void*)dp;
	}else{
		dp=(demand_param*)req->param;
		etr=dp->etr;
	}


	if(req->key==test_key){
		EPRINT("read function", false);
	}

	GTD_entry *target_etr;
retry:
	switch(dp->status){
		case EVICTIONW:
			dp_status_update(dp, MISSR);
			if(map_read_wrapper(etr, req, dmm.li, dp, req->key)){
				if(dmm.cache->type==COARSE){
					dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, req->key, dmm.eviction_hint, false);
				}
				return 1;
			}
			else{ 
				return 0;
			}
		case NONE:
			if(dmm.cache->type==COARSE && etr->status==EMPTY){
				printf("%s:%d non populate error!\n", __FILE__,__LINE__);
				abort();
			}

			if(!dmm.cache->exist(dmm.cache, req->key)){//cache miss
				if(flying_request_hit_check(req, req->key, dp, false)){
					return 1;
				}
				DMI.miss_num++;	
				DMI.read_miss_num++;
				if(dmm.cache->is_needed_eviction(dmm.cache, req->key, &req->consecutive_length, &dmm.eviction_hint)){

					if(dmm.cache->is_eviction_hint_full(dmm.cache, dmm.eviction_hint)){
						/*When using Asynchronous I/O the request may be delay since the number of evicting requests is over cache size*/
						dp_status_update(dp, NONE);
						if(!inf_assign_try(req)){
							abort();
						}
						return 1;
					}
					DMI.eviction_cnt++;
					dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, req->key, dmm.eviction_hint, true);
					if(dmm.cache->type==COARSE){
						dp->et.gtd=dmm.cache->get_eviction_GTD_entry(dmm.cache, req->key);
						if(!dp->et.gtd){
							dp_status_update(dp, EVICTIONW); // it is clean
							DMI.clean_eviction++;
						}
						else{
							dp_status_update(dp, EVICTIONR); //it is dirty
							DMI.dirty_eviction++;
						}
						goto retry;						
					}
					else{
						dp->et.mapping=dmm.cache->get_eviction_mapping_entry(dmm.cache, req->key);
						if(!dp->et.mapping){ 
							dp_status_update(dp, EVICTIONW);
							DMI.clean_eviction++;
							goto retry;
						}
						else{
							DMI.dirty_eviction++; 
						}
						target_etr=&dmm.GTD[GETGTDIDX(dp->et.mapping->lba)];
						dp_status_update(dp, EVICTIONR);
						if(target_etr->physical_address==UINT32_MAX){
							fdriver_lock(&target_etr->lock);
							list_insert(target_etr->pending_req, (void*)req);
							fdriver_unlock(&target_etr->lock);
							goto retry;
						}
						else{
							return map_read_wrapper(target_etr, req, dmm.li,dp, dp->et.mapping->lba);
						}
					}
					dp_status_update(dp, MISSR);
					abort();
					return map_read_wrapper(etr, req, dmm.li, dp, dp->et.mapping->lba);
				}
				else{
					dmm.eviction_hint=dmm.cache->update_eviction_hint(dmm.cache, req->key, dmm.eviction_hint, true);
					DMI.cold_miss_num++;
					dp_status_update(dp, MISSR);
					return map_read_wrapper(etr, req, dmm.li,dp, req->key);
				}
			}
			else{
				DMI.hit_num++;	
				DMI.read_hit_num++;
				dp_status_update(dp, HIT);
			}
		case HIT:
			ppa=dmm.cache->get_mapping(dmm.cache, req->key);
			goto read_data;
		case EVICTIONR:
			if(dmm.cache->type==COARSE){
				dmm.cache->update_eviction_target_translation(dmm.cache,req->key, dp->et.gtd, NULL, req->value->value);
				target_etr=dp->et.gtd;
			}
			else{
				target_etr=&dmm.GTD[GETGTDIDX(dp->et.mapping->lba)];
				demand_map_fine_type_pending(req, dp->et.mapping, req->value->value, &req->consecutive_length);
			}

			if(dmm.cache->entry_type==DYNAMIC && dmm.cache->need_more_eviction(dmm.cache, req->key, &req->consecutive_length, &dmm.eviction_hint)){
				//printf("more eviction!!!\n");
				dp_status_update(dp, NONE);
			}
			else{
				dp_status_update(dp, EVICTIONW);
			}
			
			write_updated_map(req, target_etr, dp);
			return 1;
		case MISSR:
			if(dmm.cache->type==FINE){
				target.lba=req->key;
				if((demand_map_fine_type_pending(req, &target, req->value->value, &req->consecutive_length)==1)){
					goto end;
				}
			}
			else if(demand_map_coarse_type_pending(req, etr, req->value->value)==1){ //this is issue read request
				goto end;
			}
	}

read_data:
	if(ppa==UINT32_MAX){
		printf("try to read invalidate ppa %s:%d\n", __FILE__,__LINE__);
		req->type=FS_NOTFOUND_T;
		req->end_req(req);
		goto end;
	}
	if(req->key==test_key){
		printf("%u:%u read data\n", req->key, ppa);
	}
	req->value->ppa=ppa;
	free(req->param);
	send_user_req(req, DATAR, ppa/L2PGAP, req->value);
end:
	return 1;
}
