#include "game.h"

namespace {

//TODO: once hint and BFS-difficulty exist, make this thing not a template; instead, use multiple functions
// that way, the identical functions can be combined (also some of the errors suck)

//#define abort() // oddly enough, these assertions don't slow anything down noticably, just ~1%

class linker {
	//This is similar to union-find, but not exactly. The operations I need are slightly different.
	struct link_t {
		uint16_t links[4]; // Available links, or -1. May point to a node with the same root.
		
		uint16_t join_root; // Points towards the root node of any joined set. For the real root, points to itself.
		uint16_t join_next; // This creates a linked list of joined nodes. The last one points to the root.
		uint16_t join_last; // For the root node, points to the last node in the linked list. For others, unused.
		
		uint16_t castlecolor; // kinda doesn't fit here, but it works and anything else would be way worse
	};
	link_t links[100*100];
	uint16_t a_linkable_island; // Points to any one island that does not have population 1.
	uint16_t n_linkable_islands; // Number of islands that do not have population 1.
	
public:
#ifndef STDOUT_ERROR
void debug(int i){printf("link[%.3i]:%.3i,%.3i,%.3i,%.3i; %.3i,%.3i,x\n",i,
links[i].links[0],links[i].links[1],links[i].links[2],links[i].links[3],
links[i].join_root,links[i].join_next);}
#endif
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
				
