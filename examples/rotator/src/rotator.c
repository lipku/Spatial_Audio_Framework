/*
 Copyright 2017-2018 Leo McCormack
 
 Permission to use, copy, modify, and/or distribute this software for any purpose with or
 without fee is hereby granted, provided that the above copyright notice and this permission
 notice appear in all copies.
 
 THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
 SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
 ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
 * Filename:
 *     rotator.c
 * Description:
 *     A simple spherical harmonic domain rotator.
 * Dependencies:
 *     saf_utilities, saf_sh
 * Author, date created:
 *     Leo McCormack, 02.11.2017
 */

#include "rotator.h"
#include "rotator_internal.h"


void rotator_create
(
    void ** const phRot
)
{
    rotator_data* pData = (rotator_data*)malloc(sizeof(rotator_data));
    if (pData == NULL) { return;/*error*/ }
    *phRot = (void*)pData;
  
	/* Default user parameters */
    pData->yaw = 0.0f;
    pData->pitch = 0.0f;
    pData->roll = 0.0f;
    pData->bFlipYaw = 0;
    pData->bFlipPitch = 0;
    pData->bFlipRoll = 0;
    pData->chOrdering = CH_ACN;
    pData->norm = NORM_N3D;
}


void rotator_destroy
(
    void ** const phRot
)
{
    rotator_data *pData = (rotator_data*)(*phRot);

    if (pData != NULL) {
        free(pData);
        pData = NULL;
    }
}


void rotator_init
(
    void * const hRot,
    int          sampleRate
)
{
    rotator_data *pData = (rotator_data*)(hRot);
    /**/
}


void rotator_process
(
    void  *  const hRot,
    float ** const inputs,
    float ** const outputs,
    int            nInputs,
    int            nOutputs,
    int            nSamples,
	int            isPlaying
)
{
    rotator_data *pData = (rotator_data*)(hRot);
    int i, n, ch;
    int o[SH_ORDER+2];
    float Rxyz[3][3], M_rot[NUM_SH_SIGNALS][NUM_SH_SIGNALS];
    CH_ORDER chOrdering;
    NORM_TYPES norm;
 
    if (nSamples == FRAME_SIZE && isPlaying) {
        /* prep */
        for(n=0; n<SH_ORDER+2; n++){  o[n] = n*n;  }
        chOrdering = pData->chOrdering;
        norm = pData->norm;
		for (i = 0; i < MIN(NUM_SH_SIGNALS, nInputs); i++)
			memcpy(pData->inputFrameTD[i], inputs[i], FRAME_SIZE * sizeof(float));
		for (; i < NUM_SH_SIGNALS; i++)
			memset(pData->inputFrameTD[i], 0, FRAME_SIZE * sizeof(float));
        
        /* account for norm scheme */
        switch(norm){
            case NORM_N3D: /* already N3D */
                break;
            case NORM_SN3D: /* convert to N3D before rotation */
                for (n = 0; n<SH_ORDER+1; n++)
                    for (ch = o[n]; ch<o[n+1]; ch++)
                        for(i = 0; i<FRAME_SIZE; i++)
                            pData->outputFrameTD[ch][i] *= sqrtf(2.0f*(float)n+1.0f);
                break;
        }
        
        /* calculate rotation matrix */
        yawPitchRoll2Rzyx (pData->yaw, pData->pitch, pData->roll, Rxyz);
        getSHrotMtxReal(Rxyz, (float*)M_rot, SH_ORDER);
        
        /* apply rotation (assumes ACN/N3D) */
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, NUM_SH_SIGNALS, FRAME_SIZE, NUM_SH_SIGNALS, 1.0f,
                    (float*)M_rot, NUM_SH_SIGNALS,
                    (float*)pData->inputFrameTD, FRAME_SIZE, 0.0f,
                    (float*)pData->outputFrameTD, FRAME_SIZE);
        
        /* account for norm scheme */
        switch(norm){
            case NORM_N3D: /* already N3D */
                break;
            case NORM_SN3D: /* convert back to SN3D after rotation */
                for (n = 0; n<SH_ORDER+1; n++)
                    for (ch = o[n]; ch<o[n+1]; ch++)
                        for(i = 0; i<FRAME_SIZE; i++)
                            pData->outputFrameTD[ch][i] /= sqrtf(2.0f*(float)n+1.0f);
                break;
        }
        for (i=0; i < MIN(NUM_SH_SIGNALS, nOutputs); i++)
            memcpy(outputs[i], pData->outputFrameTD[i], FRAME_SIZE*sizeof(float));
        for (; i < nOutputs; i++)
            memset(outputs[i], 0, FRAME_SIZE*sizeof(float));
    }
    else{
        for (i=0; i < nOutputs; i++)
            memset(outputs[i], 0, FRAME_SIZE*sizeof(float));
    }
}

