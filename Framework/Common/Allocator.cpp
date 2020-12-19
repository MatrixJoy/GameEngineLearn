#include "Allocator.h"
#include <cassert>
#include <cstring>

#ifndef ALIGN
#define ALIGN(x, a)	(((x)+((a)-1)) & ~((a)-1))
#endif

using namespace My;

My::Allocator::Allocator(size_t dataSize, size_t pageSize, size_t alignment)
	: m_pPageList(nullptr),m_pFreeList(nullptr) {
		Reset(dataSize,pageSize,alignment);
	}

My::Allocator::~Allocator(){
	FreeAll();
}

void My::Allocator::Reset(size_t dataSize, size_t pageSize, size_t alignment){
	FreeAll();
	m_szDataSize = dataSize;
	m_szPageSize = pageSize;
	size_t minimal_size = (sizeof(BlockHeader) > m_szDataSize) ? sizeof(BlockHeader) : m_szDataSize;
	// this magic only works when alignment is 2^n, which should general be the case
	// because most CPU/GPU also requires the aligment be in 2^n
	// but still we use a assert to guarantee it
#if defined(_DEBUG)
	assert(alignment > 0 && ((alignment & (alignment-1)))==0);
#endif
	m_szBlockSize = ALIGN(minimal_size, alignment);
	m_szAlignmentSize = m_szBlockSize - minimal_size;
	m_nBlocksPerPage = (m_szPageSize -sizeof(PageHeader))/m_szBlockSize;
}

void* My::Allocator::Allocate(){
	// free list empty, create new page
	if (!m_pFreeList)
	{
		// allocate new page
		PageHeader* newPage = reinterpret_cast<PageHeader *> (new uint8_t[m_szPageSize]);
		++m_nPages;
		m_nBlocks += m_nBlocksPerPage;
		m_nFreeBlocks += m_nBlocksPerPage;
#if defined(_DEBUG)
		FillFreePage(newPage);
#endif

		// page list not empty, link new page
		if (m_pPageList)
		{
			newPage->pNext = m_pPageList;
		}

		// push new page
		m_pPageList = newPage;

		// link new free blocks
		BlockHeader* currBlock = newPage->Blocks();
		for (unsigned i = 0; i < m_nBlocksPerPage - 1; ++i)
		{
			currBlock->pNext = NextBlock(currBlock);
			currBlock = NextBlock(currBlock);
		}
		currBlock->pNext = nullptr; // last block

		// push new blocks
		m_pFreeList = newPage->Blocks();
	}

	// pop free block
	BlockHeader* freeBlock = m_pFreeList;
	m_pFreeList = m_pFreeList->pNext;
	--m_nFreeBlocks;
#if defined(_DEBUG)
	FillAllocatedBlock(freeBlock);
#endif
	return reinterpret_cast<void*>(freeBlock);
}

void My::Allocator::Free(void* p){
	BlockHeader* block = reinterpret_cast<BlockHeader*>(p);
#if defined(_DEBUG)
	FillFreeBlock(block);
#endif
	block->pNext=m_pFreeList;
	m_pFreeList=block;
	++m_nFreeBlocks;
}

void My::Allocator::FreeAll(){
	PageHeader* pPage = m_pPageList;
	while(pPage){
		PageHeader* _p = pPage;
		pPage=pPage->pNext;
		delete[] reinterpret_cast<uint8_t*>(_p);
	}
	m_pPageList = nullptr;
	m_pFreeList = nullptr;
	m_nPages = 0;
	m_nBlocks = 0;
	m_nFreeBlocks = 0;
}

#if defined(_DEBUG)
void My::Allocator::FillFreePage(PageHeader *pPage){
	// page header
	pPage->pNext = nullptr;

	// blocks
	BlockHeader *pBlock = pPage->Blocks();
	for (uint32_t i = 0; i < m_nBlocksPerPage; i++){
		FillFreeBlock(pBlock);
		pBlock = NextBlock(pBlock);
	}
}

void My::Allocator::FillFreeBlock(BlockHeader *pBlock){
	// block header + data
	std::memset(pBlock, PATTERN_FREE, m_szBlockSize - m_szAlignmentSize);

	// alignment
	std::memset(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize - m_szAlignmentSize,
			PATTERN_ALIGN, m_szAlignmentSize);
}

void My::Allocator::FillAllocatedBlock(BlockHeader *pBlock){
	// block header + data
	std::memset(pBlock, PATTERN_ALLOC, m_szBlockSize - m_szAlignmentSize);

	// alignment
	std::memset(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize - m_szAlignmentSize,
			PATTERN_ALIGN, m_szAlignmentSize);
}
#endif

My::BlockHeader* My::Allocator::NextBlock(BlockHeader *pBlock){
	return reinterpret_cast<BlockHeader *>(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize);
}
