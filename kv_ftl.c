#include "kv_ftl.h"
#include "memory_map.h"
#include "nvme/nvme.h"
#include "request_transform.h"

P_KV_INDEX_TABLE kvIndexTablePtr;

static unsigned int kvNextValueLba;
static unsigned int kvPendingGetLength[KV_MAX_CMD_SLOTS];

void InitKvFtl()
{
	unsigned int key;
	unsigned int slot;

	kvIndexTablePtr = (P_KV_INDEX_TABLE)KV_INDEX_TABLE_ADDR;

	for(key = 0; key < KV_MAX_KEYS; key++)
	{
		kvIndexTablePtr->entry[key].valueLba = 0;
		kvIndexTablePtr->entry[key].valueLength = 0;
	}

	for(slot = 0; slot < KV_MAX_CMD_SLOTS; slot++)
		kvPendingGetLength[slot] = 0;

	kvNextValueLba = 0;
}

unsigned int KvPut(unsigned int cmdSlotTag, unsigned int key, unsigned int valueLength)
{
	unsigned int valueLba;

	if(key >= KV_MAX_KEYS)
		return KV_STATUS_ERROR;

	if(valueLength == 0 || valueLength > KV_VALUE_SIZE)
		return KV_STATUS_ERROR;

	if(kvNextValueLba >= storageCapacity_L)
		return KV_STATUS_ERROR;

	valueLba = kvNextValueLba;
	kvNextValueLba++;

	kvIndexTablePtr->entry[key].valueLba = valueLba;
	kvIndexTablePtr->entry[key].valueLength = valueLength;

	ReqTransNvmeToSlice(cmdSlotTag, valueLba, 0, IO_NVM_WRITE);

	return KV_STATUS_OK;
}

unsigned int KvGet(unsigned int cmdSlotTag, unsigned int key, unsigned int hostBufferLength)
{
	unsigned int valueLba;
	unsigned int valueLength;

	if(key >= KV_MAX_KEYS)
		return KV_STATUS_NO_SUCH_KEY;

	valueLength = kvIndexTablePtr->entry[key].valueLength;
	if(valueLength == 0)
		return KV_STATUS_NO_SUCH_KEY;

	if(hostBufferLength < valueLength)
		return KV_STATUS_ERROR;

	if(cmdSlotTag >= KV_MAX_CMD_SLOTS)
		return KV_STATUS_ERROR;

	valueLba = kvIndexTablePtr->entry[key].valueLba;
	kvPendingGetLength[cmdSlotTag] = valueLength;
	ReqTransNvmeToSlice(cmdSlotTag, valueLba, 0, IO_NVM_READ);

	return KV_STATUS_OK;
}

unsigned int KvHasPendingGetCompletion(unsigned int cmdSlotTag)
{
	if(cmdSlotTag >= KV_MAX_CMD_SLOTS)
		return 0;

	return kvPendingGetLength[cmdSlotTag] != 0;
}

unsigned int KvConsumePendingGetCompletion(unsigned int cmdSlotTag, unsigned int *valueLength)
{
	if(cmdSlotTag >= KV_MAX_CMD_SLOTS)
		return 0;

	if(kvPendingGetLength[cmdSlotTag] == 0)
		return 0;

	*valueLength = kvPendingGetLength[cmdSlotTag];
	kvPendingGetLength[cmdSlotTag] = 0;

	return 1;
}