void rotator_setYaw(void  * const hRot, float newYaw)
{
    rotator_data *pData = (rotator_data*)(hRot);
    pData->yaw = pData->bFlipYaw == 1 ? -DEG2RAD(newYaw) : DEG2RAD(newYaw);
}

void rotator_setPitch(void* const hRot, float newPitch)
{
    rotator_data *pData = (rotator_data*)(hRot);
    pData->pitch = pData->bFlipPitch == 1 ? -DEG2RAD(newPitch) : DEG2RAD(newPitch);
}

void rotator_setRoll(void* const hRot, float newRoll)
{
    rotator_data *pData = (rotator_data*)(hRot);
    pData->roll = pData->bFlipRoll == 1 ? -DEG2RAD(newRoll) : DEG2RAD(newRoll);
}

void rotator_setFlipYaw(void* const hRot, int newState)
{
    rotator_data *pData = (rotator_data*)(hRot);
    if(newState !=pData->bFlipYaw ){
        pData->bFlipYaw = newState;
        rotator_setYaw(hRot, -rotator_getYaw(hRot));
    }
}

void rotator_setFlipPitch(void* const hRot, int newState)
{
    rotator_data *pData = (rotator_data*)(hRot);
    if(newState !=pData->bFlipPitch ){
        pData->bFlipPitch = newState;
        rotator_setPitch(hRot, -rotator_getPitch(hRot));
    }
}

void rotator_setFlipRoll(void* const hRot, int newState)
{
    rotator_data *pData = (rotator_data*)(hRot);
    if(newState !=pData->bFlipRoll ){
        pData->bFlipRoll = newState;
        rotator_setRoll(hRot, -rotator_getRoll(hRot));
    }
}

void rotator_setChOrder(void* const hRot, int newOrder)
{
    rotator_data *pData = (rotator_data*)(hRot);
    pData->chOrdering = (CH_ORDER)newOrder;
}

void rotator_setNormType(void* const hRot, int newType)
{
    rotator_data *pData = (rotator_data*)(hRot);
    pData->norm = (NORM_TYPES)newType;
}

/*gets*/

float rotator_getYaw(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return pData->bFlipYaw == 1 ? -RAD2DEG(pData->yaw) : RAD2DEG(pData->yaw);
}

float rotator_getPitch(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return pData->bFlipPitch == 1 ? -RAD2DEG(pData->pitch) : RAD2DEG(pData->pitch);
}

float rotator_getRoll(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return pData->bFlipRoll == 1 ? -RAD2DEG(pData->roll) : RAD2DEG(pData->roll);
}

int rotator_getFlipYaw(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return pData->bFlipYaw;
}

int rotator_getFlipPitch(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return pData->bFlipPitch;
}

int rotator_getFlipRoll(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return pData->bFlipRoll;
}

int rotator_getChOrder(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return (int)pData->chOrdering;
}

int rotator_getNormType(void* const hRot)
{
    rotator_data *pData = (rotator_data*)(hRot);
    return (int)pData->norm;
}




