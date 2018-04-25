#include "game.h"

namespace {

//TODO: template this class on whether it's hint/solve/solve_another/difficulty
//template<bool
class solver {
	//design goal: be able to solve all 100*100 maps, in 1MB RAM or less (including stack and gamemap)
	//1MB / 10000 = 100 bytes per tile
	//usage:
	//- gamemap: 1+1+2+4+4 = 12 bytes per tile
	//- gamemap finished() buffers (can be moved to stack if necessary): 1+2 = 3 bytes per tile
	//- nextjoined: 2 bytes per tile
	//- possibilities: 20 bytes per tile
	//- links: 16 bytes per tile
	//- sum: 12+3+2+20+16 = 53 bytes per tile
	//anything that's not per tile (including the stack) fits in the 48576 bytes left in the megabyte
	
	gamemap& map;
	
	//a linked list of all tiles joined with this one, in arbitrary order, to allow iterating through them
	//unspecified value for ocean tiles
	uint16_t nextjoined[100*100];
	
	// the 1<<0 bit means 'it's possible that 0 bridges exit this tile to the right'
	// 1<<(1,2,3) means up, left and down, respectively
	// the 1<<(4..7) bits mean it's possible to have 1 bridge in that direction
	// 1<<(8..11) means 2 bridges
	// 1<<(12..15) means joined with that island; if set, 0 1 and 2 are banned in that direction
	// ocean tiles have this set too; for them, left and right are always equal, as are up/down
	uint16_t possibilities[10][100*100];
	
	
	//This is similar to union-find, but not exactly. The operations I need are slightly different.
	struct link_t {
		uint16_t links[4]; // Available links, or -1. May point to a node with the same root.
		
		uint16_t join_root; // Points towards the root node of any joined set. May not necessarily be the actual root.
		uint16_t join_next; // This creates a linked list of joined nodes. The last one points to the root.
		
		// These two are only used by the root node. For others, value is unspecified and probably outdated.
		uint16_t join_firstneighbor; // Points to the first joined node to have neighbors.
		uint16_t join_last; // Points to the last node in the linked list.
		
		//TODO: union some of these
	};
	link_t links[100*100];
	uint16_t n_linkable_islands;
	uint16_t a_linkable_island; // Points to any one island that does not have population 1.
	
	void link_init()
	{
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t index = y*100+x;
			gamemap::island& here = map.map[y][x];
			link_t& lhere = links[index];
			
			if (here.population >= 0)
			{
				//lhere.links[0] = (here.bridgelen[0]>=1 ? index + here.bridgelen[0]     : -1);
				//lhere.links[1] = (here.bridgelen[1]>=1 ? index - here.bridgelen[1]*100 : -1);
				//lhere.links[2] = (here.bridgelen[2]>=1 ? index - here.bridgelen[2]     : -1);
				//lhere.links[3] = (here.bridgelen[3]>=1 ? index + here.bridgelen[3]*100 : -1);
				uint16_t flags = possibilities[0][index];
				lhere.links[0] = (flags&0x1110 ? index + here.bridgelen[0]     : -1);
				lhere.links[1] = (flags&0x2220 ? index - here.bridgelen[1]*100 : -1);
				lhere.links[2] = (flags&0x4440 ? index - here.bridgelen[2]     : -1);
				lhere.links[3] = (flags&0x8880 ? index + here.bridgelen[3]*100 : -1);
				
				lhere.join_root = index;
				lhere.join_next = index;
				lhere.join_firstneighbor = index;
				lhere.join_last = index;
			}
			//else the island will never be touched by these functions
		}
		
		//drop pop-1 nodes
		n_linkable_islands = 0;
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t index = y*100+x;
			gamemap::island& here = map.map[y][x];
			link_t& lhere = links[index];
			
