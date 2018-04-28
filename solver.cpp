#include "game.h"

namespace {

class linker {
	//This is similar to union-find, but not exactly. The operations I need are slightly different.
	struct link_t {
		uint16_t links[4]; // Available links, or -1. May point to a node with the same root.
		
		uint16_t join_root; // Points towards the root node of any joined set. For the real root, points to itself.
		uint16_t join_next; // This creates a linked list of joined nodes. The last one points to the root.
		uint16_t join_last; // For the root node, points to the last node in the linked list. For others, unused.
		
		// including this speeds up the system a bit for some reason. simpler address calculation for size-16 objects than 14?
		uint16_t padding;
	};
	link_t links[100*100];
	uint16_t n_linkable_islands;
	uint16_t a_linkable_island; // Points to any one island that does not have population 1.
	
public:
	void init(gamemap& map, uint16_t* possibilities)
	{
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t index = y*100+x;
			gamemap::island& here = map.map[y][x];
			link_t& lhere = links[index];
			
			if (here.population >= 0)
			{
				uint16_t flags = possibilities[index];
				lhere.links[0] = (flags&0x1110 ? index + here.bridgelen[0]     : -1);
				lhere.links[1] = (flags&0x2220 ? index - here.bridgelen[1]*100 : -1);
				lhere.links[2] = (flags&0x4440 ? index - here.bridgelen[2]     : -1);
				lhere.links[3] = (flags&0x8880 ? index + here.bridgelen[3]*100 : -1);
				
				lhere.join_root = index;
				lhere.join_next = index;
				lhere.join_last = index;
				
				// including this speeds up the system a bit for some reason. write combining so it doesn't have to read the cacheline?
				lhere.padding = 0;
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
				for (int i=0;i<4;i++)
				{
					if (lhere.links[i] != (uint16_t)-1)
					{
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
		//  during the initial phase, mark bridges between known-red and known-blue castles known to not exist
		//    (requires the ability to query all edges from a vertex, not just two)
		//  during the main phase, always start from a castle
		//  if you reach a different-color castle, stop there, consider it a loop, and join them
		//  alternatively and equivalently, create a fake connection between all castles (or one per color) and allow using that to create loops
		//  this can make it report 'don't know' for some maps that should be solvable, but that's already the case for some maps, like
		//   1331
		//   1331
		//  (or 22/22 if the no-double-binding-2s rule is relaxed), so a guessing engine is required anyways
	}
	
	uint16_t root(uint16_t index)
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
	
	uint16_t count() { return n_linkable_islands; }
	uint16_t any() { return a_linkable_island; }
	//Returns an island joined with this one.
	//It's cyclical; keep calling this, and once you get the starting point back, you've seen them all.
	uint16_t iter(uint16_t index)
	{
		return links[index].join_next;
	}
	
	bool can_join(uint16_t index, int dir)
	{
		return links[index].links[dir] != (uint16_t)-1;
	}
	void join(uint16_t a, uint16_t b)
	{
		uint16_t r1 = root(a);
		uint16_t r2 = root(b);
		
		if (r1 == r2) return;
		
		link_t& lr1 = links[r1];
		link_t& lr2 = links[r2];
		
		links[lr1.join_last].join_next = r2;
		links[lr2.join_last].join_next = r1;
		lr1.join_last = lr2.join_last;
		lr2.join_root = r1;
	}
	//Returns any link from 'index', except the first link leading to 'other' is excluded.
	//If there are two links to 'other', the other one can be returned.
	//Always returns the same output for the same input. Does not necessarily return links to root nodes.
	uint16_t query(uint16_t index, uint16_t other)
	{
		if (other != (uint16_t)-1) other = root(other);
		uint16_t r = root(index);
		uint16_t ni = r;
		do {
			link_t& link = links[ni];
			
			for (int i=0;i<4;i++)
			{
				if (link.links[i] != (uint16_t)-1)
				{
					uint16_t targetroot = root(link.links[i]);
					if (targetroot == r)
						link.links[i] = -1;
					else if (targetroot == other)
						other = -1;
					else
						return link.links[i];
				}
			}
			
			ni = link.join_next;
			//if (other != (uint16_t)-1) links[r].join_firstneighbor = ni;
		} while (ni != r);
		return -1;
	}
	//Given 'link', which is a return value from linker::query, and 'orig', the input to linker::query,
	// tells which bridge on the original map that corresponds to, and in which direction.
	//If there are multiple possible bridges between those two roots, returns an arbitrary one.
	void lookback(gamemap& map, uint16_t link, uint16_t orig, uint16_t& topleft, bool& down)
	{
		gamemap::island& here = map.get(link);
		link_t& lhere = links[link];
		orig = root(orig);
		
		if (lhere.links[0] != (uint16_t)-1 && root(lhere.links[0]) == orig)
		{
			topleft = link;
			down = false;
			return;
		}
		
		if (lhere.links[1] != (uint16_t)-1 && root(lhere.links[1]) == orig)
		{
			topleft = link - here.bridgelen[1]*100;
			down = true;
			return;
		}
		
		if (lhere.links[2] != (uint16_t)-1 && root(lhere.links[2]) == orig)
		{
			topleft = link - here.bridgelen[2];
			down = false;
			return;
		}
		
		if (lhere.links[3] != (uint16_t)-1 && root(lhere.links[3]) == orig)
		{
			topleft = link;
			down = true;
			return;
		}
		
		abort(); // unreachable
	}
};

enum op_t { op_solve, op_another, op_hint, op_difficulty };
template<op_t op>
class solver {
	//design goal: be able to solve as many 100*100 maps as possible, in 1MB RAM or less (including stack and gamemap)
	//1MB / 10000 = 104 bytes per tile, but let's say 100 to give ample space for not-per-tile stuff
	//usage:
	//- gamemap: 1+1+2+4+4 = 12 bytes per tile
	//- gamemap finished() buffers (can be moved to stack if necessary): 1+2 = 3 bytes per tile
	//- nextjoined: 2 bytes per tile
	//- possibilities: 64 bytes per tile
	//- links: 16 bytes per tile
	//- sum: 12+3+2+20+16 = 97 bytes per tile
	//anything that's not per tile (including the stack) fits in the 48576 bytes left in the megabyte
	
	gamemap& map;
	
	//for small islands, points to self
	//for large islands, cyclical linked list of all tiles of the island (arbitrary order), to allow iterating through them
	//for ocean tiles, unspecified value
	uint16_t nextjoined[100*100];
	
	// the 1<<0 bit means 'it's possible that 0 bridges exit this tile to the right'
	// 1<<(1,2,3) means up, left and down, respectively
	// the 1<<(4..7) bits mean it's possible to have 1 bridge in that direction
	// 1<<(8..11) means 2 bridges
	// 1<<(12..15) means joined with that island; if set, 0 1 and 2 are banned in that direction
	// ocean tiles have this set too; for them, left and right are always equal, as are up/down
	uint16_t* possibilities;
	uint16_t possibilities_buf[32][100*100];
	
	linker link;
	
	uint32_t difficulty;
	
	
	//Can only block possibilities, never allow anything new.
	//Returns false if the map is now known unsolvable. If so, 'possibilities' is unspecified.
	//TODO: should add fill_simple calls to some FILO stack, rather than recursing
	bool set_state(uint16_t index, uint16_t newstate)
	{
		uint16_t prevstate = possibilities[index];
//printf("SET(%.3i):%.4X->%.4X\n",index,prevstate,newstate);
		
		if (newstate & ~prevstate) abort();
		
		uint16_t tmp = newstate;
		tmp |= tmp>>4;
		tmp |= tmp>>8;
		if ((tmp&0x000F) != 0x000F) return false;
		
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
					possibilities[ix2] ^= src_bits;
					ix2 += offset;
					possibilities[ix2] ^= dst_bits;
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
					if (!fill_simple(ix2)) return false;
					ix2 += offset;
				}
				if (!fill_simple(ix2)) return false;
			}
		}
		
		if (!fill_simple(index)) return false;
		return true;
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
		if (LIKELY((possibilities[index]&0xF000) == 0))
		{
			uint16_t flags = possibilities[index];
			
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
			uint16_t flags = possibilities[ni] & ~0xF000;
			
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
	
	
	//Returns false if the map is now known unsolvable. If so, 'possibilities' is unspecified.
	bool fill_simple(uint16_t index)
	{
		index = map.get(index).rootnode;
		gamemap::island& here = map.get(index);
		
		if (here.population < 0)
		{
			uint16_t flags = possibilities[index];
			
			//ocean tile with mandatory vertical bridge and allowed horizontal -> require zero horizontal
			if ((flags & 0x0550) != 0 && (flags & 0x000A) == 0)
				if (!set_state(index, flags & 0x0AAF)) return false;
			//mandatory horizontal -> ban vertical
			if ((flags & 0x0AA0) != 0 && (flags & 0x0005) == 0)
				if (!set_state(index, flags & 0x055F)) return false;
			
			//if (!(flags&0x1111) || !(flags&0x2222) || !(flags&0x4444) || !(flags&0x8888)) return false;
			return true;
		}
		
		
		uint8_t min;
		uint8_t max;
		bool has_doubles;
		count_bridges(index, min, max, has_doubles);
		
		if (here.population < min || here.population > max) return false;
		if (min == max) return true;
		
		//if no further bridges can be built, ban the options
		if (here.population == min)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[ni];
				uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
				if (!set_state(ni, flags_min)) return false;;
				
				ni = nextjoined[index];
			} while (ni != index);
			
			return true;
		}
		
		//if all possible bridges must be built, do that
		if (here.population == max)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[ni];
				uint16_t flags_max = (flags & ~(flags>>4 | flags>>8));
				if (!set_state(ni, flags_max)) return false;
				
				ni = nextjoined[index];
			} while (ni != index);
			
