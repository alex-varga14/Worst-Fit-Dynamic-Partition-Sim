#include "memsim.h"
#include <cassert>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <list>
#include <set>

struct Partition
{
	int tag;
	int64_t size, addr;
	//Partition constructor
	Partition(int64_t addr, int64_t size, int tag)
	{
		this->addr = addr;
		this->size = size;
		this->tag = tag;
	}
};

class Simulator
{
	public:
		//iterator to iterate over list of partitions
		typedef std::list<Partition>::iterator PartitionRef;
		
		struct scmp 
		{
			bool operator()(const PartitionRef & c1, const PartitionRef & c2) const 
			{
				if (c1->size == c2->size)
				return c1->addr < c2->addr;
				else
				return c1->size > c2->size;
			}
		};
		// all partitions, in a linked list
		std::list<Partition> all_blocks;
		// sorted partitions by size/address
		std::set<PartitionRef,scmp> free_blocks;
		// quick access to all tagged partitions
		std::unordered_map<long, std::vector<PartitionRef>> tagged_blocks;
		// int to store the page size being simulated
		int64_t page_size;
		// iterator to hold position of worst fit partition 
		PartitionRef worst_fit_iterator;
		//MemSimResult to hold stats: requested pages etc
		MemSimResult result;
		
		//Constructor based on mem_sim function
		Simulator(int64_t page_size);
		//function to add a Partition to all_blocks partition list and its tag to tagged_blocks map
		void addPartition(Partition p);
		//helper function to set up partition if its the first allocation, requests new pages
		void partition_empty(int64_t size);
		// helper function to allocate new pages if request is larger than available space
		int64_t new_pages_needed(int64_t request);
		
		void allocate(int tag, int size);
			
		void deallocate(int tag);
		
		MemSimResult getStats();
		void check_consistency();
};

void Simulator::check_consistency()
{
	// make sure the sum of all partition sizes in your linked list is
	// the same as number of page requests * page_size
	PartitionRef it;
	int64_t sum_partition_sizes = 0;
	for(it = all_blocks.begin(); it != all_blocks.end(); ++it)
	{
		sum_partition_sizes += it->size;
	}
	assert(sum_partition_sizes == (this->result.n_pages_requested * this->page_size));

	// make sure your addresses are correct

	// make sure the number of all partitions in your tag data structure +
	// number of partitions in your free blocks is the same as the size
	// of the linked list
	// if(!this->all_blocks.empty())
	// {
		// int64_t all_partitions = (this->tagged_blocks.size() + this->free_blocks.size());
		// assert(all_partitions == this->all_blocks.size());
	// }

	// make sure that every free partition is in free blocks
	
	// make sure that every partition in free_blocks is actually free
	std::set<PartitionRef, scmp>::iterator iter;
	for(iter = this->free_blocks.begin(); iter != this->free_blocks.end(); ++iter)
	{
		assert((*iter)->tag == 0);
	}

	// make sure that none of the partition sizes or addresses are < 1
	// PartitionRef iter1;
	// for(iter1 = all_blocks.begin(); iter1 != all_blocks.end(); ++iter1)
	// {
	// }
}

Simulator::Simulator(int64_t page_size)
{
	//set page size to input page size, instantiate list of partitions and map to store tag's
	this->result.n_pages_requested = 0;
	this->page_size = page_size;
	this->all_blocks = std::list<Partition>();
	this->tagged_blocks = std::unordered_map<long, std::vector<PartitionRef>>();
}

