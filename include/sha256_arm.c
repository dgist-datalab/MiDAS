#include "sha256_arm.h"
#include <stdlib.h>
void sha256_process_arm(uint8_t data[], uint32_t length,uint32_t seed, void *out){
	uint32_t state[8]={0x6A09E667UL,0xBB67AE85UL,0x3C6EF372UL,0xA54FF53AUL,0x510E527FUL,0x9B05688CUL,0x1F83D9ABUL,0x5BE0CD19UL};
	for(int i=0; i<length; i++){
		data[i]+=seed;
	}
	uint32x4_t STATE0, STATE1, ABEF_SAVE, CDGH_SAVE;
	uint32x4_t MSG0, MSG1, MSG2, MSG3;
	uint32x4_t TMP0, TMP1, TMP2;

	/* Load state */
	STATE0 = vld1q_u32(&state[0]);
	STATE1 = vld1q_u32(&state[4]);

	while (length >= 64)
	{
		/* Save state */
		ABEF_SAVE = STATE0;
		CDGH_SAVE = STATE1;

		/* Load message */
		MSG0 = vld1q_u32((const uint32_t *)(data +  0));
		MSG1 = vld1q_u32((const uint32_t *)(data + 16));
		MSG2 = vld1q_u32((const uint32_t *)(data + 32));
		MSG3 = vld1q_u32((const uint32_t *)(data + 48));

		/* Reverse for little endian */
		MSG0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG0)));
		MSG1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG1)));
		MSG2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG2)));
		MSG3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG3)));

		TMP0 = vaddq_u32(MSG0, vld1q_u32(&KK[0x00]));

		/* Rounds 0-3 */
		MSG0 = vsha256su0q_u32(MSG0, MSG1);
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG1, vld1q_u32(&KK[0x04]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
		MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);

		/* Rounds 4-7 */
		MSG1 = vsha256su0q_u32(MSG1, MSG2);
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG2, vld1q_u32(&KK[0x08]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
		MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);

		/* Rounds 8-11 */
		MSG2 = vsha256su0q_u32(MSG2, MSG3);
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG3, vld1q_u32(&KK[0x0c]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
		MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);

		/* Rounds 12-15 */
		MSG3 = vsha256su0q_u32(MSG3, MSG0);
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG0, vld1q_u32(&KK[0x10]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
		MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

		/* Rounds 16-19 */
		MSG0 = vsha256su0q_u32(MSG0, MSG1);
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG1, vld1q_u32(&KK[0x14]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
		MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);

		/* Rounds 20-23 */
		MSG1 = vsha256su0q_u32(MSG1, MSG2);
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG2, vld1q_u32(&KK[0x18]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
		MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);

		/* Rounds 24-27 */
		MSG2 = vsha256su0q_u32(MSG2, MSG3);
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG3, vld1q_u32(&KK[0x1c]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
		MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);

		/* Rounds 28-31 */
		MSG3 = vsha256su0q_u32(MSG3, MSG0);
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG0, vld1q_u32(&KK[0x20]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
		MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

		/* Rounds 32-35 */
		MSG0 = vsha256su0q_u32(MSG0, MSG1);
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG1, vld1q_u32(&KK[0x24]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
		MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);

		/* Rounds 36-39 */
		MSG1 = vsha256su0q_u32(MSG1, MSG2);
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG2, vld1q_u32(&KK[0x28]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
		MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);

		/* Rounds 40-43 */
		MSG2 = vsha256su0q_u32(MSG2, MSG3);
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG3, vld1q_u32(&KK[0x2c]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
		MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);

		/* Rounds 44-47 */
		MSG3 = vsha256su0q_u32(MSG3, MSG0);
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG0, vld1q_u32(&KK[0x30]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
		MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

		/* Rounds 48-51 */
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG1, vld1q_u32(&KK[0x34]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

		/* Rounds 52-55 */
		TMP2 = STATE0;
		TMP0 = vaddq_u32(MSG2, vld1q_u32(&KK[0x38]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);

		/* Rounds 56-59 */
		TMP2 = STATE0;
		TMP1 = vaddq_u32(MSG3, vld1q_u32(&KK[0x3c]));
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

		/* Rounds 60-63 */
		TMP2 = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
		STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);

		/* Combine state */
		STATE0 = vaddq_u32(STATE0, ABEF_SAVE);
		STATE1 = vaddq_u32(STATE1, CDGH_SAVE);

		data += 64;
		length -= 64;
	}

	/* Save state */
	vst1q_u32(&state[0], STATE0);
	vst1q_u32(&state[4], STATE1);
	uint32_t result=0;
	for(int i=0; i<8; i++){
		result^=state[i];
	}
	for(int i=0; i<length; i++){
		data[i]-=seed;
	}
	*(uint32_t*)out=result;
}