			return true;
		}
		
		//if only one more bridge is free, or all but one must be taken, lock all two-bridge possibilities
		if (!has_doubles) return true; // no two-bridge possibilities open -> skip checks
		
		if (here.population-1 == min)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[ni];
				uint16_t flags_min_minus1 = (flags & ~(flags<<8));
				if (!set_state(ni, flags_min_minus1)) return false;
				
				ni = nextjoined[index];
			} while (ni != index);
		}
		if (here.population+1 == max)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[ni];
				uint16_t flags_max_minus1 = (flags & ~(flags>>8));
				if (!set_state(ni, flags_max_minus1)) return false;
				
				ni = nextjoined[index];
			} while (ni != index);
		}
		
		return true;
	}
	
public:
	solver(gamemap& map) : map(map)
	{
		possibilities = possibilities_buf[0];
		
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
				if (op!=op_another && here.bridges[dir] == 1) flags &= ~(0x0001<<dir);
				if (op!=op_another && here.bridges[dir] == 2) flags &= ~(0x0011<<dir);
				if (here.bridges[dir] == 3) { flags &= ~(0x0111<<dir); flags |= 0x1000<<dir; }
			}
			possibilities[index] = flags;
		}
	}
	
	bool do_isolation_rule()
	{
		//use the isolation rule
	link_again_top: ;
		link.init(map, possibilities);
//puts("BEGIN");
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t index = y*100+x;
			gamemap::island& here = map.map[y][x];
			
			uint16_t flags = possibilities[index];
			if (here.population>0 && !(flags & 0x0001) && link.can_join(index, 0))
			{
				link.join(index, index+here.bridgelen[0]);
			}
			if (here.population>0 && !(flags & 0x0008) && link.can_join(index, 3))
			{
				link.join(index, index+here.bridgelen[3]*100);
			}
//if(x==0)puts("");printf("%.4X ",flags);
		}
