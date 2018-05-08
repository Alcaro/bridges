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




/*


"14 52 2\n"
"1 4422 \n"
"356   5\n"
"  4  3 \n"
"   13  \n"
"  2 3  \n"
"25   54\n",





"1223 225 2\n" // 220
"45 544  2 \n"
"   3      \n"
"  43  3   \n"
"   43   33\n"
" 24  674 5\n"
"45 33  22 \n"
"    1   4 \n"
"1  344374 \n"
" 44    4 3\n",

" 21       \n" // 416
"1         \n"
"364 4  4  \n"
"   2   2  \n"
"   4      \n"
"          \n"
" 3       1\n"
"32244    2\n"
"          \n"
"23  1     \n",

"1 11  4443\n" // 216
"2 52    4 \n"
"    1 2 33\n"
"4 7    4  \n"
"     1 2 3\n"
"53  3 32  \n"
"31 48   63\n"
"2    22   \n"
"   3564  1\n"
"2 4     3 \n",

"        21\n" // 432
"212  22 5 \n"
"5 3214 2  \n"
"          \n"
"   1      \n"
" 1 4 364  \n"
"          \n"
"   2  2   \n"
"5       6 \n"
"124     52\n",

"11 4  23 3\n" // 209
"5  8   664\n"
"3  5 2  4 \n"
"4     3  2\n"
"3 23 4  3 \n"
" 23  2    \n"
" 4 52    2\n"
" 32      3\n"
"4454  54 2\n"
"3 6  44 2 \n",

"     3   2\n" // 206
"   4434 53\n"
" 21  3    \n"
"  34   1  \n"
"         1\n"
" 3   2  2 \n"
"   2    1 \n"
" 36 5 63 1\n"
"  4 1     \n"
"  213 4   \n",
   
"22 5 6 422\n" // 214
"4 2 2 2  3\n"
"576   54  \n"
"      454 \n"
"    35   4\n"
"3   2 123 \n"
"4 4 456 6 \n"
"          \n"
"653 2 3 3 \n"
"4  5 3   2\n",

" 12 2  5 2\n" // 221
" 2      2 \n"
"   4 32   \n"
"   5 22   \n"
"1376   76 \n"
"2 5 441 2 \n"
"3 4  11223\n"
"  358  4 3\n"
"    5  3  \n"
"2 5454 4 1\n",





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
	
	"3   4 2\n" // 6, but flipped upside down so the hint box doesn't cover the relevant islands
	"       \n"
	" 1  2  \n"
	"       \n"
	" 3    4\n"
	"3   1  \n"
	" 1    2\n",
	
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

//these took me several attempts to solve
"2< # 2 \n"
"^ 3 2 2\n"
"  ^    \n"
"3 ^ A<<\n"
" 2     \n"
"3 1   2\n"
"^4  2  \n",

//TODO: double check that the upper one (with reefs moved) is equally solvable to the lower (original)
"2< 3< 3\n"
" >6< 2 \n"
"2   3 5\n"
" 4 2 # \n"
" ^4 5 4\n"
"2      \n"
"^<# 4 3\n",

"2< 3< 3\n"
" >6< 2 \n"
"2   3 5\n"
" 4 2   \n"
" ^4 5 4\n"
"2  #   \n"
"^< #4 3\n",

//except maximizing difficulty isn't fun, it just gets tedious after a while. better stick to interesting patterns instead
//for example:
//- one where the correct solution is only vertical bridges,
//    but the islands' varying sizes still make it nontrivial to find where the bridges go
//    (horizontal should be possible and hard to rule out, of course)
//- one with a long bridge crossing the entire map
//- one very similar to above, but that bridge is now incorrect
//- something that turns into a spiral
//- various other forms of symmetry
//- mainlands (will always be castles, will never have population)

};