				lhere.castlecolor = here.population;
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
	// The root of 'a' becomes the new root. It's fine if the two islands don't touch.
	// Returns false if that connects different-color castles. This can be ignored after the setup stage.
	bool join(uint16_t a, uint16_t b)
	{
		uint16_t r1 = root(a);
		uint16_t r2 = root(b);
		
		if (r1 == r2) return true;
		
		link_t& lr1 = links[r1];
		link_t& lr2 = links[r2];
		
		links[lr1.join_last].join_next = r2;
		links[lr2.join_last].join_next = r1;
		lr1.join_last = lr2.join_last;
		lr2.join_root = r1;
		
		if (lr1.castlecolor >= 80 || lr2.castlecolor >= 80)
		{
			if (lr1.castlecolor == lr2.castlecolor) return true;
			if (lr1.castlecolor >= 80 && lr2.castlecolor >= 80) return false;
			if (lr1.castlecolor < lr2.castlecolor) lr1.castlecolor = lr2.castlecolor;
		}
		return true;
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
	//Returns all links from 'index', one at the time. Initialize the states to -1 on first call.
	//Always returns the same output for the same input. Does not necessarily return links to root nodes.
	uint16_t query_all(uint16_t index, uint16_t& state1, uint16_t& state2)
	{
		uint16_t r = root(index);
		
		uint16_t& ni = state1;
		uint16_t& i = state2;
		if (state1 == (uint16_t)-1)
		{
			ni = r;
			i = 0;
		}
		
		do {
			link_t& link = links[ni];
			
			while (i < 4)
			{
				if (link.links[i] != (uint16_t)-1)
				{
					uint16_t targetroot = root(link.links[i]);
					if (targetroot == r)
						link.links[i] = -1;
					else
						return link.links[i++]; // i++ to ensure the same one isn't returned forever
				}
				i++;
			}
			
			i = 0;
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
	
	uint16_t color(uint16_t index) { return links[root(index)].castlecolor; }
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
	uint16_t possibilities_buf[32*100*100];
	
	linker link;
	
public:
	uint32_t accumulator; // used for difficulty/hint counting
private:
	
	//this increases the difficulty score even for incorrect guesses; this is intentional
	__attribute__((always_inline)) void add_difficulty(uint32_t n) { if (op==op_difficulty) accumulator += n; }
	
	
	//Can only block possibilities, never allow anything new.
	//Returns false if the map is now known unsolvable. If so, 'possibilities' is unspecified.
	//TODO: should add fill_simple calls to towalk, rather than recursing
	bool set_state(uint16_t index, uint16_t newstate)
	{
//verify();
		uint16_t prevstate = possibilities[index];
//printf("SET(%.3i):%.4X->%.4X\n",index,prevstate,newstate);
		
		if (newstate & ~prevstate) abort();
		if (newstate == prevstate) abort();
		
		uint16_t tmp = newstate;
		tmp |= tmp>>4;
		tmp |= tmp>>8;
		if ((tmp&0x000F) != 0x000F) return false;
		
		static const int16_t offsets[4] = { 1, -100, -1, 100 };
		
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
//verify(false);
					possibilities[ix2] ^= src_bits;
					if (possibilities[ix2] & src_bits) abort();
					ix2 += offset;
					possibilities[ix2] ^= dst_bits;
					if (possibilities[ix2] & dst_bits) abort();
//verify(false);
				} while (map.get(ix2).population < 0);
			}
		}
//verify();
		
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
			uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
			uint16_t flags_max = flags;
			
			min += popcount_mid(flags_min | flags_min>>4);
			max += popcount_mid(flags_max | flags_max>>4);
			has_doubles |= (flags & (flags>>8));
			
			ni = nextjoined[ni];
		} while (ni != index);
	}
	
	
	//Returns false if the map is now known unsolvable. If so, 'possibilities' is unspecified.
	bool fill_simple(uint16_t index)
	{
		index = map.get(index).rootnode;
		gamemap::island& here = map.get(index);
		
		if (here.population >= 80) return true; // these ones don't care about how many bridges they have
		
		if (here.population < 0)
		{
			uint16_t flags = possibilities[index];
			
			//ocean tile with mandatory vertical bridge and allowed horizontal -> require zero horizontal
			if ((flags & 0x0550) != 0 && (flags & 0x000A) == 0)
			{
				add_difficulty(1);
				if (!set_state(index, flags & 0x0AAF)) return false;
			}
			//mandatory horizontal -> ban vertical
			if ((flags & 0x0AA0) != 0 && (flags & 0x0005) == 0)
			{
				add_difficulty(1);
				if (!set_state(index, flags & 0x055F)) return false;
			}
			
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
				if (flags != flags_min)
				{
					add_difficulty(1);
					if (!set_state(ni, flags_min)) return false;
				}
				
				ni = nextjoined[ni];
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
				if (flags != flags_max)
				{
					add_difficulty(1);
					if (!set_state(ni, flags_max)) return false;
				}
				
				ni = nextjoined[ni];
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
				if (flags != flags_min_minus1)
				{
					add_difficulty(2);
					if (!set_state(ni, flags_min_minus1)) return false;
				}
				
				ni = nextjoined[ni];
			} while (ni != index);
		}
		if (here.population+1 == max)
		{
			uint16_t ni = index;
			do {
				uint16_t flags = possibilities[ni];
				uint16_t flags_max_minus1 = (flags & ~(flags>>8));
				if (flags != flags_max_minus1)
				{
					add_difficulty(2);
					if (!set_state(ni, flags_max_minus1)) return false;
				}
				
				ni = nextjoined[ni];
			} while (ni != index);
		}
		
		return true;
	}
	
public:
	solver(gamemap& map) : map(map) {}
	
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
				if (!link.join(index, index+here.bridgelen[0])) return false;
			}
			if (here.population>0 && !(flags & 0x0008) && link.can_join(index, 3))
			{
				if (!link.join(index, index+here.bridgelen[3]*100)) return false;
			}
//if(x==0&&y!=0)puts("");printf("%.4X ",flags);
		}
