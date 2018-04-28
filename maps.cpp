//map rules:
//- the map must be rectangular, and 100 tiles or less in both directions
//- large islands must eventually terminate, no loops allowed
//- large islands must end with an island tile, no oceans or reefs
//- for performance reasons, large islands should use the shortest possible path to the roots
//- if 10 bridges is possible, the root must be joined to the tile below or right; it may not be on the bottom-right edge
//    for simplicity, better to completely avoid large islands with roots at bottom-right
//- castles must be joined with the tile below, right, AND below-right
//- placing zero bridges must not be a solution
//- the above rules are not properly enforced, violations may yield arbitrary absurd results

extern const char * const game_maps[] = {
// test maps

//"          \n"
//" 4< 2  1  \n"
//"    ^ 2#2 \n"
//" >v    1  \n"
//" 8<       \n"
//"     >>>v \n"
//"  v  ^<<A \n"
//" >A< ^^ v \n"
//"  ^  ^^<< \n"
//"          \n",


//">>>A<<<<\n"
//"1<2<3<4<\n"
//">>>B<<<<\n"
//"v1v12345\n"
//"v^v     \n"
//"A2B     \n"
//"^^^     \n"
//"^3^     \n"
//"^^^     \n",


//*


		"33\n"
		"22\n",

		"1    1\n"
		"3    3\n"
		" 133  \n"
		"3  31 \n"
		"113  3\n",
		
		"22   2\n"
		" 122  \n"
		"2  213\n"
		"1 2  2\n",
		
		" 1 \n"
		" 32\n"
		"2 1\n"
		"32 \n",
		
		"2  1\n"
		"3233\n"
		" 131\n"
		"1 2 \n",
		
		"1232\n"
		"2 11\n"
		"5543\n"
		"23 2\n",
		
		"1332\n"
		"13 3\n"
		"21  \n"
		"3 42\n",
		
		"3431\n"
		" 112\n"
		"32 3\n"
		"2  1\n",


"r<        \n" // contains castles, but not a good introduction to them
"^^ 2 3 1  \n" // castles need to show up after reef and large islands
"  2 1 1 2 \n"
" 4 1 4 1  \n"
"  2 3 2 5 \n"
" 3 1 3 2  \n"
"  2 5 2 g<\n"
"        >^\n",


//*/





	"  2  \n" // 1
	"     \n"
	"2 7 1\n"
	"     \n"
	"  2  \n",
	
	" 1  3\n" // 2
	"     \n"
	"2  1 \n"
	"     \n"
	"2 2 3\n",
	
	"4 5 3 3\n" // 3
	"       \n"
	"      4\n"
	"       \n"
	"      3\n"
	"  2    \n"
	"2   2 2\n",
	
	"2   5 3\n" // 4
	"  2    \n"
	"       \n"
	"2      \n"
	"       \n"
	"    2  \n"
	"3 4   3\n",
	
	" 2    3\n" // 5
	"       \n"
	" 1  2  \n"
	"       \n"
	"2 2 3  \n"
	"       \n"
	"3     4\n",
	
	" 1    2\n" // 6
	"3   1  \n"
	" 3    4\n"
	"       \n"
	" 1  2  \n"
	"       \n"
	"3   4 2\n",
	
	"4     4\n" // 7
	" 2     \n"
	"  1  3 \n"
	" 3 2   \n"
	"       \n"
	"3  4 4 \n"
	"  1 3 4\n",
	
	" 1    3\n" // 8
	"3    3 \n"
	"       \n"
	"5  4   \n"
	"      3\n"
	" 2 5 2 \n"
	"3     2\n",
	
	"4 3 3 3\n" // 9
	" 2   4 \n"
	"3  3  3\n"
	"       \n"
	"2  8 4 \n"
	"    1 3\n"
	" 1 4  1\n",
	
	"1   4 2\n" // 10
	"     1 \n"
	"  21   \n"
	"1    45\n"
	" 14 52 \n"
	"5     4\n"
	"2      \n",
	
	" 3    3\n" // 11
	"       \n"
	" 1  2 5\n"
	"       \n"
	"1     4\n"
	"       \n"
	"1     2\n",
	
	"1  4 1 \n" // 12
	"    1 2\n"
	"2  6 2 \n"
	"       \n"
	" 1 3  3\n"
	"2 2  1 \n"
	" 1 2  2\n",
	
	" 1 2  2\n" // 13
	"1   1  \n"
	" 3 5   \n"
	"3 3 3 3\n"
	"   3 2 \n"
	"  2 1 1\n"
	"2  3 3 \n",
	
	"  1 2 1\n" // 14
	"3  3   \n"
	"  1 3 4\n"
	"   2   \n"
	"4 5 4 5\n"
	"   1   \n"
	"2 4 2 1\n",
	
	"1 4 1  \n" // 15
	"   3  4\n"
	"2 3    \n"
	" 1 5 2 \n"
	"    1 4\n"
	"2  4 2 \n"
	"  1 2 2\n",
	
	"2 3 2  \n" // 16
	" 1 2  2\n"
	"3 2 2  \n"
	" 1 3   \n"
	"    5 3\n"
	"2  3   \n"
	"  1 4 1\n",
	
	" 1 2  2\n" // 17
	"2 4 2  \n"
	"   2   \n"
	"4 5 4 3\n"
	"   3   \n"
	"  1 4 3\n"
	"2  3 1 \n",
	
	" 2  3 2\n" // 18
	"1      \n"
	" 3 2 1 \n"
	"3   3 3\n"
	"  2  2 \n"
	"    1 2\n"
	"2 4  2 \n",
	
	"2  2  2\n" // 19
	"       \n"
	"4 2 1 4\n"
	" 1 4 3 \n"
	"3 2 1 2\n"
	" 2   3 \n"
	"2  2  2\n",
	
	"2 3 3 2\n" // 20
	" 1 3 1 \n"
	"3 2 1 3\n"
	"       \n"
	"2  3  4\n"
	"       \n"
	"2 4 3 2\n",
	
	" 1  2 3 3\n" // 21
	"1    1   \n"
	" 4  5  1 \n"
	"2 1   3 3\n"
	" 1   2 3 \n"
	"4 6 4 3 2\n"
	"     2 3 \n"
	"    1 1 1\n"
	"2 4  3 2 \n",
	
	"1 2  2  2\n" // 22
	"         \n"
	"  3 5 3 4\n"
	"3  1 1   \n"
	" 1  4 1  \n"
	"3 1  3  4\n"
	"    1 1  \n"
	" 1   2   \n"
	"2  2  3 2\n",
	
	" 1  3 4 2\n" // 23
	"3  2     \n"
	"  2 3    \n"
	" 1 4  8 4\n"
	"         \n"
	"3  4  6 3\n"
	"    1    \n"
	" 1 2  4 3\n"
	"2 2 3  1 \n",
	
	" 1  1 4 2\n" // 24
	"1        \n"
	" 3 3  6 4\n"
	"2    1 1 \n"
	" 1  1 1  \n"
	"3  3    3\n"
	"  1  2 3 \n"
	"   1    2\n"
	"2 3 3  2 \n",
	
	"2 3 2 2 1\n" // 25
	"         \n"
	"2  2  5 4\n"
	"  2    1 \n"
	"2  3 2  2\n"
	"    1 3  \n"
	"2 3  2  1\n"
	" 1 2  2  \n"
	"2 2 2  2 \n",
	
	" 1 2  2 1\n" // 26
	"1 4  2   \n"
	"      4 5\n"
	"2 6 2    \n"
	"   2 1   \n"
	"    2 5 4\n"
	"3 6      \n"
	"   3  5 3\n"
	"1 4  2 1 \n",
	
	"2 2 2 2 2\n" // 27
	" 1 4 3 3 \n"
	"        2\n"
	"2  5 7 4 \n"
	"         \n"
	" 1 4 6  3\n"
	"2 2 1    \n"
	"   1 4  3\n"
	"1 3 3 1  \n",
	
	"2 3  1  1\n" // 28
	"   1     \n"
	"  2   1 3\n"
	"3  4 4 2 \n"
	"  2 2 2  \n"
	"   2 1  2\n"
	"2 2 2 4  \n"
	"       1 \n"
	"1  3  3 2\n",
	
	"2 3 2 2 3\n" // 29
	"         \n"
	"4 6 6 6 6\n"
	" 1 2 2   \n"
	"2 3 1 4 4\n"
	"   2 4 1 \n"
	"2 3 1 2 1\n"
	"         \n"
	"2 2  3 2 \n",
	
	" 2 3 3  3\n" // 30
	"3 2      \n"
	" 2 5 5 3 \n"
	"3 2     2\n"
	" 3 5 5 6 \n"
	"2 1     2\n"
	" 2 6 7 5 \n"
	"        1\n"
	"2  2 3 2 \n",



//TODO: number these
"2 2   2\n" // small, but very hard
"  1    \n"
"  2 2  \n"
"       \n"
"2   213\n"
"       \n"
"1 2   2\n",


"r<        \n" // contains castles, but not a good introduction to them
"^^ 2 3 1  \n" // castles need to show up after reef and large islands
"  2 1 1 2 \n"
" 4 1 4 1  \n"
"  2 3 2 5 \n"
" 3 1 3 2  \n"
"  2 5 2 g<\n"
"        >^\n",


//hard but playable
"11      \n"
"254 4 22\n"
"      1 \n"
" 2   153\n"
"        \n"
"   26 5 \n"
"    1 3 \n"
" 34   1 \n",


};
