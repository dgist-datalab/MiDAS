#include "oob_manager.h"

extern lsmtree LSM;

oob_manager *get_oob_manager(uint32_t ppa){
	return (oob_manager*)LSM.pm->bm->get_oob(LSM.pm->bm, ppa);
}

uint32_t get_version_from_piece_ppa(uint32_t piece_ppa){
	oob_manager *oob=get_oob_manager(PIECETOPPA(piece_ppa));
	return oob->version[piece_ppa%L2PGAP];
}