//puts("");
		
		bool link_did_any = false;
		
		if (map.has_castles)
		{
			//if map has castles, enforce that they don't touch each other
			for (int y=0;y<map.height;y++)
			for (int x=0;x<map.width;x++)
			{
				uint16_t index = y*100+x;
				gamemap::island& here = map.map[y][x];
				if (here.population < 0) continue;
				
				uint16_t flags = possibilities[index];
				uint16_t newflags = flags;
				//test all four links, to ensure the offending bridge is always caught
				for (int dir=0;dir<4;dir++)
				{
					if (!(flags & (0x0110<<dir))) continue;
					
					static const int16_t offsets[4] = { 1, -100, -1, 100 };
					uint16_t other = index + offsets[dir]*here.bridgelen[dir];
					
					uint16_t mycolor = link.color(index);
					uint16_t othercolor = link.color(other);
					
					if (mycolor != othercolor && mycolor >= 80 && othercolor >= 80)
					{
						newflags &= ~(0x0110<<dir);
					}
				}
				if (newflags != flags)
				{
					if (!set_state(index, newflags)) return false;
					link_did_any = true;
				}
//if(x==0&&y!=0)puts("");printf("%.4X ",flags);
			}
			if (link_did_any) goto link_again_top; // to ensure it doesn't try to create the bridges we just removed
			
			//then create fake links between the castles, so the solver won't say
			// 'okay, only one path, it must exist' when reaching another castle
			
			uint16_t prevcastle = -1;
			for (int castletype=1;castletype<=4;castletype++)
			{
				if (map.an_island[castletype] == -1) continue;
				
				uint16_t newcastle = map.an_island[castletype];
				if (prevcastle != (uint16_t)-1)
					link.join(prevcastle, newcastle);
				prevcastle = newcastle;
			}
		}
		
		
