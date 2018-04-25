#include "game.h"
#include <math.h>

void gamemap::init(const char * inmap)
{
	width = 0;
	while (inmap[width] != '\n') width++;
	height = 0;
	while (inmap[(width+1) * height] != '\0') height++;
	
	numislands = 0;
	
	has_castles = false;
	an_island[0] = -1; // this value is used if there are zero islands
	an_island[1] = -1;
	an_island[2] = -1;
	an_island[3] = -1;
	an_island[4] = -1;
	
	for (int y=0;y<height;y++)
	for (int x=0;x<width;x++)
	{
		island& here = map[y][x];
		
		int rootpos = y*100+x;
		char mapchar = inmap[y*(width+1) + x];
		
		while (mapchar == '<' || mapchar == '>' || mapchar == 'v' || mapchar == '^')
		{
			if (mapchar == '>') rootpos += 1;
			if (mapchar == '^') rootpos -= 100;
			if (mapchar == '<') rootpos -= 1;
			if (mapchar == 'v') rootpos += 100;
			
			int y2 = rootpos/100;
			int x2 = rootpos%100;
			mapchar = inmap[y2*(width+1) + x2];
		}
		
		if (mapchar == ' ') here.population = -1;
		else if (mapchar == '#') here.population = -2;
		else if (mapchar == 'r') here.population = 80;
		else if (mapchar == 'b') here.population = 81;
		else if (mapchar == 'y') here.population = 82;
		else if (mapchar == 'g') here.population = 83;
		else if (mapchar >= '0' && mapchar <= '9') here.population = mapchar-'0';
		else here.population = mapchar-'A'+10;
		
		here.totbridges = 0;
		here.bridges[0] = 0;
		here.bridges[1] = 0;
		here.bridges[2] = 0;
		here.bridges[3] = 0;
		
		here.bridgelen[0] = -1;
		here.bridgelen[1] = -1;
		here.bridgelen[2] = -1;
		here.bridgelen[3] = -1;
		
		here.rootnode = rootpos;
		
		if (here.population >= 0)
		{
			numislands++;
			if (rootpos == y*100+x)
			{
				an_island[0] = rootpos;
				if (here.population >= 80)
				{
					an_island[here.population-80+1] = rootpos;
					has_castles = true;
				}
			}
			
			for (int x2=x-1;x2>=0;x2--)
			{
				if (map[y][x2].population != -1)
				{
					if (map[y][x2].population == -2) break;
					
					here.bridgelen[2] = x-x2;
					map[y][x2].bridgelen[0] = x-x2;
					for (int x3=x2+1;x3<x;x3++)
					{
						map[y][x3].bridgelen[0] = x-x3;
						map[y][x3].bridgelen[2] = x3-x2;
					}
					
					if (map[y][x2].rootnode == here.rootnode && x-x2==1)
					{
						here.bridges[2] = 3;
						map[y][x2].bridges[0] = 3;
					}
					
					break;
				}
			}
			for (int y2=y-1;y2>=0;y2--)
			{
				if (map[y2][x].population != -1)
				{
					if (map[y2][x].population == -2) break;
					
					here.bridgelen[1] = y-y2;
					map[y2][x].bridgelen[3] = y-y2;
					for (int y3=y2+1;y3<y;y3++)
					{
						map[y3][x].bridgelen[1] = y3-y2;
						map[y3][x].bridgelen[3] = y-y3;
					}
					
					if (map[y2][x].rootnode == here.rootnode && y-y2==1)
					{
						here.bridges[1] = 3;
						map[y2][x].bridges[3] = 3;
					}
					
					break;
				}
			}
		}
	}
}

//only allows dir=0 or 3
void gamemap::toggle(int x, int y, int dir)
{
	if (dir == 0)
	{
		island& here = map[y][x];
		if (here.bridges[0] == 3) return;
		
		int newbridges = (here.bridges[0]+1)%3;
		int diff = newbridges - here.bridges[0];
		for (int xx=0;xx<here.bridgelen[0];xx++)
		{
			map[y][x+xx  ].bridges[0] = newbridges;
			map[y][x+xx+1].bridges[2] = newbridges;
			get(map[y][x+xx  ].rootnode).totbridges += diff;
			get(map[y][x+xx+1].rootnode).totbridges += diff;
		}
	}
	else
	{
		island& here = map[y][x];
		if (here.bridges[3] == 3) return;
		
		int newbridges = (here.bridges[3]+1)%3;
		int diff = newbridges - here.bridges[3];
		for (int yy=0;yy<here.bridgelen[3];yy++)
		{
			map[y+yy  ][x].bridges[3] = newbridges;
			map[y+yy+1][x].bridges[1] = newbridges;
			get(map[y+yy  ][x].rootnode).totbridges += diff;
			get(map[y+yy+1][x].rootnode).totbridges += diff;
		}
	}
}

void gamemap::reset()
{
	for (int y=0;y<height;y++)
	for (int x=0;x<width;x++)
	{
		island& here = map[y][x];
		
		here.totbridges = 0;
		if (here.bridges[0] <= 2) here.bridges[0] = 0;
		if (here.bridges[1] <= 2) here.bridges[1] = 0;
		if (here.bridges[2] <= 2) here.bridges[2] = 0;
		if (here.bridges[3] <= 2) here.bridges[3] = 0;
	}
}

//The map is finished if
//(1) the sum of bridgelen[y][x][0..3] equals population[y][x] for all non-ocean tiles
//(2) no bridges cross each other
//(3) all tiles are reachable from each other by following bridges
bool gamemap::finished()
{
	for (int y=0;y<height;y++)
	for (int x=0;x<width;x++)
	{
		island& here = map[y][x];
		
		//if not ocean or castle, and wrong number of bridges,
		if (here.population >= 0 && here.population < 80 &&
			here.rootnode == y*100+x && here.totbridges != here.population)
		{
			return false; // then map isn't solved
		}
		
		//if ocean and has both horizontal and vertical bridges,
		if (here.population < 0 &&
			here.bridges[0] != 0 &&
			here.bridges[3] != 0)
		{
			return false; // then map isn't solved
		}
	}
	
	//all islands are satisfied and there's no crossing - test if everything is connected
	int foundislands = 0;
	memset(visited, 0, sizeof(visited));
	
	//TODO: >=has_castles once tests are confirmed to catch it
	for (int castletype=4;castletype>=0;castletype--)
	{
		if (an_island[castletype] == -1) continue;
		
		int ntowalk = 0;
		towalk[ntowalk++] = an_island[castletype];
		while (ntowalk)
		{
			int ix = towalk[--ntowalk];
			if (visited[ix]) continue;
			visited[ix] = true;
			
			island& here = map[ix/100][ix%100];
			foundislands++;
			
			//castle connected to another castle? failure
			if (here.population >= 80 && here.population != 80+castletype-1)
				return false;
			
			if (here.bridges[0])
				towalk[ntowalk++] = ix + here.bridgelen[0];
			if (here.bridges[1])
				towalk[ntowalk++] = ix - here.bridgelen[1]*100;
			if (here.bridges[2])
				towalk[ntowalk++] = ix - here.bridgelen[2];
			if (here.bridges[3])
				towalk[ntowalk++] = ix + here.bridgelen[3]*100;
		}
	}
	
	return (foundislands == numislands);
}
