//  ***************************************************************
//  e2block.c - Creation date: 30/11/2022
//  -------------------------------------------------------------
//  NanoShell Copyright (C) 2022 - Licensed under GPL V3
//
//  ***************************************************************
//  Programmer(s):  iProgramInCpp (iprogramincpp@gmail.com)
//  ***************************************************************
#include <vfs.h>
#include <ext2.h>

//Assumes stuff has been prepared in Ext2FlushBlockGroupDescriptor. Don't use.
static void Ext2FlushBlockGroupDescriptorInternal(Ext2FileSystem* pFS, uint32_t blockGroupIndex, uint32_t blocksToWrite, void* pMem)
{
	uint32_t bgdtStart;
	if (blockGroupIndex == 0)
		bgdtStart = (pFS->m_blockSize == 1024) ? 2 : 1;
	else
		bgdtStart = 2 + pFS->m_blocksPerGroup * blockGroupIndex;
	
	ASSERT(Ext2WriteBlocks(pFS, bgdtStart, blocksToWrite, pMem) == DEVERR_SUCCESS);
}

void Ext2FlushBlockGroupDescriptor(Ext2FileSystem *pFS, UNUSED uint32_t bgdIndex)
{
	uint8_t* pMem = (uint8_t*)pFS->m_pBlockGroups;
	uint32_t blocksToWrite = (pFS->m_blockGroupCount * sizeof(Ext2BlockGroupDescriptor) + pFS->m_blockSize - 1) >> pFS->m_log2BlockSize;
	
	Ext2FlushBlockGroupDescriptorInternal(pFS, 0, blocksToWrite, pMem);
	
	// If the file system has SPARSE_SUPER enabled...
	if (pFS->m_superBlock.m_readOnlyFeatures & E2_ROF_SPARSE_SBLOCKS_AND_GDTS)
	{
		// The groups chosen are 0, 1, and powers of 3, 5, and 7. We've already written zero.
		
		Ext2FlushBlockGroupDescriptorInternal(pFS, 1, blocksToWrite, pMem);
		for (uint32_t blockGroupNo = 3; blockGroupNo < pFS->m_blockGroupCount; blockGroupNo *= 3) Ext2FlushBlockGroupDescriptorInternal(pFS, blockGroupNo, blocksToWrite, pMem);
		for (uint32_t blockGroupNo = 5; blockGroupNo < pFS->m_blockGroupCount; blockGroupNo *= 5) Ext2FlushBlockGroupDescriptorInternal(pFS, blockGroupNo, blocksToWrite, pMem);
		for (uint32_t blockGroupNo = 7; blockGroupNo < pFS->m_blockGroupCount; blockGroupNo *= 7) Ext2FlushBlockGroupDescriptorInternal(pFS, blockGroupNo, blocksToWrite, pMem);
	}
	else
	{
		for (uint32_t blockGroupNo = 1; blockGroupNo < pFS->m_blockGroupCount; blockGroupNo++)
		{
			Ext2FlushBlockGroupDescriptorInternal(pFS, blockGroupNo, blocksToWrite, pMem);
		}
	}
}

void Ext2FlushBlockBitmap(Ext2FileSystem *pFS, UNUSED uint32_t bgdIndex)
{
	uint32_t* pData = (uint32_t*)&pFS->m_pBlockBitmapPtr[(bgdIndex * pFS->m_blocksPerBlockBitmap) << pFS->m_log2BlockSize];
	
	ASSERT(Ext2WriteBlocks(pFS, pFS->m_pBlockGroups[bgdIndex].m_blockAddrBlockUsageBmp, pFS->m_blocksPerBlockBitmap, pData) == DEVERR_SUCCESS);
}

