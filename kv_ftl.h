#ifndef KV_FTL_H_
#define KV_FTL_H_

#include "ftl_config.h"

#define KV_MAX_STORED_KEYS             (4194304)
#define KV_HASH_TABLE_SIZE             (8388608)
#define KV_HASH_TABLE_MASK             (KV_HASH_TABLE_SIZE - 1)
#define KV_VALUE_SIZE                  (BYTES_PER_NVME_BLOCK)	// 4KB
#define KV_MAX_CMD_SLOTS               (1024)

#define KV_STATUS_OK                   (0)
#define KV_STATUS_NO_SUCH_KEY          (1)
#define KV_STATUS_ERROR                (2)

typedef struct _KV_INDEX_ENTRY {
	unsigned int key;
	unsigned int valueLba;		// LBA of the value in the storage
	unsigned int valueLength;
} KV_INDEX_ENTRY, *P_KV_INDEX_ENTRY;

typedef struct _KV_INDEX_TABLE {
	KV_INDEX_ENTRY entry[KV_HASH_TABLE_SIZE];
} KV_INDEX_TABLE, *P_KV_INDEX_TABLE;

void InitKvFtl();
unsigned int KvPut(unsigned int cmdSlotTag, unsigned int key, unsigned int valueLength);
unsigned int KvGet(unsigned int cmdSlotTag, unsigned int key, unsigned int hostBufferLength);
unsigned int KvHasPendingGetCompletion(unsigned int cmdSlotTag);
unsigned int KvConsumePendingGetCompletion(unsigned int cmdSlotTag, unsigned int *valueLength);

#endif /* KV_FTL_H_ */