//puts("");
		
//puts("HELLO");
		bool link_did_any = false;
		if (link.count() != 0)
		{
	link_again: ;
			uint16_t index1 = link.any();
			uint16_t index2 = link.any();
			uint16_t index1p = -1;
			uint16_t index2p = -1;
			
			do
			{
				uint16_t next;
				
//printf("1:%.3i(...),%.3i(...),...\n",index1,index1p);
//printf("1:%.3i(%.3i),%.3i(%.3i),...\n",
//index1,link.root(index1),
//index1p,(index1p==65535)?-1:link.root(index1p));
				next = link.query(index1, index1p);
//printf("1:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
//index1,link.root(index1),
//index1p,(index1p==65535)?-1:link.root(index1p),
//next,(next==65535)?-1:link.root(next));
				index1p = index1;
				index1 = next;
				if (next == (uint16_t)-1) goto link_done; // only a single island left? then we're done
				
				next = link.query(index2, index2p);
//printf("2:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
//index2,link.root(index2),
//index2p,(index2p==65535)?-1:link.root(index2p),
//next,(next==65535)?-1:link.root(next));
				if (next != (uint16_t)-1)
				{
					index2p = index2;
					index2 = next;
					
					next = link.query(index2, index2p);
//printf("3:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
//index2,link.root(index2),
//index2p,(index2p==65535)?-1:link.root(index2p),
//next,(next==65535)?-1:link.root(next));
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
					link.lookback(map, index2, index2p, topleft, down);
//printf("FORCEJOIN=%.3i(%.3i) - join %.3i dir %i\n", index2, index2p, topleft, down*3);
					link.join(index2, index2p);
					if (!set_state(topleft, possibilities[topleft] & ~(down ? 0x0008 : 0x0001))) return false;
					link_did_any = true;
					goto link_again;
				}
			} while (index1 != index2);
			
//printf("LOOP=%.3i\n",index1);
			int numvisited = 0;
			do {
				map.towalk[numvisited++] = index1;
				
				uint16_t next = link.query(index1, index1p);
//printf("LOOPWALK:%.3i(%.3i),%.3i(%.3i),%.3i(%.3i)\n",
//index1,link.root(index1),
//index1p,(index1p==65535)?-1:link.root(index1p),
//next,(next==65535)?-1:link.root(next));
				index1p = index1;
				index1 = next;
			} while (index1 != index2);
			
			for (int i=1;i<numvisited;i++)
			{
//printf("JOINLOOP:%.3i,%.3i\n",index1,map.towalk[i]);
				link.join(index1, map.towalk[i]);
			}
			goto link_again;
		}
	link_done: ;
		// if a set_state from a forced join helped, perhaps that set_state blocked some other bridges,
		// so let's check if we can prove anything new
		if (link_did_any) goto link_again_top;
		
		
		if (link.count() != 0)
		{
			//iterate everything joined with a_linkable_island; if not equal to n_linkable_islands, it's disjoint, so return false
			uint16_t n_linked_islands = 0;
			uint16_t index = link.any();
			
			do {
//printf("LINK::%.3i\n",index);
				n_linked_islands++;
				index = link.iter(index);
			} while (index != link.any());
			
//printf("LINKS=%i,%i\n",n_linked_islands,link.count());
			if (n_linked_islands != link.count()) return false;
		}
		
		return true;
	}
	
	bool solve_rec(uint16_t layer)
	{
		if (layer < sizeof(possibilities_buf)/sizeof(possibilities_buf[0]))
		{
			if (!do_isolation_rule()) return false;
			
			//if still unsure, guess
			for (int y=0;y<map.height;y++)
			for (int x=0;x<map.width;x++)
			{
				uint16_t index = y*100+x;
//printf("EEE=%.3i\n",index);
				//gamemap::island& here = map.map[y][x];
				uint16_t flags = possibilities[index];
				uint16_t multidir_flags = (flags & (flags>>4 | flags>>8));
				if (!multidir_flags) continue;
				
				//right and up (ignore left/down, they'd only find duplicates)
				for (int dir=0;dir<2;dir++)
				{
//printf("%.4X:%.4X\n",flags,multidir_flags & (0x0111<<dir));
					//if multiple bridge numbers are allowed in this direction...
					if (multidir_flags & (0x0111<<dir))
					{
						int set_mask = (dir ? 0xDDDD : 0xEEEE);
						for (int n=0;n<=2;n++)
						{
							int dirbit = (0x0001<<(n*4)<<dir);
							if (flags & dirbit)
							{
//printf("GUESS:BEGIN(%.3i:%.4X->%.4X)\n",index,flags,(flags & (set_mask | dirbit)));
								//if looking for another solution, only make contrary assumptions
								if (op==op_another && map.get(index).bridges[dir] == n)
								{
//puts("GUESS:NOTSAME");
									continue;
								}
								
								int newflags = (flags & (set_mask | dirbit));
								memcpy(possibilities_buf[layer+1], possibilities_buf[layer], sizeof(possibilities_buf[0]));
								possibilities = possibilities_buf[layer+1];
								
								if (set_state(index, newflags) && solve_rec(layer+1))
								{
//puts("GUESS:GOOD");
									//if that's a valid solution, return it
									//otherwise, mark the map unsolvable
									possibilities = possibilities_buf[layer];
									memcpy(possibilities_buf[layer], possibilities_buf[layer+1], sizeof(possibilities_buf[0]));
									return true;
								}
								else
								{
//puts("GUESS:BAD");
									//if that can't be a valid solution, mark it as such
									possibilities = possibilities_buf[layer];
									if (!set_state(index, flags & ~dirbit)) return false; // it's possible that both yes and no are impossible
									
									if (!do_isolation_rule()) return false;
									flags = possibilities[index];
								}
							}
						}
					}
				}
			}
		}
		else
		{
			//if out of memory, report it's unsolvable I guess?
			puts("EEE");
			abort();
			return false;
		}
		
		//TODO: test large islands and castles
		
		if (op==op_another && layer==0) return false;
		return true;
	}
	
	bool solve()
	{
//puts("SOLVEBEGIN");
		if (map.numislands == 0) return (op!=op_another);
		
		//block 1s and 2s from using both of their bindings towards each other
		if (map.numislands > 2) // except if that's the only two islands, those maps are valid
		{
			for (int y=0;y<map.height;y++)
			for (int x=0;x<map.width;x++)
			{
				gamemap::island& here = map.map[y][x];
				uint16_t flags = possibilities[y*100+x];
				uint16_t newflags = flags;
				
				if (here.population == 2 && (flags&0x0100) && map.map[y][x+here.bridgelen[0]].population == 2)
					newflags &= ~0x0100;
				if (here.population == 1 && (flags&0x0010) && map.map[y][x+here.bridgelen[0]].population == 1)
					newflags &= ~0x0010;
				
				if (here.population == 2 && (flags&0x0800) && map.map[y+here.bridgelen[3]][x].population == 2)
					newflags &= ~0x0800;
				if (here.population == 1 && (flags&0x0080) && map.map[y+here.bridgelen[3]][x].population == 1)
					newflags &= ~0x0080;
				
				if (flags != newflags)
				{
					if (!set_state(y*100+x, newflags)) return false;
				}
			}
		}
		
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			if (map.map[y][x].rootnode != y*100+x) continue; // only poke the roots, no point doing duplicate checks
			if (!fill_simple(y*100+x)) return false;
		}
		
		if (!solve_rec(0)) return false;
		
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			uint16_t flags = possibilities[y*100+x];
			
			uint8_t nright = !(flags&0x0001) + !(flags&0x0011) + !(flags&0x0111);
			uint8_t ndown  = !(flags&0x0008) + !(flags&0x0088) + !(flags&0x0888);
