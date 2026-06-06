#include "kv_ftl.h"
#include "memory_map.h"
#include "nvme/nvme.h"
#include "request_transform.h"

P_KV_INDEX_TABLE kvIndexTablePtr;

static unsigned int kvNextValueLba;
static unsigned int kvStoredKeyCount;
static unsigned int kvPendingGetLength[KV_MAX_CMD_SLOTS];

static unsigned int KvHash(unsigned int key)
{
	key ^= key >> 16;
	key *= 0x7feb352d;
	key ^= key >> 15;
	key *= 0x846ca68b;
	key ^= key >> 16;

	return key;
}

static unsigned int KvFindSlot(unsigned int key, unsigned int *slot, unsigned int *found)
{
	unsigned int idx;
	unsigned int probe;

	idx = KvHash(key) & KV_HASH_TABLE_MASK;

	for (probe = 0; probe < KV_HASH_TABLE_SIZE; ++probe) {
		if (kvIndexTablePtr->entry[idx].valueLength == 0) {
			*slot = idx;
			*found = 0;	// empty slot
			return KV_STATUS_OK;
		}

		if (kvIndexTablePtr->entry[idx].key == key) {
			*slot = idx;
			*found = 1;	// found the key
			return KV_STATUS_OK;
		}

		idx = (idx + 1) & KV_HASH_TABLE_MASK;
	}

	return KV_STATUS_ERROR;
}

void InitKvFtl() 
{
	unsigned int idx;
	unsigned int slot;

	kvIndexTablePtr = (P_KV_INDEX_TABLE)KV_INDEX_TABLE_ADDR;

	for (idx = 0; idx < KV_HASH_TABLE_SIZE; ++idx) {
		kvIndexTablePtr->entry[idx].key = 0;
		kvIndexTablePtr->entry[idx].valueLba = 0;
		kvIndexTablePtr->entry[idx].valueLength = 0;
	}

	for (slot = 0; slot < KV_MAX_CMD_SLOTS; ++slot) {
		kvPendingGetLength[slot] = 0;	
	}

	kvNextValueLba = 0;
	kvStoredKeyCount = 0;
}

unsigned int KvPut(unsigned int cmdSlotTag, unsigned int key, unsigned int valueLength)
{
	unsigned int slot;
	unsigned int found;
	unsigned int status;
	unsigned int valueLba;

	if (valueLength > KV_VALUE_SIZE)
		return KV_STATUS_ERROR;

	status = KvFindSlot(key, &slot, &found);
	if (status != KV_STATUS_OK)
		return KV_STATUS_ERROR;

	if (found) {
		valueLba = kvIndexTablePtr->entry[slot].valueLba;
	} else {
		if (kvNextValueLba >= storageCapacity_L)
			return KV_STATUS_ERROR;

		if (kvStoredKeyCount >= KV_MAX_STORED_KEYS)
			return KV_STATUS_ERROR;

		valueLba = kvNextValueLba;
		kvNextValueLba++;
		kvStoredKeyCount++;
	}

	kvIndexTablePtr->entry[slot].key = key;
	kvIndexTablePtr->entry[slot].valueLba = valueLba;
	kvIndexTablePtr->entry[slot].valueLength = valueLength;

	ReqTransNvmeToSlice(cmdSlotTag, valueLba, 0, IO_NVM_WRITE);

	return KV_STATUS_OK;
}

unsigned int KvGet(unsigned int cmdSlotTag, unsigned int key, unsigned int hostBufferLength)
{
	unsigned int slot;
	unsigned int found;
	unsigned int status;
	unsigned int valueLba;
	unsigned int valueLength;

	status = KvFindSlot(key, &slot, &found);
	if (status != KV_STATUS_OK || !found)
		return KV_STATUS_NO_SUCH_KEY;

	valueLength = kvIndexTablePtr->entry[slot].valueLength;

	if (hostBufferLength < valueLength)
		return KV_STATUS_ERROR;

	if (cmdSlotTag >= KV_MAX_CMD_SLOTS)
		return KV_STATUS_ERROR;

	valueLba = kvIndexTablePtr->entry[slot].valueLba;
	kvPendingGetLength[cmdSlotTag] = valueLength;
	ReqTransNvmeToSlice(cmdSlotTag, valueLba, 0, IO_NVM_READ);

	return KV_STATUS_OK;
}

unsigned int KvHasPendingGetCompletion(unsigned int cmdSlotTag)
{
	if (cmdSlotTag >= KV_MAX_CMD_SLOTS)
		return 0;

	return (kvPendingGetLength[cmdSlotTag] != 0);
}

unsigned int KvConsumePendingGetCompletion(unsigned int cmdSlotTag, unsigned int *valueLength)
{
	if (cmdSlotTag >= KV_MAX_CMD_SLOTS)
		return 0;

	if (kvPendingGetLength[cmdSlotTag] == 0)
		return 0;

	*valueLength = kvPendingGetLength[cmdSlotTag];
	kvPendingGetLength[cmdSlotTag] = 0;

	return 1;
}