			if (here.population == 1)
			{
printf("UNREACHABLE:%.3i\n",index);
				for (int i=0;i<4;i++)
				{
					if (lhere.links[i] != (uint16_t)-1)
					{
printf("UNLINK:%.3i,%.3i\n",index,lhere.links[i]);
						links[lhere.links[i]].links[i^2] = -1;
						lhere.links[i] = -1;
					}
				}
			}
			if (here.population > 1)
			{
				a_linkable_island = index;
				n_linkable_islands++;
			}
		}
		
		//TODO: castles
	}
	uint16_t link_root(uint16_t index)
	{
		uint16_t root = index;
		while (root != links[root].join_root)
		{
			root = links[root].join_root;
		}
		
		uint16_t ni = index;
		while (ni != root)
		{
			uint16_t tmp = links[ni].join_root;
			links[ni].join_root = root;
			ni = tmp;
		}
		
		return root;
	}
	
	void link_join(uint16_t a, uint16_t b)
	{
		//link_t& la = links[a];
		//link_t& lb = links[b];
		uint16_t r1 = link_root(a);
		uint16_t r2 = link_root(b);
		
		if (r1 == r2) return;
		
		link_t& lr1 = links[r1];
		link_t& lr2 = links[r2];
		
		links[lr1.join_last].join_next = r2;
		links[lr2.join_last].join_next = r1;
		lr1.join_last = lr2.join_last;
		lr2.join_root = r1;
	}
	//Returns any link from 'index', except the first link leading to 'other' is excluded.
	//If there are two links to 'other', one can be returned.
	//Always returns the same output for the same input. Does not necessarily return links to root nodes.
	uint16_t link_query(uint16_t index, uint16_t other)
	{
		if (other != (uint16_t)-1) other = link_root(other);
		uint16_t root = link_root(index);
		uint16_t ni = links[root].join_firstneighbor;
		do {
			link_t& link = links[ni];
			
			for (int i=0;i<4;i++)
			{
				if (link.links[i] != (uint16_t)-1)
				{
					uint16_t targetroot = link_root(link.links[i]);
					if (targetroot == root)
						link.links[i] = -1;
					else if (targetroot == other)
						other = -1;
					else
						return link.links[i];
				}
			}
			
			ni = link.join_next;
			//if (a == -1) links[root].join_firstneighbor = ni;
		} while (ni != root);
		return -1;
	}
	//Given 'link', which is a return value from link_query, and 'orig', the input to link_query,
	// tells which bridge on the original map that corresponds to, and in which direction.
	//If there are multiple possible bridges between those two roots, returns an arbitrary one.
	void link_lookback(uint16_t link, uint16_t orig, uint16_t& topleft, bool& down)
	{
		gamemap::island& here = map.get(link);
		orig = link_root(orig);
		
		if (here.bridgelen[0] != -1 && link_root(link + here.bridgelen[0]) == orig)
		{
			topleft = link;
			down = false;
			return;
		}
		
		if (here.bridgelen[1] != -1 && link_root(link - here.bridgelen[1]*100) == orig)
		{
			topleft = link - here.bridgelen[1]*100;
			down = true;
			return;
		}
		
		if (here.bridgelen[2] != -1 && link_root(link - here.bridgelen[2]) == orig)
		{
			topleft = link - here.bridgelen[2];
			down = false;
			return;
		}
		
		if (here.bridgelen[3] != -1 && link_root(link + here.bridgelen[3]*100) == orig)
		{
			topleft = link;
			down = true;
			return;
		}
		
		abort(); // unreachable
	}
	
	
	//Can only block possibilities, never allow anything new.
	void set_state(uint16_t index, uint16_t newstate)
	{
		uint16_t prevstate = possibilities[0][index];
		
//printf("\n%.3i,%.4x,%.4x",index,prevstate,newstate);
		
		int16_t offsets[4] = { 1, -100, -1, 100 };
		
		for (int dir=0;dir<4;dir++)
		{
			int16_t offset = offsets[dir];
			uint16_t src_bits = ((prevstate^newstate) & (0x0111<<dir));
			uint16_t dst_bits = (dir&2) ? (src_bits>>2) : (src_bits<<2);
			if ((prevstate^newstate) & (0x0111<<dir))
			{
				uint16_t ix2 = index;
				do
				{
					possibilities[0][ix2] ^= src_bits;
					ix2 += offset;
					possibilities[0][ix2] ^= dst_bits;
				} while (map.get(ix2).population < 0);
			}
		}
		
		for (int dir=0;dir<4;dir++)
		{
			int16_t offset = offsets[dir];
			if ((prevstate^newstate) & (0x0111<<dir))
			{
				//don't merge those calls, it's a tiny slowdown for whatever reason
				uint16_t ix2 = index + offset;
				while (map.get(ix2).population < 0)
				{
					fill_simple(ix2);
					ix2 += offset;
				}
				fill_simple(ix2);
			}
		}
		
		fill_simple(index);
	}
	
	
	//Returns popcount of the middle eight bits, ignoring the other eight.
	__attribute__((always_inline)) int popcount_mid(uint16_t in)
	{
		uint64_t pack = 0x4332322132212110ULL;
		return ((pack >> ((in>>2) & 0x3C)) + (pack >> ((in>>6) & 0x3C)))&15;
	}
	
	//If 'index' is not an island root node, undefined behavior.
	void count_bridges(uint16_t index, uint8_t& min, uint8_t& max, bool& has_doubles)
	{
		if (LIKELY((possibilities[0][index]&0xF000) == 0))
		{
			uint16_t flags = possibilities[0][index];
			
			//check how many bridges exit from each tile
			//keep only the lowest and highest bits
			// (the <<8s would be unneeded if '0 or 2 but not 1' was impossible, but I don't think it is)
			uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
			uint16_t flags_max = flags;
			
			min = popcount_mid(flags_min | flags_min>>4);
			max = popcount_mid(flags_max | flags_max>>4);
			has_doubles = (flags & (flags>>8));
			
			return;
		}
		
		min = 0;
		max = 0;
		has_doubles = false;
		
		uint16_t ni = index;
		do {
			uint16_t flags = possibilities[0][ni] & ~0xF000;
			
			//check how many bridges exit from each tile
			//keep only the lowest and highest bits
			// (the <<8s would be unneeded if '0 or 2 but not 1' was impossible, but I don't think it is)
			uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
			uint16_t flags_max = flags;
			
			min += popcount_mid(flags_min | flags_min>>4);
			max += popcount_mid(flags_max | flags_max>>4);
			has_doubles |= (flags & (flags>>8));
			
			ni = nextjoined[index];
		} while (ni != index);
	}
	
	
	void fill_simple(uint16_t index)
	{
		gamemap::island& here = map.get(index);
		if (here.rootnode != index) return;
		
		if (here.population < 0)
		{
			uint16_t flags = possibilities[0][index];
			
			//ocean tile with mandatory vertical bridge and allowed horizontal -> require zero horizontal
			if ((flags & 0x0550) != 0 && (flags & 0x000A) == 0)
				set_state(index, flags & 0x0AAF);
			//mandatory horizontal -> ban vertical
			if ((flags & 0x0AA0) != 0 && (flags & 0x0005) == 0)
				set_state(index, flags & 0x055F);
			return;
		}
		
		
		uint8_t min;
		uint8_t max;
		bool has_doubles;
		count_bridges(index, min, max, has_doubles);
		
		if (min == max) return;
		
		//if no further bridges can be built, ban the options
		if (here.population == min)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[0][ni];
				uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
				set_state(ni, flags_min);
				
				ni = nextjoined[index];
			} while (ni != index);
			
			return;
		}
		
		//if all possible bridges must be built, do that
		if (here.population == max)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[0][ni];
				uint16_t flags_max = (flags & ~(flags>>4 | flags>>8));
				set_state(ni, flags_max);
				
				ni = nextjoined[index];
			} while (ni != index);
			
			return;
		}
		
		//if only one more bridge is free, or all but one must be taken, lock all two-bridge possibilities
		if (!has_doubles) return; // no two-bridge possibilities open -> skip checks
		
		if (here.population-1 == min)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[0][ni];
				uint16_t flags_min_minus1 = (flags & ~(flags<<8));
				set_state(ni, flags_min_minus1);
				
				ni = nextjoined[index];
			} while (ni != index);
		}
		if (here.population+1 == max)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[0][ni];
				uint16_t flags_max_minus1 = (flags & ~(flags>>8));
				set_state(ni, flags_max_minus1);
				
				ni = nextjoined[index];
			} while (ni != index);
		}
	}
	
