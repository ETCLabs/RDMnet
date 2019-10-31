#ifndef UID_H
#define UID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****The basic RDM UID definition*****/
typedef struct uid
{
	uint16_t manu; /*The manufacturer id*/
	uint32_t id;   /*the device id under that manufacturer*/
}uid;

#ifdef __cplusplus
};
#endif

#endif