//printf("IX=%.3i FL=%.4X BR=%i BD=%i\n",y*100+x,flags,nright,ndown);
			
			while (map.map[y][x].bridges[0] != nright) map.toggle(x, y, 0);
			while (map.map[y][x].bridges[3] != ndown)  map.toggle(x, y, 3);
		}
		
		if (!map.finished()) abort();
		return true;
	}
};
}

bool solver_solve(gamemap& map) { solver<op_solve> s(map); return s.solve(); }
bool solver_solve_another(gamemap& map) { solver<op_another> s(map); return s.solve(); }
//int16_t solver_hint(const gamemap& map, int skip) { solver s; return s.hint(map, skip); }
//int solver_difficulty(const gamemap& map) { solver s; return s.difficulty(map); }

static void test_split(cstring in, string& map, string& solution)
{
	array<cstring> lines = in.csplit("\n");
	
	for (size_t i=0;i<lines.size()-1;i+=2)
	{
		map      += lines[i  ]+"\n";
		solution += lines[i+1]+"\n";
	}
}

static void test_one(cstring test)
{
	string map;
	string solution_good;
	test_split(test, map, solution_good);
	
	gamemap m;
	m.init(map);
	assert(solver_solve(m));
	assert(m.finished());
	for (int y=0;y<m.height;y++)
	for (int x=0;x<m.width;x++)
	{
		char exp = solution_good[y*(m.width+1) + x];
		if (exp != ' ')
		{
			assert_eq(m.map[y][x].bridges[0], exp-'0');
		}
	}
	assert(!solver_solve_another(m));
}

