#include "game.h"

static string generate_blank(int width, int height)
{
	string line = "";
	for (int i=0;i<width;i++) line+=' ';
	line += "\n";
	
	string ret = "";
	for (int i=0;i<height;i++) ret += line;
	return ret;
}

static void fill_until(string& ret, int width, int y, int x, int dir, char replacement, int add = 0)
{
	int ix = (width+1)*y + x;
	int offsets[4] = { 1, -(width+1), -1, width+1 };
	int offset = offsets[dir];
	
	while (true)
	{
		ix += offset;
		if (ix < 0 || ix >= (int)ret.length() || ret[ix] == '\n') break;
		if (isdigit(ret[ix]))
		{
			ret[ix] += add;
			break;
		}
		if (replacement!='#' && ret[ix]!=' ') break;
		ret[ix] = replacement;
	}
}

//returns false if that tile is unusable
static bool add_island(string& ret, int width, int y, int x)
{
	int ix = (width+1)*y + x;
	bool first = (ret[ix] == ' ');
	
	int offsets[4] = { 1, -(width+1), -1, width+1 };
	
	int allowed = 0;
	
	for (int dir=0;dir<4;dir++)
	{
		int ix2 = ix;
		int offset = offsets[dir];
		while (true)
		{
			ix2 += offset;
			if (ix2 < 0 || (unsigned)ix2 >= ret.length() || ret[ix2] == '\n' || ret[ix2] == '#')
			{
				break;
			}
			if (isdigit(ret[ix2]))
			{
				allowed |= 1<<dir;
				break;
			}
		}
	}
	
	char newtile = '0';
	if (allowed == 0 && !first)
	{
		ret[ix] = ' ';
		return false;
	}
	if (allowed != 0)
	{
		bool any = false;
		do {
			for (int dir=0;dir<4;dir++)
			{
				if ((allowed & (1<<dir)) && rand()%4 == 0)
				{
					int nbridge = (rand()%2 ? 2 : 1);
					fill_until(ret, width, y, x, dir, '#', nbridge);
					newtile += nbridge;
					any = true;
				}
			}
		} while (!any);
	}
	ret[ix] = newtile;
	
	fill_until(ret, width, y, x, 0, '.');
	fill_until(ret, width, y, x, 1, '.');
	fill_until(ret, width, y, x, 2, '.');
	fill_until(ret, width, y, x, 3, '.');
	
	return true;
}

string map_generate(int width, int height, int islands)
{
	string ret = generate_blank(width, height);
	
	//possible values in the string:
	//space - empty
	//0-8 - number of bridges
	//# - the intended solution has a bridge here
	//. - can place island here; possible bridges may exist in all four directions
	//\n - line separator
	
	add_island(ret, width, rand()%height, rand()%width);
	islands--;
	
	while (islands)
	{
//puts(ret);
		int nslots = 0;
		int nix = 0;
		for (unsigned ix=0;ix<ret.length();ix++)
		{
			if (ret[ix] == '.')
			{
				nslots++;
				if (rand() % nslots == 0) nix = ix;
			}
		}
		
		if (nslots == 0) break;
		
		bool ok = add_island(ret, width, nix/(width+1), nix%(width+1));
		if (ok) islands--;
	}
	
	for (unsigned ix=0;ix<ret.length();ix++)
	{
		if (ret[ix] == '.' || ret[ix] == '#') ret[ix] = ' ';
	}
	
	return ret;
}

test("generator","solver","generator")
{
	for (int i=0;true;i++)
	{
		if (i == 100) assert(!"game generator needs to contain loops in its intended solutions");
		string game = map_generate(2, 2, 4);
		if (game == "22\n22\n") break;
		if (game == "44\n44\n") break;
		if (game == "33\n44\n") break;
		if (game == "44\n33\n") break;
		if (game == "34\n34\n") break;
		if (game == "43\n43\n") break;
	}
}