//Helper function is all_blocks is empty, requests necessary pages and pushes first partition into list and declares it free
void Simulator::partition_empty(int64_t size)
{
	//determine how many pages are needed to allocate desired memory
	int64_t pages = this->new_pages_needed(size);
	//add requested pages for stats
	this->result.n_pages_requested += pages;
	//push back first partition, addr = page size * pages needed to fulfil allocation
	this->all_blocks.push_back(Partition(0, this->page_size * pages, 0));
	//push partition to free_blocks
	this->free_blocks.insert(this->all_blocks.begin());
}
//Helper function to allocate new pages based on request size
int64_t Simulator::new_pages_needed(int64_t request)
{
	// if requested allocation is divisible by page size, return exact amount of pages necessary
	// else request larger to whole page size, must allocate extra page
	if((request % this->page_size) == 0)
		return request / this->page_size;
	else
		return 1 + request/ this->page_size;
}
//helper function to add a partition to list
void Simulator::addPartition(Partition p)
{
	this->all_blocks.push_back(p);
	this->free_blocks.insert(prev(this->all_blocks.end()));
}

void Simulator::allocate(int tag, int size)
{
	//bool flag to store if allocation request can fit within a "hole" partition in free_blocks
	bool worst_fit_available = false;
	//Check to see if this is the first 
	if(all_blocks.empty()) 
		this->partition_empty(size);
	//if free paritions not empty, determine if size of allocation fits within available free partition, if not ask the OS for more memory
	if(!this->free_blocks.empty()) 
	{
		//find largest partition that fits requested size, free_blocks.begin() always gives largest partition
		// in case of ties, gives the partition with the smallest address
		std::set<PartitionRef, scmp>::iterator it = this->free_blocks.begin();
	    if((*it)->size >= size) 
		{
			//set flag for worst fit found and skip adding pages
			worst_fit_available = true;
			//set iterator to worst_fit partition
			this->worst_fit_iterator = *it;
			//erase the worst_fit partition from free blocks
			this->free_blocks.erase(it);
		}
	}
	//New allocation does not fit within a free_block partition
	if(!worst_fit_available) 
	{
	    //No free blocks, add memory at end of parition list
	    PartitionRef last_block = prev(this->all_blocks.end());
	    //If last partition is free, i.e tag == 0
	    if(last_block->tag == 0) 
	    {
			// determine size of last partition 
			int64_t last_size = last_block->size;
			// add pages for allocation and sum requested for stats
			last_size += this->new_pages_needed(size - last_block->size) * this->page_size;
			this->result.n_pages_requested += this->new_pages_needed(size - last_block->size);
			//find paritition to begin free'ing memory from
			std::set<PartitionRef, scmp>::iterator it = this->free_blocks.find(last_block);
			if(it != this->free_blocks.end()) 
				this->free_blocks.erase(it);
			//set partitions new allocation tag and size
			last_block->tag = tag;
			last_block->size = size;
			//push back partition and tag into tagged_blocks
			this->tagged_blocks[tag].push_back(last_block);
			//determine how mant bytes remain in requested page once new partition has been allocated
			last_size -= size;
			// If newly allocated pages overflow, too large, append new free partition of overflowed size to end of partitions list
			// add to partition list and add to free blocks
			if(last_size > 0) 
				addPartition(Partition(last_block->addr + last_block->size, last_size, 0));
	    }
	    //last partition is not free, tag != 0
		else 
		{
			//Create new partition at the end request new page(s)
			this->result.n_pages_requested += this->new_pages_needed(size);
			// add new partition to list and tag to tagged_blocks
			this->all_blocks.push_back(Partition(last_block->addr + last_block->size, size, tag));
			this->tagged_blocks[tag].push_back(prev(this->all_blocks.end()));
			// if newly requested page size larger than necessary allocation, append overflowed partition to end
			// add to partition list and add to free blocks
			if((this->new_pages_needed(size) * this->page_size - size) > 0) 
				addPartition(Partition(last_block->addr + last_block->size + size, this->new_pages_needed(size) * this->page_size - size, 0));
	    }
	} 
	//Allocation request fits within one worst-fit partition free block
	else 
	{
		int64_t worst_fit_size = this->worst_fit_iterator->size - size;
		// set new partition tag and size from allocation
		this->worst_fit_iterator->tag = tag;
		this->worst_fit_iterator->size = size;
		//push newly tagged partition to tagged_blocks
		this->tagged_blocks[tag].push_back(this->worst_fit_iterator);
		// if size larger than necessary allocation
		if(worst_fit_size > 0) 
		{
			//add remaining parition at index of worst_fit_iterator, insert remaining partition addr to the worst_fit_iterator addr + size of newly allocated mem
			this->all_blocks.insert(next(this->worst_fit_iterator), Partition(this->worst_fit_iterator->addr + size, worst_fit_size, 0));
			this->free_blocks.insert(next(this->worst_fit_iterator));
		}
	}
}