public:
	solver(gamemap& map, bool use_source_bridges) : map(map)
	{
		//make sure that existing bridges are labeled as such on all tiles they're on
		//'map' is known correct, but anything other than that should use set_state
		
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			nextjoined[y*100+x] = y*100+x;
		}
		
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			gamemap::island& here = map.map[y][x];
			int index = y*100+x;
			
			if (here.rootnode != index)
			{
				int tmp = nextjoined[here.rootnode];
				nextjoined[here.rootnode] = nextjoined[index];
				nextjoined[index] = tmp;
			}
			
			uint16_t flags = 0x000F;
			for (int dir=0;dir<4;dir++)
			{
				if (here.bridgelen[dir] != -1) flags |= 0x0110<<dir;
				if (use_source_bridges && here.bridges[dir] == 1) flags &= ~(0x0001<<dir);
				if (use_source_bridges && here.bridges[dir] == 2) flags &= ~(0x0011<<dir);
				if (here.bridges[dir] == 3) { flags &= ~(0x0111<<dir); flags |= 0x1000<<dir; }
			}
			possibilities[0][index] = flags;
		}
	}
	
	//Returns index (y*100+x) to an island where a bridge can be determined buildable.
	//If skip is 1, the second available hint is returned. If no further bridges can be determined buildable on this map, returns -1.
	//Can return -1 for skip=0 if the map is unsolvable or input bridges are incorrect.
	int16_t hint(const gamemap& map, int skip = 0);
	
	//Returns whether the given map is solvable. Assumes input bridges are correct.
	//If solvable, the relevant bridges are added to the map. If not solvable, the bridges in 'map' are undefined.
	bool solve()
	{
		if (map.numislands == 0) return true;
		
		//block 1s and 2s from using both of their bindings towards each other
		if (map.numislands > 2) // except if that's the only two islands, waste of time
		{
			for (int y=0;y<map.height;y++)
			for (int x=0;x<map.width;x++)
			{
				gamemap::island& here = map.map[y][x];
				uint16_t flags = possibilities[0][y*100+x];
				uint16_t newflags = flags;
				
				if (here.population == 2 && (flags&0x0100) && map.map[y][x+here.bridgelen[0]].population == 2)
					newflags &= ~0x0100;
				if (here.population == 1 && (flags&0x0010) && map.map[y][x+here.bridgelen[0]].population == 1)
					newflags &= ~0x0010;
				
				if (here.population == 2 && (flags&0x0800) && map.map[y+here.bridgelen[3]][x].population == 2)
					newflags &= ~0x0800;
				if (here.population == 1 && (flags&0x0080) && map.map[y+here.bridgelen[3]][x].population == 1)
					newflags &= ~0x0080;
				
				if (flags != newflags) set_state(y*100+x, newflags);
			}
		}
		
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			fill_simple(y*100+x);
		}
		
		//use the isolation rule
	link_again_top: ;
		link_init();
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t index = y*100+x;
			gamemap::island& here = map.map[y][x];
			//link_t& lhere = links[index];
			
			uint16_t flags = possibilities[0][index];
			if (here.population > 1 && !(flags & 0x0001)) link_join(index, index+here.bridgelen[0]);
			if (here.population > 1 && !(flags & 0x0008)) link_join(index, index+here.bridgelen[3]*100);
		}
		