static void test_unsolv(cstring map)
{
	gamemap m;
	m.init(map.c_str());
	assert(!solver_solve(m));
}

static void test_multi(cstring map)
{
	gamemap m;
	m.init(map.c_str());
	assert(solver_solve(m));
	assert(m.finished());
	assert(solver_solve_another(m));
	assert(m.finished());
}

test("solver", "", "solver")
{
	testcall(test_one( // the outer islands only connect to one island each, so the map is trivial
		" 2 \n" /* */ " 0 \n"
		"271\n" /* */ "210\n"
		" 2 \n" /* */ " 0 \n"
	));
	testcall(test_one( // a few islands are immediately obvious, the others show up once the simple ones are done
		" 1  3\n" /* */ " 1  0\n"
		"2 1  \n" /* */ "1 0  \n"
		"2  23\n" /* */ "1  10\n"
	));
	testcall(test_one( // make sure the smallest possible maps remain functional
		"11\n" /* */ "10\n"
	));
	testcall(test_one(
		"2\n" /* */ "0\n"
		"2\n" /* */ "0\n"
	));
	testcall(test_one( // the fabled zero
		"    \n" /* */ "    \n"
		"  0 \n" /* */ "  0 \n"
		"    \n" /* */ "    \n"
		"    \n" /* */ "    \n"
	));
	testcall(test_one( // a map without islands, while unusual, is valid
		"   \n" /* */ "   \n"
		"   \n" /* */ "   \n"
	));
	testcall(test_one( // a 0x1 map is even more unusual, but equally valid (0x0 is unrepresentable)
		"\n" /* */ "\n"
	));
	testcall(test_one( // bridges may not cross
		"464\n" /* */ "220\n"
		"4 4\n" /* */ "0 0\n"
		"2 2\n" /* */ "0 0\n"
		" 2 \n" /* */ " 0 \n"
	));
	testcall(test_one( // requires guessing (or using the 2s-may-not-double-bind rule)
		"2 2\n" /* */ "1 0\n"
		"222\n" /* */ "110\n"
	));
	testcall(test_one( // all islands must be connected
		"22\n" /* */ "10\n"
		"22\n" /* */ "10\n"
	));
	testcall(test_one( // doesn't enter the must-be-connected function, but it has caught a few bugs
		"1 2  \n" /* */ "1 0  \n"
		" 1 1 \n" /* */ " 0 0 \n"
		"14441\n" /* */ "11110\n"
		" 1 1 \n" /* */ " 0 0 \n"
		"  2 1\n" /* */ "  1 0\n"
	));
	testcall(test_one( // bigger and nastier
		"4    4 \n" /* */ "2    0 \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"    682\n" /* */ "    220\n"
		" 282   \n" /* */ " 220   \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"4    4 \n" /* */ "2    0 \n"
	));
	testcall(test_one( // also requires guessing, multiple times (or the 2s-may-not-double-bind rule)
		"2  2\n" /* */ "1  0\n"
		" 22 \n" /* */ " 10 \n"
		"  33\n" /* */ "  10\n"
		" 22 \n" /* */ " 10 \n"
		"2  2\n" /* */ "1  0\n"
	));
	testcall(test_one( // nasty amount of cycles because must annoy island tracker
		"2          2\n" /* */ "1          0\n"
		" 2        2 \n" /* */ " 1        0 \n"
		"  2  3   2  \n" /* */ "  1  1   0  \n"
		"   2 3  2   \n" /* */ "   1 1  0   \n"
		"    2  2    \n" /* */ "    1  0    \n"
		"       33   \n" /* */ "       10   \n"
		" 33         \n" /* */ " 10         \n"
		"    2  2    \n" /* */ "    1  0    \n"
		"   2    2   \n" /* */ "   1    0   \n"
		"  2      2  \n" /* */ "  1      0  \n"
		" 2    3   2 \n" /* */ " 1    1   0 \n"
		"2     3    2\n" /* */ "1     1    0\n"
	));
	testcall(test_one( // all islands must be connected (exercises the new solver too)
		" 1 \n" /* */ " 0 \n"
		" 32\n" /* */ " 10\n"
		"2 1\n" /* */ "0 0\n"
		"32 \n" /* */ "10 \n"
	));
	
	// these require nested guesses (created by the game generator)
	// (humans can solve them all without nested guesses, by knowing that 1s can't connect two islands)
	testcall(test_one(
		"2  1\n" /* */ "0  0\n"
		"3233\n" /* */ "1110\n"
		" 131\n" /* */ " 100\n"
		"1 2 \n" /* */ "1 0 \n"
	));
	testcall(test_one(
		"1232\n" /* */ "0120\n"
		"2 11\n" /* */ "0 00\n"
		"5543\n" /* */ "2120\n"
		"23 2\n" /* */ "02 0\n"
	));
	testcall(test_one(
		"1332\n" /* */ "1100\n"
		"13 3\n" /* */ "10 0\n"
		"21  \n" /* */ "00  \n"
		"3 42\n" /* */ "1 10\n"
	));
	testcall(test_one(
		"3431\n" /* */ "2110\n"
		" 112\n" /* */ " 000\n"
		"32 3\n" /* */ "11 0\n"
		"2  1\n" /* */ "1  0\n"
	));
	//these require guessing, even with the advanced solver
	//the cycle detector doesn't know about the population rules
	//(extra 1s to avoid the 2s-don't-double-bind special case)
	testcall(test_one(
		"1 \n" /* */ "0 \n"
		"32\n" /* */ "10\n"
		"23\n" /* */ "10\n"
		" 1\n" /* */ " 0\n"
	));
	testcall(test_one( // make sure anisland[0] has population 1
		" 11 \n" /* */ " 00 \n"
		"1441\n" /* */ "1110\n"
		"1441\n" /* */ "1110\n"
		" 11 \n" /* */ " 00 \n"
	));
	testcall(test_one(
		"1    1\n" /* */ "0    0\n"
		"3    3\n" /* */ "1    0\n"
		" 133  \n" /* */ " 120  \n"
		"3  31 \n" /* */ "1  10 \n"
		"113  3\n" /* */ "012  0\n"
	));
	testcall(test_one(
		"22   2\n" /* */ "11   0\n"
		" 122  \n" /* */ " 110  \n"
		"2  213\n" /* */ "1  010\n"
		"1 2  2\n" /* */ "1 1  0\n"
	));
	
	//the new solver tried connecting a few impossible bridges to this one
	testcall(test_one(
		" 42\n" /* */ " 20\n"
		"231\n" /* */ "000\n"
		"442\n" /* */ "210\n"
	));
	//crashed after making an incorrect assumption
	testcall(test_one(
		"222\n" /* */ "110\n"
		"3 4\n" /* */ "1 0\n"
		"212\n" /* */ "100\n"
	));
	//didn't catch a 'no, that's impossible' from set_state on an ocean tile
	testcall(test_one(
		"2 21\n" /* */ "1 00\n"
		"32 3\n" /* */ "20 0\n"
		"  23\n" /* */ "  10\n"
	));
	//the old solver made a silly assumption and violated a population limit on this map
	testcall(test_one(
		"1     \n" /* */ "0     \n"
		"244422\n" /* */ "121110\n"
		"    1 \n" /* */ "    0 \n"
		" 2  43\n" /* */ " 0  20\n"
		"   45 \n" /* */ "   20 \n"
		"   13 \n" /* */ "   10 \n"
		" 34 1 \n" /* */ " 21 0 \n"
	));
	
	
	testcall(test_unsolv( // obviously impossible
		"11\n"
		"11\n"
	));
	testcall(test_unsolv( // even more obviously impossible
		"12\n"
	));
	testcall(test_unsolv( // stupidly obviously impossible
		"2 \n"
		" 3\n"
	));
	testcall(test_unsolv( // filling in everything yields a disjoint map
		"2 2 \n"
		" 2 2\n"
	));
	
	//these have multiple solutions
	testcall(test_multi(
		"33\n"
		"22\n"
	));
	testcall(test_multi(
		" 31\n"
		"272\n"
		"131\n"
	));
	testcall(test_multi(
		"232\n"
		"343\n"
		"232\n"
	));
	testcall(test_multi(
		"2332\n"
		"3443\n"
		"2332\n"
	));
	
	//'are they joined' walk went 'A B C D B A' before noticing it's a loop, then couldn't follow the path back to 'B'
	testcall(test_multi(
		"331  \n"
		"22  2\n"
		"   13\n"
		"  232\n"
	));
	//I don't remember what this caught, probably something assumption-related again
	testcall(test_multi(
		"3322\n" /* */ "3322\n"
		"3   \n" /* */ "3   \n"
		"353 \n" /* */ "353 \n"
		"25 3\n" /* */ "25 3\n"
	));
	//if an assumption is false, it could claim the opposite is a valid solution, even if it's disjoint
	testcall(test_multi(
		"2 12 \n"
		"4  52\n"
		"2  2 \n"
		"  252\n"
		"   1 \n"
	));
	
	//this map is requires a solver depth of 37
	/*
	testcall(test_multi(
		"23  3 6  4  52 \n"
		"2 3   5    45 3\n"
		"34    5 4 2 32 \n"
		"44         6  4\n"
		"3 344  3  3 243\n"
		"      2 1 4 2  \n"
		"3 355 4 1 2 1  \n"
		"  255  4   4 4 \n"
		"4 2 5 3215 6 5 \n"
		" 4    5 451 34 \n"
		"3 1331 2 1145  \n"
		"  3622 47 22 34\n"
		"57 5 3323145  3\n"
		" 4       44542 \n"
		"442     332  22\n"
	));
	*/
	
	//enable these once the solver handles castles
	/*
	testcall(test_unsolv( // unsolvable, but falsely reported solvable if an_island[0] points to a corner and it walks from there
		"2  2\n"
		" r< \n"
		" ^^ \n"
		"2  2\n"
	));
	*/
}