uint32_t Ext2AllocateBlock(Ext2FileSystem *pFS, uint32_t hint)
{
	// Look for a free BG descriptor with at least one free block.
	uint32_t freeBGD = ~0u;
	
	uint32_t hintBG = (hint - pFS->m_superBlock.m_firstDataBlock) / pFS->m_blocksPerGroup;
	
	for (uint32_t i = hintBG; i < pFS->m_blockGroupCount; i++)
	{
		Ext2BlockGroupDescriptor *pBG = &pFS->m_pBlockGroups[i];
		
		if (pBG->m_nUnallocatedBlocks > 0)
		{
			freeBGD = i;
			break;
		}
	}
	
	if (freeBGD == ~0u)
	{
		for (uint32_t i = 0; i < hintBG; i++)
		{
			Ext2BlockGroupDescriptor *pBG = &pFS->m_pBlockGroups[i];
			
			if (pBG->m_nUnallocatedBlocks > 0)
			{
				freeBGD = i;
				break;
			}
		}
		
		// If we don't have any such things, return.
		if (freeBGD == ~0u) return ~0u;
	}
	
	Ext2BlockGroupDescriptor *pBG = &pFS->m_pBlockGroups[freeBGD];
	
	// Look for a free block inside the block group.
	uint32_t entriesPerBlock = pFS->m_blockSize * pFS->m_blocksPerBlockBitmap / sizeof(uint32_t); // 32-bit entries.
	uint32_t* pData = (uint32_t*)&pFS->m_pBlockBitmapPtr[(freeBGD * pFS->m_blocksPerBlockBitmap) << pFS->m_log2BlockSize];
	
	//SLogMsg("thing: %d, freeBGD: %d, blocksperblockbitmap: %d", (freeBGD * pFS->m_blocksPerBlockBitmap) << pFS->m_log2BlockSize, freeBGD, pFS->m_blocksPerBlockBitmap);
	
	for (uint32_t k = 0; k < entriesPerBlock; k++)
	{
		// if all the blocks here are allocated....
		if (pData[k] == ~0u)
			continue;
		
		for (uint32_t l = 0; l < 32; l++)
		{
			if (pData[k] & (1 << l)) continue;
			
			// Set the bit in the bitmap and return.
			pData[k] |= (1 << l);
			
			// Flush the bitmap.
			//ASSERT(Ext2WriteBlocks(pFS, pBG->m_blockAddrBlockUsageBmp + j, 1, pFS->m_pBlockBuffer) == DEVERR_SUCCESS);
			Ext2FlushBlockBitmap(pFS, freeBGD);
			
			// Update the free blocks.
			pBG->m_nUnallocatedBlocks--;
			Ext2FlushBlockGroupDescriptor(pFS, freeBGD);
			
			// Update the superblock.
			pFS->m_superBlock.m_nUnallocatedBlocks--;
			Ext2FlushSuperBlock(pFS);
			
			return pFS->m_superBlock.m_firstDataBlock + freeBGD * pFS->m_blocksPerGroup + k * 32 + l;
		}
	}
	
	// Maybe this entry was faulty. well, that's the problem of the driver that wrote this...
	// TODO: Don't just bail out if we have such a faulty thing.
	LogMsg("ERROR: Block group descriptor whose m_nUnallocatedBlocks is %d actually lied and there are no blocks inside! An ``e2fsck'' MUST be performed.", pBG->m_nUnallocatedBlocks);
	return ~0u;
}

void Ext2BlockBitmapCheck(Ext2FileSystem* pFS, uint32_t blockNo, bool bWrite, bool* pResultInOut)
{
	blockNo -= pFS->m_superBlock.m_firstDataBlock; //since block numbers start at 1
	
	uint32_t bitsPerBlock = pFS->m_blockSize * 8;
	uint32_t blockGroup = blockNo / pFS->m_blocksPerGroup, indexInsideBG = blockNo % pFS->m_blocksPerGroup;
	//uint32_t blocksPerBlockBitmap = (pFS->m_superBlock.m_blocksPerGroup + bitsPerBlock - 1) / bitsPerBlock;
	uint32_t blockThisBitIsIn = indexInsideBG / bitsPerBlock, indexInsideThatBlock = indexInsideBG % bitsPerBlock;
	
	Ext2BlockGroupDescriptor* pBG = &pFS->m_pBlockGroups[blockGroup];
	
	// Read the block.
	ASSERT(Ext2ReadBlocks(pFS, pBG->m_blockAddrBlockUsageBmp + blockThisBitIsIn, 1, pFS->m_pBlockBuffer) == DEVERR_SUCCESS);
	
	uint32_t* data = (uint32_t*)pFS->m_pBlockBuffer;
	uint32_t thingIndex = indexInsideThatBlock / 32, thingBit = indexInsideThatBlock % 32;
	
	if (bWrite)
	{
		if (*pResultInOut)
			data[thingIndex] |= (1 << thingBit);
		else
			data[thingIndex] &=~(1 << thingBit);
		
		ASSERT(Ext2WriteBlocks(pFS, pBG->m_blockAddrBlockUsageBmp + blockThisBitIsIn, 1, pFS->m_pBlockBuffer) == DEVERR_SUCCESS);
	}
	else
	{
		*pResultInOut = (data[thingIndex] & (1 << thingBit)) == 0;
	}
}

bool Ext2CheckBlockFree(Ext2FileSystem* pFS, uint32_t blockNo)
{
	bool result = false;
	Ext2BlockBitmapCheck(pFS, blockNo, false, &result);
	return result;
}

void Ext2CheckBlockMarkFree(Ext2FileSystem* pFS, uint32_t blockNo)
{
	bool result = false;
	Ext2BlockBitmapCheck(pFS, blockNo, true, &result);
}

void Ext2CheckBlockMarkUsed(Ext2FileSystem* pFS, uint32_t blockNo)
{
	bool result = true;
	Ext2BlockBitmapCheck(pFS, blockNo, true, &result);
}

void Ext2FreeBlock(Ext2FileSystem *pFS, uint32_t blockNo)
{
	//SLogMsg("Ext2FreeBlock(%x)", blockNo);
	Ext2CheckBlockMarkFree(pFS, blockNo);
}