puts("HELLO");
		bool link_did_any = false;
		while (true)
		{
	link_again: ;
			uint16_t index1 = a_linkable_island;
			uint16_t index2 = a_linkable_island;
			uint16_t index1p = -1;
			uint16_t index2p = -1;
			
			do
			{
				uint16_t next;
				
				next = link_query(index1, index1p);
printf("1:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
index1,link_root(index1),
index1p,(index1p==65535)?-1:link_root(index1p),
next,(next==65535)?-1:link_root(next));
				index1p = index1;
				index1 = next;
				if (next == (uint16_t)-1) goto link_done; // only a single island left? then we're done
				
				next = link_query(index2, index2p);
//printf("2:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
//index2,link_root(index2),
//index2p,(index2p==65535)?-1:link_root(index2p),
//next,(next==65535)?-1:link_root(next));
				if (next != (uint16_t)-1)
				{
					index2p = index2;
					index2 = next;
					
					next = link_query(index2, index2p);
//printf("3:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
//index2,link_root(index2),
//index2p,(index2p==65535)?-1:link_root(index2p),
//next,(next==65535)?-1:link_root(next));
				}
				if (next != (uint16_t)-1)
				{
					index2p = index2;
					index2 = next;
				}
				
				if (next == (uint16_t)-1)
				{
					uint16_t topleft;
					bool down;
					link_lookback(index2, index2p, topleft, down);
printf("FORCEJOIN=%.3i(%.3i) - join %.3i dir %i\n", index2, index2p, topleft, down*3);
					link_join(index2, index2p);
					set_state(topleft, possibilities[0][topleft] & ~(down ? 0x0008 : 0x0001));
					link_did_any = true;
					goto link_again;
				}
			} while (index1 != index2);
			
			printf("LOOP=%.3i\n",index1);
			
			int numvisited = 0;
			do {
				map.towalk[numvisited++] = index2;
				
				uint16_t next = link_query(index2, index2p);
				index2p = index2;
				index2 = next;
			} while (index1 != index2);
			
			for (int i=1;i<numvisited;i++)
			{
				printf("JOINLOOP:%.3i,%.3i\n",index1,map.towalk[i]);
				link_join(index1, map.towalk[i]);
			}
		}
	link_done: ;
		// if a set_state from a forced join helped, perhaps that set_state blocked some other bridges,
		// so we can be sure of something new
		if (link_did_any) goto link_again_top;
		
		//TODO: guessing (recursion)
		//TODO: test large islands and castles
		
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t flags = possibilities[0][y*100+x];
			
			uint8_t nright = !(flags&0x0001) + !(flags&0x0011) + !(flags&0x0111);
			uint8_t ndown  = !(flags&0x0008) + !(flags&0x0088) + !(flags&0x0888);
//printf("IX=%.3i FL=%.4X BR=%i BD=%i\n",y*100+x,flags,nright,ndown);
			
			while (map.map[y][x].bridges[0] < nright) map.toggle(x, y, 0);
			while (map.map[y][x].bridges[3] < ndown)  map.toggle(x, y, 3);
		}
		
		return map.finished();
	}
	//Give it a solved map as input. Returns another solution for the same map, if one exists; otherwise, returns false.
	bool solve_another(gamemap& map);
	
	//This is only an estimate.
	int difficulty(const gamemap& map);
};
}

//int16_t solver_hint(const gamemap& map, int skip) { solver s; return s.hint(map, skip); }
bool solver_solve(gamemap& map) { solver s(map, true); return s.solve(); }
//bool solver_solve_another(gamemap& map) { solver s; return s.solve_another(map); }
//int solver_difficulty(const gamemap& map) { solver s; return s.difficulty(map); }