//puts("HELLO");
		if (link.count() != 0)
		{
	link_again: ;
			uint16_t index1 = link.any();
			uint16_t index2 = index1;
			uint16_t index1p = -1;
			uint16_t index2p = -1;
			
			//TODO: this can go 1,2,3,4,5,3,2 and consider 2 part of the loop,
			// rather than enforcing the existence of that bridge
			
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
				if (next == (uint16_t)-1) goto link_done; // only a single island left? then we're done here, try the other castles
				
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
					//if (map.get(index2).population < 80 && map.get(index2p).population < 80)
					{
						uint16_t topleft;
						bool down;
						link.lookback(map, index2, index2p, topleft, down);
//printf("FORCEJOIN=%.3i(%.3i) - join %.3i dir %i\n", index2, index2p, topleft, down*3);
						if (possibilities[topleft] & (down ? 0x0008 : 0x0001))
						{
							add_difficulty(10);
//puts("A");
							if (!set_state(topleft, possibilities[topleft] & ~(down ? 0x0008 : 0x0001))) return false;
//puts("A+");
							link_did_any = true;
						}
					}
//else printf("FORCEJOINCASTLE=%.3i,%.3i\n",index2,index2p);
					link.join(index2, index2p);
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
		link_done:
		// if a set_state from a forced join helped, perhaps that set_state blocked some other bridges,
		// so let's check if we can prove anything new
		if (link_did_any) goto link_again_top;
		
		
		if (link.count() != 0)
		{
			//iterate everything joined with a_linkable_island; if not equal to n_linkable_islands, it's disjoint, so return false
			uint16_t n_linked_islands = 0;
			
			uint16_t index = link.any();
			uint16_t indexstart = index;
			do {
//printf("LINK::%.3i\n",index);
				n_linked_islands++;
				index = link.iter(index);
			} while (index != indexstart);
			
//printf("LINKS=%i,%i\n",n_linked_islands,link.count());
			if (n_linked_islands != link.count()) return false;
		}
		
		return true;
	}
	
	//1 - solved
	//0 - unsolvable
	//-1 - requires higher max depth to prove solvability
	int solve_rec(uint16_t layer, uint16_t maxlayer)
	{
//static int n=0;if(layer>n){n=layer;printf("MAXDEPTH=%i\n",n);}
//printf("LAYER=%i\n",layer);
		add_difficulty(3*layer*layer);
		
		size_t layerbufsize = sizeof(uint16_t)*100*map.height;
		
		uint16_t maxlayer2 = sizeof(possibilities_buf)/layerbufsize;
		if (op != op_hint && op != op_difficulty) maxlayer = maxlayer2; // discard the argument and hope inliner deletes it
		if (layer < maxlayer && layer < maxlayer2)
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
				if (!multidir_flags) continue; // no guess possible? skip this tile
				if (map.map[y][x].population < 0) continue; // don't guess on ocean tiles
				
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
								// (but only for first layer, deeper ones can do whatever)
								if (op==op_another && layer==0 && map.get(index).bridges[dir] == n)
								{
//puts("GUESS:NOTSAME");
									continue;
								}
								
								int newflags = (flags & (set_mask | dirbit));
								memcpy(possibilities_buf + (layer+1)*layerbufsize, possibilities_buf + layer*layerbufsize, layerbufsize);
								possibilities = possibilities_buf + (layer+1)*layerbufsize;
								
								int ret = set_state(index, newflags);
								if (ret) ret = solve_rec(layer+1, maxlayer);
								if (ret == 1)
								{
//puts("GUESS:GOOD");
									//if that's a valid solution, return it
									//otherwise, mark the map unsolvable
									possibilities = possibilities_buf + layer*layerbufsize;
									memcpy(possibilities_buf + layer*layerbufsize, possibilities_buf + (layer+1)*layerbufsize, layerbufsize);
									return true;
								}
								if (ret == 0)
								{
//puts("GUESS:BAD");
									//if that can't be a valid solution, mark it as such
									possibilities = possibilities_buf + layer*layerbufsize;
									
									//it's possible that both yes and no are impossible
									if (!set_state(index, flags & ~dirbit)) return false;
									
									if (!do_isolation_rule()) return false;
									flags = possibilities[index];
									multidir_flags = (flags & (flags>>4 | flags>>8));
									if (!(multidir_flags & (0x0111<<dir))) break;
								}
							}
						}
					}
				}
			}
		}
		else if (layer < maxlayer2)
		{
			return -1;
		}
		else
		{
#ifndef STDOUT_ERROR
			puts("Out of memory");
			puts(map.serialize());
#endif
			abort();
			return -1;
		}
		
		if (op==op_another && layer==0) return false;
		return true;
	}
	
	//Returns an unspecified value for op_difficulty and op_hint.
	bool solve()
	{
//puts("SOLVEBEGIN");
		possibilities = possibilities_buf;
		
		int num_roots = 0;
		
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
			
			if (here.rootnode == index && here.population > 0) num_roots++;
			
			uint16_t flags = 0x000F;
			for (int dir=0;dir<4;dir++)
			{
				if (here.bridgelen[dir] != -1) flags |= 0x0110<<dir;
				if (op!=op_another && op!=op_difficulty && here.bridges[dir] == 1) flags &= ~(0x0001<<dir);
				if (op!=op_another && op!=op_difficulty && here.bridges[dir] == 2) flags &= ~(0x0011<<dir);
				if (here.bridges[dir] == 3) { flags &= ~(0x0111<<dir); flags |= 0x1000<<dir; }
			}
			possibilities[index] = flags;
		}
		
		if (map.numislands == 0) return (op!=op_another);
		if (op==op_hint || op==op_difficulty) accumulator=0;
		
		//block 1s and 2s from using both of their bindings towards each other
		if (num_roots > 2) // except if that's the only two islands
		{
			for (int y=0;y<map.height;y++)
			for (int x=0;x<map.width;x++)
			{
				gamemap::island& here = map.map[y][x];
				uint16_t flags = possibilities[y*100+x];
				uint16_t newflags = flags;
				
				//keep the pointless flags&0x1234 checks, here.bridgelen[0] is undefined if the relevant flag isn't set
				//this isn't a pure performance optimization - the isolation rule processor ignores pop-1 islands,
				//  so without this, it could claim the map '1111' is solvable
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
//printf("%.3i(%.3i) %.4X->%.4X\n",y*100+x,here.population,flags,newflags);
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
		
		if (op==op_hint && accumulator) return false;
		
		if (op == op_hint || op == op_difficulty)
		{
			int maxdepth = 1;
		again: ;
			int ret = solve_rec(0, maxdepth);
			if (ret == 0) return false;
			if (ret == -1) { maxdepth++; goto again; }
			maxdepth--; // needs at least depth 1, or it won't notice that it's finished. this should not contribute to difficulty
			add_difficulty(100*maxdepth*maxdepth);
#ifndef STDOUT_ERROR
if(maxdepth>=2)puts("#######"),puts(map.serialize()),puts("#######");
#endif
		}
		else
		{
			if (solve_rec(0, 0) <= 0) return false;
		}
		
		if (op==op_hint || op==op_difficulty) return false; // return value for these operations is irrelevant
		
		assign();
		return true;
	}
	
	void assign()
	{
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
		
#ifndef STDOUT_ERROR
		if (!map.finished())
		{
			puts("ERROR: solver returned bad solution");
			puts(map.serialize());
#ifdef ARLIB_TEST
			assert(false);
#else
			abort();
#endif
		}
#endif
	}

void verify(bool strict=true)
{
	for (int y=0;y<map.height-1;y++)
	for (int x=0;x<map.width-1;x++)
	{
		uint16_t flags = possibilities[y*100+x];
		uint16_t flagsr = possibilities[y*100+x+1];
		uint16_t flagsd = possibilities[y*100+x+100];
		if (((flagsr>>2)^flags)&0x1111) abort();
		if (((flagsd<<2)^flags)&0x8888) abort();
		
		if (strict && map.map[y][x].population<0 && (flags^(flags>>2))&0x3333) abort();
	}
}

#ifndef STDOUT_ERROR
void print()
{
	for (int y=0;y<map.height;y++)
	for (int x=0;x<map.width;x++)
	{
		uint16_t flags = possibilities[y*100+x];
		if(y&&!x)puts("");
		printf("%.4X ",flags);
	}
	puts("");
}
#endif
};
}