void Simulator::deallocate(int tag)
{
	//populate array of partition iterators pointing to all blocks with tag for deallocation
	std::vector<PartitionRef> all_blocks = this->tagged_blocks[tag];
	//for every parition with tag to deallocate
	for(std::vector<PartitionRef>::iterator iter = all_blocks.begin(); iter != all_blocks.end(); ++iter)
	{
		//matching tag
		if((*iter)->tag == tag)
		{
		    //mark partition free
		    (*iter)->tag = 0;
		  
			//Must determine if paritions must be merged
			// - 2-way merge with partition ahead or behind tagged partition 
			// - 3-day merge with both partitions ahead and behind tagged
			// Terminating X
			// cases: | A | X | B | --> | A | free | B | 
			//	      | A | X | free | --> | A |  free  | 
			//        | free | X | B | --> |   free   | B |
			//        | free | X | free | --> |    free    |
			
			// check if partition B is free
			if(((*iter) != prev(this->all_blocks.end())) && (next((*iter))->tag == 0)) 
			{
				//set iterator to partition B
				PartitionRef B = next((*iter));
				//no need to change addr but append size of B 
				(*iter)->size += B->size;
				//erase the B partition once merged
				this->all_blocks.erase(B);
				//find paritition to begin free'ing memory from
				std::set<PartitionRef, scmp>::iterator it = this->free_blocks.find(B);
				if(it != this->free_blocks.end()) 
					this->free_blocks.erase(it);
			}
			// check if partition A is free
			if(((*iter) != this->all_blocks.begin()) && (prev((*iter))->tag == 0)) 
			{
				//set iterator to partition A
				PartitionRef A = prev((*iter));
				//set partition as A address and append A partition's size 
				(*iter)->addr = A->addr;
				(*iter)->size += A->size;
				//erase the A block once merged
				this->all_blocks.erase(A);
				//find paritition to begin free'ing memory from
				std::set<PartitionRef, scmp>::iterator it = this->free_blocks.find(A);
				if(it != this->free_blocks.end()) 
					this->free_blocks.erase(it);		
		    }
		    //insert newly de-allocated partition into free_blocks 
		    this->free_blocks.insert((*iter));
		}
    }
	//erase the de-allocated tag from tagged_blocks
	this->tagged_blocks.erase(tag);
}

MemSimResult Simulator::getStats()
{
    if(this->free_blocks.empty()) 
	{
		//free_blocks empty, therefore 0 for both free stats
		this->result.max_free_partition_size = 0;
		this->result.max_free_partition_address = 0;
    } 
	else 
	{
		//free_blocks.begin always points to largest free partition
		std::set<PartitionRef, scmp>::iterator it = this->free_blocks.begin();
		this->result.max_free_partition_address = (*it)->addr;
		this->result.max_free_partition_size = (*it)->size;
    }
    return this->result;
}
	
MemSimResult mem_sim(int64_t page_size, const std::vector<Request> & requests)
{
  Simulator sim(page_size);
  for (const auto & req : requests) 
  {
    if (req.tag < 0)
      sim.deallocate(-req.tag);
	else 
      sim.allocate(req.tag, req.size);
    //sim.check_consistency();
  }
  return sim.getStats();
}