bool gamemap::solve() { solver<op_solve> s(*this); return s.solve(); }
bool gamemap::solve_another() { solver<op_another> s(*this); return s.solve(); }
//void gamemap::hint() { solver<op_hint> s(*this); s.solve(); return s.accumulator; }
uint32_t gamemap::difficulty() { solver<op_difficulty> s(*this); s.solve(); return s.accumulator; }
uint32_t gamemap::solve_difficulty() { solver<op_difficulty> s(*this); if (s.solve()) s.assign(); return s.accumulator; }

#ifdef ARLIB_TEST
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
	assert(m.solve());
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
	assert(!m.solve_another());
}

static void test_unsolv(cstring map)
{
	gamemap m;
	m.init(map.c_str());
	assert(!m.solve());
}

static void test_multi(cstring map)
{
	gamemap m;
	m.init(map.c_str());
	assert(m.solve());
	assert(m.finished());
	assert(m.solve_another());
	assert(m.finished());
}

test("solver", "gamemap", "solver")
{
	//many of these no longer test anything useful, but they're kept anyways
	//for test_one, the left half is a map, and the right half is how many bridges are from the right of this tile
	//not really necessary since it calls .finished()
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
	testcall(test_one( // 2s may not double bind
		"2 2\n" /* */ "1 0\n"
		"222\n" /* */ "110\n"
	));
	testcall(test_one( // another one
		"22\n" /* */ "10\n"
		"22\n" /* */ "10\n"
	));
	testcall(test_one( // this one has caught a few bugs
		"1 2  \n" /* */ "1 0  \n"
		" 1 1 \n" /* */ " 0 0 \n"
		"14441\n" /* */ "11110\n"
		" 1 1 \n" /* */ " 0 0 \n"
		"  2 1\n" /* */ "  1 0\n"
	));
	testcall(test_one( // lots of cycles, though fairly trivial
		"4    4 \n" /* */ "2    0 \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"    682\n" /* */ "    220\n"
		" 282   \n" /* */ " 220   \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"4    4 \n" /* */ "2    0 \n"
	));
	testcall(test_one(
		"2  2\n" /* */ "1  0\n"
		" 22 \n" /* */ " 10 \n"
		"  33\n" /* */ "  10\n"
		" 22 \n" /* */ " 10 \n"
		"2  2\n" /* */ "1  0\n"
	));
	testcall(test_one( // these loops made sense a while ago
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
	testcall(test_one( // all islands must be connected (the no-double-binding-2s rule doesn't solve this one)
		" 1 \n" /* */ " 0 \n"
		" 32\n" /* */ " 10\n"
		"2 1\n" /* */ "0 0\n"
		"32 \n" /* */ "10 \n"
	));
	//from the game generator
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
	//requires guessing
	//(extra 1s to avoid the 2s-don't-double-bind rule)
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
	//a few failed attempts at mandatory guesses
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
	
	testcall(test_one( // solver tried connecting a few impossible bridges in this one
		" 42\n" /* */ " 20\n"
		"231\n" /* */ "000\n"
		"442\n" /* */ "210\n"
	));
	testcall(test_one( // crashed after making an incorrect assumption
		"222\n" /* */ "110\n"
		"3 4\n" /* */ "1 0\n"
		"212\n" /* */ "100\n"
	));
	testcall(test_one( // didn't catch a 'no, that's impossible' from set_state on an ocean tile
		"2 21\n" /* */ "1 00\n"
		"32 3\n" /* */ "20 0\n"
		"  23\n" /* */ "  10\n"
	));
	testcall(test_one( // made a silly assumption and violated a population limit on this map
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
	
	//'are they joined' walk went 'A B C D B A' before noticing it's a loop, then couldn't follow the path back to B
	testcall(test_multi(
		"331  \n"
		"22  2\n"
		"   13\n"
		"  232\n"
	));
	//I don't remember what this one caught, probably something assumption-related again
	testcall(test_multi(
		"3322\n" /* */ "3322\n"
		"3   \n" /* */ "3   \n"
		"353 \n" /* */ "353 \n"
		"25 3\n" /* */ "25 3\n"
	));
	//if an assumption is false, and the opposite (plus fill_simple) satisfies all populations,
	// it that's a valid solution, even if it's disjoint
	testcall(test_multi(
		"2 12 \n"
		"4  52\n"
		"2  2 \n"
		"  252\n"
		"   1 \n"
	));
	
	//large islands
	testcall(test_one(
		"v<\n" /* */ "30\n"
		"0<\n" /* */ "30\n"
	));
	//the no-double-binding-2s doesn't like large islands
	testcall(test_one(
		"v  \n" /* */ "0  \n"
		"2 2\n" /* */ "2 0\n"
		"  ^\n" /* */ "  0\n"
	));
	testcall(test_one(
		"1< \n" /* */ "30 \n"
		"   \n" /* */ "   \n"
		" >1\n" /* */ " 30\n"
	));
	testcall(test_one(
		"v4\n" /* */ "20\n"
		"4^\n" /* */ "20\n"
	));
	testcall(test_unsolv(
		"v2\n" /* */ "20\n"
		"4^\n" /* */ "20\n"
	));
	testcall(test_multi(
		"v2\n"
		"2^\n"
	));
	//this map somehow made guesser engine place a guess on an ocean tile,
	// making it allow different flags up vs down from an ocean, then explode horribly
	testcall(test_multi(
		"44<  3<<1\n"
		"59<< 7< 5\n"
		"^  2 5  7\n"
		"4 5 >A< ^\n"
		"v    3 5<\n"
		"37< 5^  3\n"
		"^8^>^ 6 1\n"
		"3^   4< ^\n"
		"^^  3  2 \n"
	));
	//another solver crash (probably due to solve_another with invalid solution as input, which isn't allowed anyways)
	testcall(test_multi(
		"         \n"
		"         \n"
		" 3<      \n"
		" ^       \n"
		"    2<<  \n"
		" 5<      \n"
		" ^  4 3  \n"
		"        1\n"
		"    2 2 ^\n"
	));
	
	//castles
	testcall(test_one(
		"g<  \n" /* */ "30  \n"
		"^^ 2\n" /* */ "32 0\n"
	));
	testcall(test_one( // different-color castles may not be connected
		"b< y<\n" /* */ "30 30\n"
		"^^ ^^\n" /* */ "30 30\n"
	));
	testcall(test_one( // same-color castles must be connected
		"r< 2  2 b<\n" /* */ "31 0  2 30\n"
		"^^      ^^\n" /* */ "30      30\n"
		"   r<     \n" /* */ "   30     \n"
		"   ^^     \n" /* */ "   30     \n"
	));
	testcall(test_one( // same-color castles must be connected, different may not
		"r< 2 2 b<\n" /* */ "32 0 2 30\n"
		"^^     ^^\n" /* */ "30     30\n"
	));
	testcall(test_one( // like above, repeated a few times to ensure an_island-based optimizations don't do anything strange
		"r< 2 2 b<\n" /* */ "32 0 2 30\n"
		"^^     ^^\n" /* */ "30     30\n"
		"2#######2\n" /* */ "0       0\n"
		"r< 2 2 b<\n" /* */ "32 0 2 30\n"
		"^^     ^^\n" /* */ "30     30\n"
		"2#######2\n" /* */ "0       0\n"
		"r< 2 2 b<\n" /* */ "32 0 2 30\n"
		"^^     ^^\n" /* */ "30     30\n"
		"2#######2\n" /* */ "0       0\n"
		"r< 2 2 b<\n" /* */ "32 0 2 30\n"
		"^^     ^^\n" /* */ "30     30\n"
	));
	testcall(test_multi( // 8 different solutions to a 2x5 map
		"b< b<\n"
		"^^ ^^\n"
	));
	testcall(test_multi( // starts at green, follows the path to the blue, joins it,
		"2 b<\n"           // notices there's only 1 path from there, enforces it,
		"  ^<\n"           // notices the castles are joined, and deems it unsolvable
		"g<  \n"
		"^<  \n"
	));
	testcall(test_multi( // another missed set_state return value
		"   y< 3 v\n"
		"   ^<  4<\n"
		"g<       \n"
		"^<  4< v \n"
		"       5<\n"
		"1  6< 2  \n"
		"^    2 5<\n"
		"   b< 4 2\n"
		"   ^<    \n"
	));
	testcall(test_multi( // another missed set_state return value
		" 4<3  3<1\n"
		"1 ^ r<  5\n"
		"    ^<b<^\n"
		"48<<  ^< \n"
		"6<<  r<  \n"
		"b<  2^< 3\n"
		"^<  ^ r< \n"
		"  3b<1^< \n"
		"  1^<   2\n"
	));
	testcall(test_unsolv( // different-color castles may not be connected
		"4 g<\n"
		"  ^^\n"
		"r<  \n"
		"^^  \n"
	));
	testcall(test_unsolv( // falsely reported solvable if an_island[0] points to a corner and it walks from there
		"2  2\n"
		" r< \n"
		" ^^ \n"
		"2  2\n"
	));
	testcall(test_unsolv( // same-color castles must be connected
		"g<b<\n"
		"^^^^\n"
		"b<g<\n"
		"^^^^\n"
	));
	
	//this map requires a solver depth of 27 and takes 39 seconds on Valgrind
	if(0)
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
	//this one requires a solver depth of 35
	if(0)
	testcall(test_multi(
		"335 5  34 4243342\n"
		"31   3 3 26   543\n"
		"5231 4  7 33 2 45\n"
		"44  5   5     663\n"
		"34456442 21133  1\n"
		"    32 1 34 55  2\n"
		" 3  3 6 5   45  2\n"
		" 45  2 2 3   3   \n"
		"    44       1   \n"
		"     3 4 5   3   \n"
		" 3    35 43 2    \n"
		" 2  4512    1    \n"
		"4 55    5    4   \n"
		"43   33 4    3   \n"
		"   21253 21 31 34\n"
		"2 25 5 135  6 62 \n"
		"25   45     4 3 3\n"
	));
}

test("difficulty", "generator", "")
{
return;
	assert_eq((cstring)game_maps[0],
	          "  2  \n"
	          "     \n"
	          "2 7 1\n"
	          "     \n"
	          "  2  \n");
	
	uint32_t diff[30];
	puts("");
	for (int i=0;i<30;i++)
	{
		gamemap m;
		m.init(game_maps[i]);
		m.solve();
		diff[i] = m.difficulty();
		
		printf("[%i]=%i\n",i,diff[i]);
	}
}
#endif
